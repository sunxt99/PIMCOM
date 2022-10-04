//
// Created by SXT on 2022/9/1.
//

#include "DetailAppend.h"

static int EndInputChannelIndex[MAX_AG] = {0}; // 最终该AG结束在的input_channel_index
static int EndIndexInInputChannel[MAX_AG] = {0};
static int OffsetForFC[MAX_AG] = {0};
static int SourceIndex[MAX_AG] = {0};

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;
extern std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
extern struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
extern std::vector<int> PIMCOM_2_effective_node;
extern struct PIMCOM_3_hierarchy_map PIMCOM_3_hierarchy_map;
extern std::map<int, std::vector<int>> PIMCOM_3_virtual_core_crossbar_map;
extern std::map<int,int> PIMCOM_4_physical_core_placement;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_4_physical_crossbar_placement;
extern std::vector<struct PIMCOM_5_pool_info> PIMCOM_5_pool_info;
extern struct PIMCOM_6_first_AG_info PIMCOM_6_first_AG_info;
extern struct PIMCOM_6_physical_core_AG_map PIMCOM_6_physical_core_AG_map;
extern struct PIMCOM_6_recv_info PIMCOM_6_recv_info;
extern std::vector<struct PIMCOM_6_instruction_ir> PIMCOM_6_base_instruction_ir;
extern std::vector<std::vector<int>> PIMCOM_6_input_cycle_record;
extern std::map<int, struct PIMCOM_6_instruction_ir> PIMCOM_6_post_instruction_ir;
extern std::map<int, struct PIMCOM_6_instruction_ir> PIMCOM_6_post_multi_core_instruction_ir;
extern struct PIMCOM_7_reload_info PIMCOM_7_reload_info;
extern std::vector<struct PIMCOM_6_instruction_ir> PIMCOM_7_base_instruction_with_reload;

struct PIMCOM_8_detailed_instruction_ir PIMCOM_8_detailed_instruction_ir;


void DetailAppend::AppendDetail(Json::Value &DNNInfo)
{
    if (FastMode)
    {
        instruction_group_num = PIMCOM_7_base_instruction_with_reload.size();
        core_num = PIMCOM_6_physical_core_AG_map.core_list.size();
        PIMCOM_8_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group.resize(instruction_group_num);
        PreProcessFast(DNNInfo);
        PrepareForInputFast(DNNInfo);
    }
    else
    {
        instruction_group_num = static_cast<int>(DNNInfo["7_base_instruction_with_reload"].size());
        core_num = static_cast<int>(DNNInfo["6_physical_core_AG_map"]["core_list"].size());
        PreProcessSlow(DNNInfo);
        PrepareForInputSlow(DNNInfo);
    }
}

