//
// Created by SXT on 2022/9/1.
//

#include "DetailAppend.h"

static int EndInputChannelIndex[MAX_AG] = {0}; // 最终该AG结束在的input_channel_index
static int EndIndexInInputChannel[MAX_AG] = {0};
static int OffsetForFC[MAX_AG] = {0};
static int SourceIndex[MAX_AG] = {0};

void DetailAppend::AppendDetail(Json::Value &DNNInfo)
{
    instruction_group_num = static_cast<int>(DNNInfo["6_core_instruction_ir"].size());
    core_num = static_cast<int>(DNNInfo["6_physical_core_AG_map"]["core_list"].size());
    clock_t start_time = clock();
    PreProcess(DNNInfo);
    clock_t end_time_1 = clock();
    PrepareForInput(DNNInfo);
    clock_t end_time_2 = clock();
    std::cout << double(end_time_1 - start_time) / CLOCKS_PER_SEC << "s" << std::endl;
    std::cout << double(end_time_2 - start_time) / CLOCKS_PER_SEC << "s" << std::endl;
}

void DetailAppend::PreProcess(Json::Value &DNNInfo)
{
    int AG_num = DNNInfo["2_resource_info"]["AGs"].asInt();
    SourceIndex[0] = 0;
    for (int i = 1; i < AG_num; ++i)
    {
        int cur_core = DNNInfo["3_hierarchy_map"]["whole"][i][0]["vcore_index"].asInt();
        int cur_node = DNNInfo["3_hierarchy_map"]["whole"][i][0]["node_index"].asInt();
        int cur_replication = DNNInfo["3_hierarchy_map"]["whole"][i][0]["replication_index"].asInt();
        int last_core = DNNInfo["3_hierarchy_map"]["whole"][i-1][0]["vcore_index"].asInt();
        int last_node = DNNInfo["3_hierarchy_map"]["whole"][i-1][0]["node_index"].asInt();
        int last_replication = DNNInfo["3_hierarchy_map"]["whole"][i-1][0]["replication_index"].asInt();
        if (cur_node == last_node && cur_replication == last_replication && cur_core == last_core)
        {
            SourceIndex[i] = SourceIndex[i-1];
        }
        else
        {
            SourceIndex[i] = i;
        }
    }
}