void DetailAppend::PreProcessFast(Json::Value &DNNInfo)
{
    int AG_num = PIMCOM_2_resource_info.AGs;
    SourceIndex[0] = 0;
    for (int i = 1; i < AG_num; ++i)
    {
        int cur_core = PIMCOM_3_hierarchy_map.whole[i][0].vcore_index;
        int cur_node = PIMCOM_3_hierarchy_map.whole[i][0].node_index;
        int cur_replication = PIMCOM_3_hierarchy_map.whole[i][0].replication_index;
        int last_core = PIMCOM_3_hierarchy_map.whole[i-1][0].vcore_index;
        int last_node = PIMCOM_3_hierarchy_map.whole[i-1][0].node_index;
        int last_replication = PIMCOM_3_hierarchy_map.whole[i-1][0].replication_index;
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

void DetailAppend::PreProcessSlow(Json::Value &DNNInfo)
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


void DetailAppend::PrepareForInputFast(Json::Value &DNNInfo)
{
    for (int i = 0; i < instruction_group_num; ++i)
    {
        struct PIMCOM_6_instruction_ir InstructionGroup = PIMCOM_7_base_instruction_with_reload[i];
        for (int j = 0; j < core_num; ++j)
        {
            std::vector<struct INST> InstructionIrList = InstructionGroup.core_list[j].instruction_ir_list;
            int instruction_ir_num = InstructionIrList.size();
            for (int k = 0; k < instruction_ir_num; ++k)
            {
                struct INST tmpInstruction = InstructionIrList[k];
                std::string operation = tmpInstruction.operation;

                if (operation != "MVMUL")
                {
                    PIMCOM_8_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
                    continue;
                }

                int node_index = tmpInstruction.node_index;
                int AG_index_in_replication = tmpInstruction.AG_index_in_replication;
                int AG_index_in_total = tmpInstruction.AG_index_in_total;
                int input_cycle_index = tmpInstruction.input_cycle_index;
                int input_element_num = tmpInstruction.input_element_num;
                int replication_index = tmpInstruction.replication_index;
                int level_index = PIMCOM_node_list[node_index].level_index;
                struct PIMCOM_node Node = PIMCOM_node_list[node_index];
                if (Node.operation == "OP_CONV")
                {
                    // kernel_h * kernel_w
                    int input_channel_size = Node.param.input_channel;
                    int input_channel_num = 0;
                    int kernel_w = Node.param.kernel_w;
                    int kernel_h = Node.param.kernel_h;
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
                        struct INST Instruction;
                        Instruction.type = VEC1OP;
                        Instruction.operation = "VM";
                        Instruction.stage = "PREPARE";
                        Instruction.conv_or_fc = "OP_CONV";
                        Instruction.input_cycle_index = input_cycle_index;
                        Instruction.node_index = node_index;
                        Instruction.replication_index = replication_index;
                        Instruction.AG_index_in_total = AG_index_in_total;
                        Instruction.AG_index_in_replication = AG_index_in_replication;
                        Instruction.element_num = move_vector_size;
                        Instruction.level_index = level_index;
                        Instruction.source = SourceIndex[AG_index_in_total];
                        Instruction.input_channel_index = l;
                        int rs_offset_w = l % kernel_w;
                        int rs_offset_h = l / kernel_h;
//                        Instruction.rs_offset_w = rs_offset_w;
//                        Instruction.rs_offset_h = rs_offset_h;
                        int rs_offset_in_channel = (AG_index_in_replication == 0 ? 0 : (l == start_input_channel_index) ? EndIndexInInputChannel[AG_index_in_total-1] : 0);
                        Instruction.rs_offset_in_channel = rs_offset_in_channel;
                        Instruction.rs_offset_between_channel = -1;
                        Instruction.rs_offset = -1;
                        Instruction.destination = AG_index_in_total;
                        Instruction.rd_offset = rd_offset;
                        rd_offset += move_vector_size;

                        PIMCOM_8_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(Instruction);
                        if (IsBreak)
                        {
                            break;
                        }
                    }
                    PIMCOM_8_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
                }
                else if (Node.operation == "OP_FC")
                {

                    // 改进方法
                    int move_vector_size = input_element_num;

                    if(AG_index_in_total == SourceIndex[AG_index_in_total])
                        OffsetForFC[AG_index_in_total] = 0;
                    else
                        OffsetForFC[AG_index_in_total] = OffsetForFC[AG_index_in_total-1] + move_vector_size;
                    struct INST Instruction;
                    Instruction.type = VEC1OP;
                    Instruction.operation = "VM";
                    Instruction.conv_or_fc = "OP_FC";
                    Instruction.stage = "PREPARE";
                    Instruction.input_cycle_index = input_cycle_index;
                    Instruction.node_index = node_index;
                    Instruction.replication_index = replication_index;
                    Instruction.AG_index_in_total = AG_index_in_total;
                    Instruction.AG_index_in_replication = AG_index_in_replication;
                    Instruction.element_num = move_vector_size;
                    Instruction.level_index = level_index;
                    Instruction.source = SourceIndex[AG_index_in_total];
                    Instruction.destination = AG_index_in_total;
                    Instruction.rs_offset = OffsetForFC[AG_index_in_total];
                    Instruction.rd_offset = 0;
                    PIMCOM_8_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(Instruction);
                    PIMCOM_8_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);

                    int start_index_in_input_channel = 0;
                    if (AG_index_in_replication != 0)
                    {
                        start_index_in_input_channel = EndIndexInInputChannel[AG_index_in_total-1];
                    }
                    EndIndexInInputChannel[AG_index_in_total] = start_index_in_input_channel+move_vector_size;
                }
            }
        }
    }
}


void DetailAppend::PrepareForInputSlow(Json::Value &DNNInfo)
{
    for (int i = 0; i < instruction_group_num; ++i)
    {
        Json::Value InstructionGroup = DNNInfo["7_base_instruction_with_reload"][i];
        for (int j = 0; j < core_num; ++j)
        {
            Json::Value InstructionIrList = InstructionGroup["core_list"][j];
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

                        DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(Instruction);
                        if (IsBreak)
                        {
                            break;
                        }
                    }
                    DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(tmpInstruction);
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
//                    DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(Instruction);
//                    DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(tmpInstruction);


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
                    DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(Instruction);

                    DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(tmpInstruction);

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
                    DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].append(tmpInstruction);
                }
            }
        }
    }
}

static int inference_start = 100;
static int inference_end = 100;
void DetailAppend::SaveDetailedInstructionFast(Json::Value & DNNInfo)
{
    std::ofstream OutFile("../fast3.txt", std::ios::out | std::ios::trunc);
    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        OutFile << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;
        for (int i = 0; i < instruction_group_num; ++i)
        {
            OutFile << "========================================= base instruction_group " << i << " =========================================" << std::endl;
            for (int j = 0; j < core_num; ++j)
            {
                OutFile << "core " << j << std::endl;
                int instruction_num = PIMCOM_8_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    struct INST Instruction = PIMCOM_8_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list[k];
                    int instruction_level_index = Instruction.level_index;
                    if (instruction_level_index > inf)
                    {
                        continue;
                    }
                    SaveSingleInstructionFast(OutFile, Instruction, inf);
                }
            }
        }
    }
    OutFile.close();
}



void DetailAppend::SaveDetailedInstructionSlow(Json::Value &DNNInfo)
{
    std::ofstream OutFile("../slow3.txt", std::ios::out | std::ios::trunc);

    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        OutFile << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;
        for (int i = 0; i < instruction_group_num; ++i)
        {
            OutFile << "========================================= base instruction_group " << i << " =========================================" << std::endl;
            for (int j = 0; j < core_num; ++j)
            {
                OutFile << "core " << j << std::endl;
                int instruction_num = DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"].size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    Json::Value Instruction = DNNInfo["8_detailed_instruction_ir"]["1_prepare_for_input"]["instruction_group"][i]["core_list"][j]["instruction_ir_list"][k];
                    int instruction_level_index = Instruction["level_index"].asInt();
                    if (instruction_level_index > inf)
                    {
                        continue;
                    }
                    SaveSingleInstructionSlow(OutFile, Instruction, inf);
                }
            }
        }
    }
    OutFile.close();
}



void DetailAppend::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/8_da.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}