void DetailAppend::PrepareForInput(Json::Value &DNNInfo)
{
    for (int i = 0; i < instruction_group_num; ++i)
    {
        Json::Value InstructionGroup = DNNInfo["6_core_instruction_ir"][i];
        for (int j = 0; j < core_num; ++j)
        {
            Json::Value InstructionIrList = InstructionGroup["core_list"][j]["instruction_ir_list"];
            int instruction_ir_num = static_cast<int>(InstructionIrList.size());
            for (int k = 0; k < instruction_ir_num; ++k)
            {
                Json::Value tmpInstruction = InstructionIrList[k];

                int node_index = tmpInstruction["node_index"].asInt();
                int AG_index_in_replication = tmpInstruction["AG_index_in_replication"].asInt();
                int AG_index_in_total = tmpInstruction["AG_index_in_total"].asInt();
                int input_cycle_index = tmpInstruction["input_cycle_index"].asInt();
                int input_element_num = tmpInstruction["input_element_num"].asInt();
                int replication_index = tmpInstruction["replication_index"].asInt();

                Json::Value Node = DNNInfo["node_list"][node_index];

                if (strcmp(Node["operation"].asCString(), "OP_CONV") == 0)
                {
                    // kernel_h * kernel_w
                    int input_channel_size = Node["param"]["input_channel"].asInt();
                    int input_channel_num = 0;
                    int kernel_w = Node["param"]["kernel_w"].asInt();
                    int kernel_h = Node["param"]["kernel_h"].asInt();
                    input_channel_num = kernel_w * kernel_h;
                    int start_input_channel_index = 0;
                    int start_index_in_input_channel = 0;
                    if (AG_index_in_replication != 0)
                    {
                        start_input_channel_index = EndInputChannelIndex[AG_index_in_total-1];
                        start_index_in_input_channel = EndIndexInInputChannel[AG_index_in_total-1];
                    }
                    int rest_input_element_num = input_element_num;
                    int rd_offset = 0;
                    for (int l = start_input_channel_index; l < input_channel_num; ++l)
                    {
                        int no_visited_size = (l == start_input_channel_index) ? (input_channel_size-start_index_in_input_channel) : (input_channel_size); // 注意这里的length和index还有是否+1的
                        int move_vector_size = 0;
                        bool IsBreak = false;
                        if (rest_input_element_num <= no_visited_size)
                        {
                            if (rest_input_element_num != no_visited_size)
                            {
                                EndIndexInInputChannel[AG_index_in_total] = (start_index_in_input_channel+rest_input_element_num)%(input_channel_size);
                                EndInputChannelIndex[AG_index_in_total] = l;
                            }
                            else
                            {
                                EndIndexInInputChannel[AG_index_in_total] = 0;
                                EndInputChannelIndex[AG_index_in_total] = l+1;
                            }
                            move_vector_size = rest_input_element_num;
                            IsBreak = true;
                        }
                        else
                        {
                            rest_input_element_num -= no_visited_size;
                            move_vector_size = no_visited_size;
                        }
                        Json::Value Instruction;
                        Instruction["operation"] = "vm_conv";
                        Instruction["input_cycle_index"] = input_cycle_index;
                        Instruction["node_index"] = node_index;
                        Instruction["replication_index"] = replication_index;
                        Instruction["AG_index_in_total"] = AG_index_in_total;
                        Instruction["AG_index_in_replication"] = AG_index_in_replication;
                        Instruction["element_num"] = move_vector_size;

                        Instruction["source"] = SourceIndex[AG_index_in_total];
                        int rs_offset_w = l % kernel_w;
                        int rs_offset_h = l / kernel_h;
                        Instruction["source_offset_w"] = rs_offset_w;
                        Instruction["source_offset_h"] = rs_offset_h;
                        int rs_offset_plus = (AG_index_in_replication == 0 ? 0 : (l == start_input_channel_index) ? EndIndexInInputChannel[AG_index_in_total-1] : 0);
                        Instruction["source_offset_plus"] = rs_offset_plus;
                        Instruction["destination"] = AG_index_in_total;
                        Instruction["destination_offset"] = rd_offset;
                        rd_offset += move_vector_size;

                        DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(Instruction);
                        if (IsBreak)
                        {
                            break;
                        }
                    }
                    DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(tmpInstruction);
                }
                else if (strcmp(Node["operation"].asCString(), "OP_FC") == 0)
                {
                    // 原始的方式(把前一层的完整输出传递给每一个包括该层权重的核)。但是这输入数据的利用率太低。但是FC层的特殊性，调整一下，改成不要每次都只传需要的部分。
//                    int start_index_in_input_channel = 0;
//                    if (AG_index_in_replication != 0)
//                    {
//                        start_index_in_input_channel = EndIndexInInputChannel[AG_index_in_total-1];
//                    }
//                    int move_vector_size = input_element_num;
//                    Json::Value Instruction;
//                    Instruction["operation"] = "vm_fc";
//                    Instruction["input_cycle_index"] = input_cycle_index;
//                    Instruction["node_index"] = node_index;
//                    Instruction["replication_index"] = replication_index;
//                    Instruction["AG_index_in_total"] = AG_index_in_total;
//                    Instruction["AG_index_in_replication"] = AG_index_in_replication;
//                    Instruction["element_num"] = move_vector_size;
//
//                    Instruction["source"] = SourceIndex[AG_index_in_total];
//                    Instruction["source_offset"] = EndIndexInInputChannel[AG_index_in_total-1];
//                    Instruction["destination"] = AG_index_in_total;
//                    Instruction["destination_offset"] = 0;
//
//                    EndIndexInInputChannel[AG_index_in_total] = start_index_in_input_channel+move_vector_size;
//                    DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(Instruction);
//                    DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(tmpInstruction);


                    // 改进方法
                    int move_vector_size = input_element_num;

                    if(AG_index_in_total == SourceIndex[AG_index_in_total])
                        OffsetForFC[AG_index_in_total] = 0;
                    else
                        OffsetForFC[AG_index_in_total] = OffsetForFC[AG_index_in_total-1] + move_vector_size;
                    Json::Value Instruction;
                    Instruction["operation"] = "vm_fc";
                    Instruction["input_cycle_index"] = input_cycle_index;
                    Instruction["node_index"] = node_index;
                    Instruction["replication_index"] = replication_index;
                    Instruction["AG_index_in_total"] = AG_index_in_total;
                    Instruction["AG_index_in_replication"] = AG_index_in_replication;
                    Instruction["element_num"] = move_vector_size;

                    Instruction["source"] = SourceIndex[AG_index_in_total];
                    Instruction["source_offset"] = OffsetForFC[AG_index_in_total];
                    Instruction["destination"] = AG_index_in_total;
                    Instruction["destination_offset"] = 0;
                    DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(Instruction);

                    DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(tmpInstruction);

                    int start_index_in_input_channel = 0;
                    if (AG_index_in_replication != 0)
                    {
                        start_index_in_input_channel = EndIndexInInputChannel[AG_index_in_total-1];
                    }
                    EndIndexInInputChannel[AG_index_in_total] = start_index_in_input_channel+move_vector_size;
//                    DNNInfo["7_fc_input_info"]["node_list"][node_index]["core_list"][j]["offset"].append(EndIndexInInputChannel[AG_index_in_total-1]);
//                    DNNInfo["7_fc_input_info"]["node_list"][node_index]["core_list"][j]["element_num"].append(move_vector_size);
                }
                else
                {
                    DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(tmpInstruction);
                }
            }
        }
    }
}

void DetailAppend::ShowDetailedInstruction(Json::Value & DNNInfo)
{
    for (int i = 0; i < instruction_group_num; ++i)
    {
        std::cout << "============================== instruction_group " << i << " ==============================" << std::endl;
        for (int j = 0; j < core_num; ++j)
        {
            std::cout << "core " << j << std::endl;
            int instruction_num = DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].size();
            for (int k = 0; k < instruction_num; ++k)
            {
                Json::Value Instruction = DNNInfo["7_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"][k];
                std::string Operation = Instruction["operation"].asCString();
                if (strcmp(Operation.c_str(), "vm_conv") == 0)
                    std::cout << "    [" << Operation << "]"
                              << " input_index:" << Instruction["input_cycle_index"]
                              << " rs:" << Instruction["source"]
//                              << " rs_offset:" << Instruction["source_offset"]
                              << " rs_offset_w:" << Instruction["source_offset_w"]
                              << " rs_offset_h:" << Instruction["source_offset_h"]
                              << " rs_offset_plus:" << Instruction["source_offset_plus"]
                              << " rd:" << Instruction["destination"]
                              << " rd_offset:" << Instruction["destination_offset"]
                              << " element_num:" << Instruction["element_num"]
                              << " N" << Instruction["node_index"]
                              << " R" << Instruction["replication_index"]
                              << " AG" << Instruction["AG_index_in_replication"] << std::endl;
                else if (strcmp(Operation.c_str(), "vm_fc") == 0)
                    std::cout << "    [" << Operation << "]"
                              << " input_index:" << Instruction["input_cycle_index"]
                              << " rs:" << Instruction["source"]
                              << " rs_offset:" << Instruction["source_offset"]
                              << " rd:" << Instruction["destination"]
                              << " rd_offset:" << Instruction["destination_offset"]
                              << " element_num:" << Instruction["element_num"]
                              << " N" << Instruction["node_index"]
                              << " R" << Instruction["replication_index"]
                              << " AG" << Instruction["AG_index_in_replication"] << std::endl;
                else if (strcmp(Operation.c_str(), "RECV") == 0)
                    std::cout << "    [" << Operation << "]"
                              <<" rd:" << Instruction["destination"]
                              <<  " from Core:" << Instruction["from_core"]
                              << " rlength:" << Instruction["relative_length"]
                              << " element_num:" << Instruction["element_num"] << std::endl;
                else if (strcmp(Operation.c_str(), "SEND") == 0)
                    std::cout << "    [" << Operation << "]"
                              << " rs:" << Instruction["source"]
                              << " to Core:" << Instruction["to_core"]
                              << " rlength:" << Instruction["relative_length"]
                              << " element_num:" << Instruction["element_num"] << std::endl;
                else if (strcmp(Operation.c_str(), "WB") == 0)
                    std::cout << "    [" << Operation << "]"
                              << " rs:" << Instruction["source"]
                              << " rep_index:" << Instruction["replication_index"]
                              << " rlength:" << Instruction["relative_length"]
                              << " element_num:" << Instruction["element_num"] << std::endl;
                else if (strcmp(Operation.c_str(), "MVMUL") == 0)
                    std::cout << "    [" << Operation << "]"
                              << " rd:" << Instruction["destination"]
                              << " rd_offset:" << Instruction["offset"]["value"]
                              << " rs:" << Instruction["source"]
                              << " node:" << Instruction["node_index"]
                              << " input:" << Instruction["input_cycle_index"]
                              << " input_element_num:" << Instruction["input_element_num"]
                              << " output_element_num:" << Instruction["output_element_num"] <<  std::endl;
                else if (strcmp(Operation.c_str(), "VADD") == 0)
                    std::cout << "    [" << Operation << "]"
                              << " rs1:" << Instruction["source_1"]
                              << " rs2:" << Instruction["source_2"]
                              << " rd:" << Instruction["destination"]
                              << " offset:" << Instruction["offset"]["value"]
                              << " rlength:" << Instruction["relative_length"]
                              << " element_num:" << Instruction["element_num"] << std::endl;
                else if (strcmp(Operation.c_str(), "ReLU") == 0)
                    std::cout << "    <" << Operation << ">"
                              << " rs:" << Instruction["source"]
                              << " rd:" << Instruction["destination"]
                              << " offset:" << Instruction["offset"]["value"]
                              << " rlength:" << Instruction["relative_length"]
                              << " element_num:" << Instruction["element_num"] << std::endl;
                else if (strcmp(Operation.c_str(), "ELTWISE") == 0)
                    std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                              << " rs1:" << Instruction["source_1"]
                              << " rs2:" << Instruction["source_2"]
                              << " rd:" << Instruction["destination"]
                              << " copy_offset:" << Instruction["copy_offset_flag"]
                              << " element_num:" << Instruction["element_num"]
                              << std::endl;
                else if (strcmp(Operation.c_str(), "CONCAT") == 0)
                    std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                              << " rs:" << Instruction["source"]
                              << " rd:" << Instruction["destination"]
                              << " rs_offset:" << Instruction["rs_offset"]
                              << " rd_offset:" << Instruction["rd_offset"]
                              << " input_cycle:" << Instruction["input_cycle"]
                              << " copy_offset:" << Instruction["copy_offset_flag"]
                              << " element_num:" << Instruction["element_num"]
                              << std::endl;
                else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VVMAX") == 0)
                    std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                               << " rs1:" << Instruction["source_1"]
                              << " rs2:" << Instruction["source_2"]
                              << " rd:" << Instruction["destination"]
                              << " input_index:"  << Instruction["input_index"]
                              << " output_index:"  << Instruction["output_index"]
                              << " rs1_offset:"  << Instruction["rs1_offset"]
                              << " rs2_offset:"  << Instruction["input_element_in_total"] << "+"  <<  Instruction["rs2_offset_in_output"]
                              << " rd_offset:"  << Instruction["input_element_in_total"] << "+"  << Instruction["rs2_offset_in_output"]
                              << " copy_offset:" << Instruction["copy_offset_flag"]
                              << " element_num:" << Instruction["element_num"]
                              << std::endl;
                else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VM") == 0)
                    std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                               << " rs:" << Instruction["source"]
                              << " rd:" << Instruction["destination"]
                              << " input_index:"  << Instruction["input_index"]
                              << " output_index:"  << Instruction["output_index"]
                              << " rs_offset:"  << Instruction["rs_offset"]
                              << " rd_offset:"  << Instruction["input_element_in_total"] << "+"  << Instruction["rd_offset_in_output"]
                              << " copy_offset:" << Instruction["copy_offset_flag"]
                              << " element_num:" << Instruction["element_num"]
                              << std::endl;
                else
                    std::cout << "    " << Operation << std::endl;
            }
        }
    }
}

void DetailAppend::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/7_da.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}