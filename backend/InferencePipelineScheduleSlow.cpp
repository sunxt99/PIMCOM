//
// Created by SXT on 2022/9/16.
//

#include "InferencePipelineSchedule.h"

static int AG_flags[MAX_AG] = {0};
static int AG_accumulated_num[MAX_AG] = {0};
static int AG_output_element_size[MAX_AG] = {0};

static int activate_flag[MAX_AG] = {0};
static int add_flag[MAX_AG] = {0};
static int comm_flag[MAX_AG] = {0};
static int wb_flag[MAX_AG] = {0};

static int node_offset_instruction_group[MAX_AG] = {0};
static int node_offset_inference[MAX_AG] = {0};
static int node_offset_inference_old[MAX_AG] = {0};

void InferencePipelineSchedule::ScheduleExecutionSlow(Json::Value &DNNInfo)
{
    core_num = static_cast<int>(DNNInfo["3_virtual_core_crossbar_map"].size());
    node_num = static_cast<int>(DNNInfo["5_node_list_augmented"].size());
    NodeList = DNNInfo["5_node_list_augmented"];
    SchedulePreparationSlow(DNNInfo);
    ScheduleNaiveSlow(DNNInfo);
}

void InferencePipelineSchedule::SchedulePreparationSlow(Json::Value &DNNInfo)
{
    //// 注意Naive写法是针对没有split_AG的情况。每个AG只有一个对应的Core
    // 根据4_physical_crossbar_placement信息提供的Core上Crossbar的关系得到Core上AG的关系
    DNNInfo["6_physical_core_AG_map"]["core_list"].resize(core_num);
    int crossbar_num = DNNInfo["4_physical_crossbar_placement"].size();
    //// 注意这里的AG_flags最多支持10000个AG的系统
    // （要得到AG的信息，遍历core_Crossbar）
    for (int i = 0; i < crossbar_num; ++i)
    {
        int AG_index = DNNInfo["4_physical_crossbar_placement"][i]["array_group_total"].asInt();
        if (AG_flags[AG_index] != 1)
        {
            AG_flags[AG_index] = 1;
            int core_index = DNNInfo["4_physical_crossbar_placement"][i]["physical_core"].asInt();
            int rep_index = DNNInfo["4_physical_crossbar_placement"][i]["replication_index"].asInt();
            int node_index = DNNInfo["4_physical_crossbar_placement"][i]["node_index"].asInt();
            int AG_index_in_total = DNNInfo["4_physical_crossbar_placement"][i]["array_group_total"].asInt();
            int AG_index_in_replication = DNNInfo["4_physical_crossbar_placement"][i]["array_group_in_weight"].asInt();
            int AG_num_per_replication = DNNInfo["4_physical_crossbar_placement"][i]["AG_num_per_replication"].asInt();
            Json::Value AGInfo;
            AGInfo["AG_index_in_total"] = AG_index_in_total;
            AGInfo["AG_index_in_replication"] = AG_index_in_replication;
            AGInfo["AG_num_per_replication"] = AG_num_per_replication;
            AGInfo["replication_index"] = rep_index;

            AGInfo["AGP"] = DNNInfo["node_list"][node_index]["AGP"].asInt();
            AGInfo["agp_index"] = DNNInfo["4_physical_crossbar_placement"][i]["agp_index"].asInt();
            AGInfo["agp_offset"] = DNNInfo["4_physical_crossbar_placement"][i]["agp_offset"].asInt();
            AGInfo["replication_num"] = DNNInfo["node_list"][node_index]["replication_num"].asInt();
            AGInfo["replication_num_origin"] = DNNInfo["node_list"][node_index]["replication_num_origin"].asInt();
            AGInfo["input_cycle_in_total"] = DNNInfo["node_list"][node_index]["input_cycle_in_total"].asInt();

            AGInfo["input_cycle_this_replication"] = DNNInfo["4_physical_crossbar_placement"][i]["input_cycle_this_replication"].asInt();
            AGInfo["input_cycle_this_replication_start"] = DNNInfo["4_physical_crossbar_placement"][i]["input_cycle_this_replication_start"].asInt();
            AGInfo["input_cycle_this_replication_end"] = DNNInfo["4_physical_crossbar_placement"][i]["input_cycle_this_replication_end"].asInt();

            AGInfo["level_index"] = NodeList[node_index]["level_index"].asInt();
            DNNInfo["6_physical_core_AG_map"]["core_list"][core_index]["AG_list"].append(AGInfo);
            DNNInfo["6_physical_core_AG_map"]["core_list"][core_index]["node_list"].append(node_index);
        }
    }

    //// 得到两个调度过程中需要的值（每个结点Rep0 AG0所在的核、以及每个结点每个Rep的AG0所在的核）
    DNNInfo["6_first_AG_info"]["node_list"].resize(node_num);
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"].size();
        for (int j = 0; j < AG_num; ++j)
        {
            int AG_index_in_replication = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["AG_index_in_replication"].asInt();
            if (AG_index_in_replication == 0)
            {
                int node_index = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["node_list"][j].asInt();
                int replication_index = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["replication_index"].asInt();
                DNNInfo["6_first_AG_info"]["node_list"][node_index]["replication_list"][replication_index] = i;
            }
        }
    }

    //// 得到周期结束需要传输数据的核
    DNNInfo["6_recv_info"]["node_list"].resize(node_num);
    int node_appearance_num[MAX_NODE] = {0};
    int node_appearance_element[MAX_NODE] = {0};
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"].size();
        int pre_node_index = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["node_list"][0].asInt();
        int pre_replication_index = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][0]["replication_index"].asInt();
        int pre_AG_index = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][0]["AG_index_in_total"].asInt();
        int pre_AG_index_in_replication = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][0]["AG_index_in_replication"].asInt();
        int pre_core_index = i;
        Json::Value RecvInfo;
        int pre_AG_height = DNNInfo["3_hierarchy_map"]["whole"][pre_AG_index][0]["height_end"].asInt() - DNNInfo["3_hierarchy_map"]["whole"][pre_AG_index][0]["height_start"].asInt() + 1;
        RecvInfo["replication_index"] = pre_replication_index;
        RecvInfo["AG_index"] = pre_AG_index;
        RecvInfo["AG_index_in_replication"] = pre_AG_index_in_replication;
        RecvInfo["core_index"] = pre_core_index;
        RecvInfo["node_index"] = pre_node_index;
        if (strcmp(NodeList[pre_node_index]["operation"].asCString(),"OP_FC")==0)
        {
            RecvInfo["start_offset_num"] = node_appearance_num[pre_node_index];
            RecvInfo["start_offset_element"] = node_appearance_element[pre_node_index];
            RecvInfo["recv_num"] = 1;
            RecvInfo["recv_element"] = pre_AG_height;
        }
        DNNInfo["6_recv_info"]["node_list"][pre_node_index].append(RecvInfo);
        node_appearance_num[pre_node_index]++;
        node_appearance_element[pre_node_index] += pre_AG_height;
        for (int j = 1; j < AG_num; ++j)
        {
            int node_index = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["node_list"][j].asInt();
            int replication_index = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["replication_index"].asInt();
            int AG_index = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["AG_index_in_total"].asInt();
            int AG_index_in_replication = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["AG_index_in_replication"].asInt();
            int AG_height = DNNInfo["3_hierarchy_map"]["whole"][AG_index][0]["height_end"].asInt() - DNNInfo["3_hierarchy_map"]["whole"][AG_index][0]["height_start"].asInt() + 1;
            if (node_index != pre_node_index || pre_replication_index != replication_index)
            {
                Json::Value RecvInfo2;
                RecvInfo2["replication_index"] = replication_index;
                RecvInfo2["AG_index"] = AG_index;
                RecvInfo2["AG_index_in_replication"] = AG_index_in_replication;
                RecvInfo2["core_index"] = i;
                RecvInfo2["node_index"] = node_index;
                if (strcmp(NodeList[node_index]["operation"].asCString(),"OP_FC")==0)
                {
                    RecvInfo2["start_offset_num"] = node_appearance_num[node_index];
                    RecvInfo2["start_offset_element"] = node_appearance_element[node_index];
                    RecvInfo2["recv_num"] = 1;
                    RecvInfo2["recv_element"] = AG_height;
                }
                DNNInfo["6_recv_info"]["node_list"][node_index].append(RecvInfo2);
            }
            else
            {
                if (strcmp(NodeList[node_index]["operation"].asCString(),"OP_FC")==0)
                {
                    int already_num = DNNInfo["6_recv_info"]["node_list"][node_index].size();
                    DNNInfo["6_recv_info"]["node_list"][node_index][already_num - 1]["recv_num"] = DNNInfo["6_recv_info"]["node_list"][node_index][already_num - 1]["recv_num"].asInt() + 1;
                    DNNInfo["6_recv_info"]["node_list"][node_index][already_num - 1]["recv_element"] = DNNInfo["6_recv_info"]["node_list"][node_index][already_num - 1]["recv_element"].asInt() + AG_height;
                }
            }
            node_appearance_num[node_index]++;
            node_appearance_element[node_index] += AG_height;
            pre_replication_index = replication_index;
            pre_node_index = node_index;
        }
    }

//    std::cout << "****************** Mapping Result ********************" << std::endl;
//    for (int i = 0; i < core_num; ++i)
//    {
//        std::cout << i << std::endl;
//        int AG_num = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"].size();
//        for (int j = 0; j < AG_num; ++j)
//        {
//            std::cout << "    " << DNNInfo["6_physical_core_AG_map"]["core_list"][i]["node_list"][j]
//                      << "    " << DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["replication_index"]
//                      << "    " << DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["AG_index_in_replication"]
//                      << "   | " << DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["AG_index_in_total"] << std::endl;
//        }
//    }

    //// 得到每个AG的instruction_group_num
    for (int i = 0; i < DNNInfo["2_AG_partition"].size(); ++i)
    {
        int replication_num = DNNInfo["2_AG_partition"][i]["replication_num"].asInt();
        for (int j = 0; j < replication_num; ++j)
        {
            DNNInfo["2_AG_partition"][i]["replication"][j]["instruction_group_num"] = static_cast<int>(ceil(DNNInfo["2_AG_partition"][i]["replication"][j]["input_cycle_this_replication"].asFloat() / float(operation_cycle_before_comm)));
            int AG_num = DNNInfo["2_AG_partition"][i]["replication"][j]["AG_list"].size();
            for (int k = 0; k < AG_num; ++k)
            {
                int AG_index = DNNInfo["2_AG_partition"][i]["replication"][j]["AG_list"][k]["AG_index"].asInt();
                DNNInfo["3_hierarchy_map"]["whole"][AG_index][0]["instruction_group_num"] = DNNInfo["2_AG_partition"][i]["replication"][j]["instruction_group_num"];
                DNNInfo["6_AG_instruction_group_num"][AG_index] = DNNInfo["2_AG_partition"][i]["replication"][j]["instruction_group_num"];
            }
        }
    }
}

void InferencePipelineSchedule::ScheduleNaiveStage1Slow(Json::Value &  DNNInfo, int instruction_group_index, bool append_instruction)
{
    //// 首先为每个AG生成MVMUL操作
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = CoreList[i]["AG_list"].size();
        for (int j = 0; j < AG_num; ++j)
        {
            Json::Value Instruction;
            int AG_index_in_total = CoreList[i]["AG_list"][j]["AG_index_in_total"].asInt();
            int AG_index_in_replication = CoreList[i]["AG_list"][j]["AG_index_in_replication"].asInt();
            int AG_num_per_replication = CoreList[i]["AG_list"][j]["AG_num_per_replication"].asInt();
            int input_cycle_in_total = CoreList[i]["AG_list"][j]["input_cycle_in_total"].asInt();
            int replication_index = CoreList[i]["AG_list"][j]["replication_index"].asInt();
            int replication_num = CoreList[i]["AG_list"][j]["replication_num"].asInt();
            int node_index = CoreList[i]["node_list"][j].asInt();
            int AGP = CoreList[i]["AG_list"][j]["AGP"].asInt();
            int agp_index = CoreList[i]["AG_list"][j]["agp_index"].asInt();
            int agp_offset = CoreList[i]["AG_list"][j]["agp_offset"].asInt();
            // 注意: 这里的start和end都包括，也就是input_cycle数量为end-start+1
            int input_cycle_this_replication_start = CoreList[i]["AG_list"][j]["input_cycle_this_replication_start"].asInt();
            int input_cycle_this_replication_end = CoreList[i]["AG_list"][j]["input_cycle_this_replication_end"].asInt();
            int level_index = CoreList[i]["AG_list"][j]["level_index"].asInt();
            // 测试用
//            int input_cycle_this_replication_end;
//            switch (node_index)
//            {
//                case 1:
//                    input_cycle_this_replication_end = input_cycle_this_replication_start + 4;
//                    break;
//                case 3:
//                    input_cycle_this_replication_end = input_cycle_this_replication_start + 5;
//                    break;
//                case 6:
//                    input_cycle_this_replication_end = input_cycle_this_replication_start + 6;
//                    break;
//                default:
//                    input_cycle_this_replication_end = CoreList[i]["AG_list"][j]["input_cycle_this_replication_end"].asInt();
//                    break;
//            }

            if (input_cycle_this_replication_start + AG_accumulated_num[AG_index_in_total] <= input_cycle_this_replication_end) // 例子。每个节点的输入个数为Line11所示。
            {
                Instruction["AG_num_per_replication"] = AG_num_per_replication;
                Instruction["input_cycle_index"] = input_cycle_this_replication_start + AG_accumulated_num[AG_index_in_total];
                Instruction["AG_index_in_total"] = AG_index_in_total;
                Instruction["replication_index"] = replication_index;
                Instruction["replication_num"] = replication_num;
                Instruction["input_cycle_in_total"] = input_cycle_in_total;
                Instruction["AG_index_in_replication"] = AG_index_in_replication;
                Instruction["input_cycle_this_replication_start"] = input_cycle_this_replication_start;
                Instruction["input_cycle_this_replication_end"] = input_cycle_this_replication_end;
                Instruction["conv_or_fc"] = NodeList[node_index]["operation"];
                Instruction["node_index"] = node_index;
                Instruction["AGP"] = AGP;
                Instruction["agp_index"] = agp_index;
                Instruction["operation"] = "MVMUL";
                Instruction["destination"] = Instruction["AG_index_in_total"];
                Instruction["source"] = Instruction["AG_index_in_total"];
                Instruction["level_index"] = level_index;

                // get the input_element_num and output_element_num
                int effective_node_index = DNNInfo["node_list"][node_index]["effective_node_index"].asInt();
                int crossbar_num = DNNInfo["2_AG_partition"][effective_node_index]["replication"][replication_index]["AG_list"][AG_index_in_replication]["virtual_crossbar_list"].size();
                int crossbar_start_index = DNNInfo["2_AG_partition"][effective_node_index]["replication"][replication_index]["AG_list"][AG_index_in_replication]["virtual_crossbar_list"][0].asInt();
                int crossbar_end_index = DNNInfo["2_AG_partition"][effective_node_index]["replication"][replication_index]["AG_list"][AG_index_in_replication]["virtual_crossbar_list"][crossbar_num-1].asInt();
                int input_element_num = DNNInfo["2_virtual_crossbar"][crossbar_start_index]["height_end"].asInt()-DNNInfo["2_virtual_crossbar"][crossbar_start_index]["height_start"].asInt()+1;
                int output_element_num = DNNInfo["2_virtual_crossbar"][crossbar_end_index]["width_end"].asInt()-DNNInfo["2_virtual_crossbar"][crossbar_start_index]["width_start"].asInt()+1;
                Instruction["input_element_num"] = input_element_num;
                Instruction["output_element_num"] = output_element_num;
                AG_output_element_size[AG_index_in_total] = output_element_num;
                Json::Value Offset;
                Offset["rd"] = 1;
                Offset["rs"] = 0;
                Offset["value"] = node_offset_inference[AG_index_in_total]*output_element_num + agp_offset;
                // 暂时先不考虑AGP的存在了
//                int whole_output_width = DNNInfo["node_list"][node_index]["W"].asInt();
//                Offset["value"] = AGP == 1 ? (node_offset_instruction_group[node_index]*output_element_num + agp_offset) : (node_offset_instruction_group[node_index]*whole_output_width + agp_offset) ;
                Instruction["offset"] = Offset;

                Instruction["instruction_group_index"] = instruction_group_index;
                if(append_instruction)
                    DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction);

                // for stage_2 ADD
                if (j != 0)
                {
                    if (node_index == CoreList[i]["node_list"][j-1].asInt() && replication_index == CoreList[i]["AG_list"][j-1]["replication_index"].asInt())
                    {
                        add_flag[AG_index_in_total] = 1;
                    }
                }

                if (AG_index_in_replication == 0)
                {
                    activate_flag[AG_index_in_total] = 1;
                }

                // for stage_3 SEND/RECV
                if (AG_index_in_replication != 0)
                {
                    int estimate_first_AG_index = j - AG_index_in_replication;
                    if (estimate_first_AG_index < 0 ||
                        CoreList[i]["AG_list"][estimate_first_AG_index]["AG_index_in_replication"].asInt() != 0 ||
                        CoreList[i]["AG_list"][estimate_first_AG_index]["replication_index"].asInt() !=
                        replication_index ||
                        CoreList[i]["node_list"][estimate_first_AG_index].asInt() != node_index)
                    {
                        comm_flag[AG_index_in_total] = 1;
                    }
                }

                // for stage_4 WB
                if (AG_index_in_replication == 0)
                {
                    wb_flag[AG_index_in_total] = 1;
                }

                // consider the offset
                node_offset_instruction_group[AG_index_in_total] += 1;
                node_offset_inference[AG_index_in_total] += 1;

                if (AG_index_in_replication == 0)
                    DNNInfo["6_input_cycle_record"][node_index].append(input_cycle_this_replication_start+AG_accumulated_num[AG_index_in_total]);
                AG_accumulated_num[AG_index_in_total] += 1;
            }
        }
    }
}

void InferencePipelineSchedule::ScheduleNaiveStage2Slow(Json::Value &  DNNInfo, int instruction_group_index)
{
    //// 同一结点且同一权重块的AG之间的结果融合（VADD）
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = CoreList[i]["AG_list"].size();
        int node_index = CoreList[i]["node_list"][0].asInt();
        int replication_index = CoreList[i]["AG_list"][0]["replication_index"].asInt();
        int p = 1;
        for (int j = 1; j < AG_num; ++j)
        {
            int current_node_index = CoreList[i]["node_list"][j].asInt();
            int current_replication_index = CoreList[i]["AG_list"][j]["replication_index"].asInt();
            int AG_index_in_total = CoreList[i]["AG_list"][j]["AG_index_in_total"].asInt();
            int level_index = CoreList[i]["AG_list"][j]["level_index"].asInt();
            if (node_index == current_node_index && replication_index == current_replication_index)
            {
                if (add_flag[AG_index_in_total] == 1)
                {
                    Json::Value Instruction;
                    Instruction["operation"] = "VADD";
                    Instruction["destination"] = CoreList[i]["AG_list"][j-p]["AG_index_in_total"];
                    Instruction["source_1"] = CoreList[i]["AG_list"][j-p]["AG_index_in_total"];
                    Instruction["source_2"] = CoreList[i]["AG_list"][j]["AG_index_in_total"];
                    Instruction["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    Instruction["level_index"] = level_index;

                    Instruction["relative_length"] = 1;
                    Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[Instruction["source_1"].asInt()];
                    Instruction["instruction_group_index"] = instruction_group_index;
                    Json::Value Offset;
                    Offset["rd"] = 1;
                    Offset["rs1"] = 1;
                    Offset["rs2"] = 1;
                    Offset["value"] = (node_offset_inference[AG_index_in_total]-1)*Instruction["element_num"].asInt();
                    Instruction["offset"] = Offset;
                    DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction);
                    p += 1;
                }
            }
            else
            {
                node_index = current_node_index;
                replication_index = current_replication_index;
                p = 1;
            }
        }
    }
}

void InferencePipelineSchedule::ScheduleNaiveStage3Slow(Json::Value &  DNNInfo, int instruction_group_index)
{
    int comm_index = 0;
    //// 结果发送与融合
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = CoreList[i]["AG_list"].size();
        for (int j = 0; j < AG_num; ++j)
        {
            int node_index = CoreList[i]["node_list"][j].asInt();
            int replication_index = CoreList[i]["AG_list"][j]["replication_index"].asInt();
            int AG_index_in_replication = CoreList[i]["AG_list"][j]["AG_index_in_replication"].asInt();
            int AG_index_in_total = CoreList[i]["AG_list"][j]["AG_index_in_total"].asInt();
            int level_index = CoreList[i]["AG_list"][j]["level_index"].asInt();
            bool SendRecv = false;
            if (AG_index_in_replication != 0)
            {
                int estimate_first_AG_index =  j - AG_index_in_replication;

                if (estimate_first_AG_index < 0 ||
                    CoreList[i]["AG_list"][estimate_first_AG_index]["AG_index_in_replication"].asInt() != 0 ||
                    CoreList[i]["AG_list"][estimate_first_AG_index]["replication_index"].asInt() != replication_index ||
                    CoreList[i]["node_list"][estimate_first_AG_index].asInt() != node_index )
                {
                    if (comm_flag[AG_index_in_total] == 1)
                    {
                        SendRecv = true;
                    }
                }
                if (SendRecv)
                {
//                    std::cout << "    " << j << std::endl;
                    int RecvCore = DNNInfo["6_first_AG_info"]["node_list"][node_index]["replication_list"][replication_index].asInt();
                    // 添加发送接收指令和结果融合指令。
                    Json::Value Instruction_send;
                    Instruction_send["level_index"] = level_index;
                    Instruction_send["operation"] = "SEND";
                    Instruction_send["to_core"] = RecvCore;
                    Instruction_send["source"] = AG_index_in_total;
                    Instruction_send["relative_length"] = node_offset_instruction_group[AG_index_in_total];
                    Instruction_send["element_num"] = Instruction_send["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    Instruction_send["instruction_group_index"] = instruction_group_index;
                    Instruction_send["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_send["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    Instruction_send["comm_index"] = comm_index;
                    Instruction_send["instruction_index_in_core"] = DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].size();
                    DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_send);

                    Json::Value Instruction_recv;
                    Instruction_recv["level_index"] = level_index;
                    Instruction_recv["operation"] = "RECV";
                    Instruction_recv["from_core"] = i;
                    // TODO: destination?
                    // 注意，另一个核的AGx对应的AG_index_in_total一定没在该Core中出现过。所以可以作为地址的一种表示。
                    Instruction_recv["destination"] = AG_index_in_total;
                    Instruction_recv["relative_length"] = node_offset_instruction_group[AG_index_in_total];
                    Instruction_recv["element_num"] = Instruction_recv["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    Instruction_recv["instruction_group_index"] = instruction_group_index;
                    Instruction_recv["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_recv["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    Instruction_recv["comm_index"] = comm_index;
                    Instruction_recv["instruction_index_in_core"] = DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].size();
                    DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].append(Instruction_recv);

                    Json::Value Instruction_vadd;
                    Instruction_vadd["level_index"] = level_index;
                    Instruction_vadd["operation"] = "VADD";
                    Json::Value CoreInfo = DNNInfo["6_physical_core_AG_map"]["core_list"][RecvCore];
                    int tmp_AG_num = CoreInfo["AG_list"].size();
                    // tmp_ag_total_index是RecvCore中同node同rep的AG0对应的位置
                    int tmp_ag_total_index = 0;
                    for (int k = 0; k < tmp_AG_num; ++k)
                    {
                        if( CoreInfo["node_list"][k].asInt() == node_index &&
                            CoreInfo["AG_list"][k]["AG_index_in_replication"].asInt() == 0 &&
                            CoreInfo["AG_list"][k]["replication_index"].asInt() == replication_index )
                            tmp_ag_total_index = CoreInfo["AG_list"][k]["AG_index_in_total"].asInt();
                    }
                    Instruction_vadd["source_1"] = tmp_ag_total_index;
                    Instruction_vadd["source_2"] = AG_index_in_total;
                    Instruction_vadd["destination"] = tmp_ag_total_index;
                    Instruction_vadd["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_vadd["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    Json::Value Offset;
                    Offset["rd"] = 1;
                    Offset["rs1"] = 1;
                    Offset["rs2"] = 0;
                    Offset["value"] = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[AG_index_in_total];;
                    Instruction_vadd["offset"] = Offset;
                    Instruction_vadd["relative_length"] = node_offset_instruction_group[node_index];
                    Instruction_vadd["element_num"] = Instruction_vadd["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    Instruction_vadd["instruction_group_index"] = instruction_group_index;
                    DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].append(Instruction_vadd);

                    comm_index++;
                    // 因为之前经过了信息融合，所以不需要多次发送接收。跳过后面同一Rep的其他AG。
                    while (j < AG_num && CoreList[i]["AG_list"][j]["replication_index"].asInt() == replication_index)
                    {
                        j += 1;
                    }
                }
            }
        }
    }
}

void InferencePipelineSchedule::ScheduleNaiveStageActSlow(Json::Value &DNNInfo, int instruction_group_index)
{
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = CoreList[i]["AG_list"].size();
        for (int j = 0; j < AG_num; ++j)
        {
            int AG_index_in_total = CoreList[i]["AG_list"][j]["AG_index_in_total"].asInt();
            int AG_index_in_replication = CoreList[i]["AG_list"][j]["AG_index_in_replication"].asInt();
            int AG_num_per_replication = CoreList[i]["AG_list"][j]["AG_num_per_replication"].asInt();
            int node_index = CoreList[i]["node_list"][j].asInt();
            int level_index = CoreList[i]["AG_list"][j]["level_index"].asInt();
            if (activate_flag[AG_index_in_total] == 1)
            {
                Json::Value Instruction;
                Instruction["level_index"] = level_index;
                Instruction["operation"] = "ReLU";
                Instruction["relative_length"] = node_offset_instruction_group[AG_index_in_total];
                Instruction["source"] = AG_index_in_total;
                Instruction["destination"] = AG_index_in_total;
                Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[Instruction["source"].asInt()];
                Instruction["instruction_group_index"] = instruction_group_index;
                Json::Value Offset;
                Offset["rd"] = 1;
                Offset["rs"] = 1;
                Offset["value"] = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[Instruction["source"].asInt()];
                Instruction["offset"] = Offset;
                DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction);
            }
        }
    }
}

void InferencePipelineSchedule::ScheduleNaiveStage4Slow(Json::Value &  DNNInfo,  int instruction_group_index)
{
    //// 结果传递与写回
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = CoreList[i]["AG_list"].size();
        for (int j = 0; j < AG_num; ++j)
        {
            int node_index = CoreList[i]["node_list"][j].asInt();
            int replication_index = CoreList[i]["AG_list"][j]["replication_index"].asInt();
            int AG_index_in_replication = CoreList[i]["AG_list"][j]["AG_index_in_replication"].asInt();
            int AG_index_in_total = CoreList[i]["AG_list"][j]["AG_index_in_total"].asInt();
            int replication_num = CoreList[i]["AG_list"][j]["replication_num"].asInt();
            int AG_num_per_replication = CoreList[i]["AG_list"][j]["AG_num_per_replication"].asInt();
            int level_index = CoreList[i]["AG_list"][j]["level_index"].asInt();
            // replication_index不等于0 且 AG_index_in_replication=0 的AG0需要把结果传递给Rep0-AG0
            // 还要看AGx和AG0是否在同一个核，这样就避免了同核之间的SEND/RECV
            if (AG_index_in_replication == 0 && replication_index != 0  && wb_flag[AG_index_in_total] > 0)
            {
                int estimated_rep0_ag0 = j - replication_index * AG_num_per_replication;

                if (estimated_rep0_ag0 < 0 ||
                    CoreList[i]["AG_list"][estimated_rep0_ag0]["AG_index_in_replication"].asInt() != 0 ||
                    CoreList[i]["AG_list"][estimated_rep0_ag0]["replication_index"].asInt() != 0 ||
                    CoreList[i]["node_list"][estimated_rep0_ag0].asInt() != node_index )
                {
                    int RecvCore = DNNInfo["6_first_AG_info"]["node_list"][node_index]["replication_list"][0].asInt();
                    Json::Value Instruction_send;
                    Instruction_send["level_index"] = level_index;
                    Instruction_send["operation"] = "SEND";
                    Instruction_send["stage"] = 4;
                    Instruction_send["to_core"] = RecvCore;
                    Instruction_send["source"] = AG_index_in_total;
                    Instruction_send["relative_length"] = node_offset_inference[AG_index_in_total];
                    Instruction_send["element_num"] = Instruction_send["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
//                    int real_instruction_group_index = (Instruction_send["relative_length"].asInt()-1)/operation_cycle_before_comm;
                    int real_instruction_group_index = instruction_group_index;
                    Instruction_send["instruction_group_index"] = real_instruction_group_index;
                    Instruction_send["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_send["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_send);

                    Json::Value Instruction_recv;
                    Instruction_recv["level_index"] = level_index;
                    Instruction_recv["operation"] = "RECV";
                    Instruction_recv["stage"] = 4;
                    Instruction_recv["from_core"] = i;
                    Instruction_recv["destination"] = AG_index_in_total;
                    Instruction_recv["relative_length"] = node_offset_inference[AG_index_in_total];
                    Instruction_recv["element_num"] = Instruction_recv["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    Instruction_recv["instruction_group_index"] = real_instruction_group_index;
                    Instruction_recv["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_recv["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].append(Instruction_recv);

                    // 这里的WB指的是接受之后写回到正确的位置
//                    Json::Value Instruction_wb;
//                    Instruction_wb["level_index"] = level_index;
//                    Instruction_wb["operation"] = "WB";
//                    Instruction_wb["source"] = AG_index_in_total;
//                    Instruction_wb["replication_index"] = replication_index;
//                    Instruction_wb["relative_length"] = node_offset_inference[AG_index_in_total];
//                    Instruction_wb["element_num"] = Instruction_wb["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
//                    Instruction_wb["instruction_group_index"] = real_instruction_group_index;
//                    Instruction_wb["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
//                    Instruction_wb["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
//                    DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].append(Instruction_wb);
                }
                else
                {
//                    Json::Value Instruction_wb;
//                    Instruction_wb["level_index"] = level_index;
//                    Instruction_wb["operation"] = "WB";
//                    Instruction_wb["source"] = AG_index_in_total;
//                    Instruction_wb["replication_index"] = replication_index;
//                    Instruction_wb["relative_length"] = node_offset_inference[AG_index_in_total];
//                    Instruction_wb["element_num"] = Instruction_wb["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
//                    int real_instruction_group_index = instruction_group_index;
//                    Instruction_wb["instruction_group_index"] = real_instruction_group_index;
//                    Instruction_wb["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
//                    Instruction_wb["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
//                    DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_wb);
                }
            }
            else if (AG_index_in_replication == 0 && replication_index == 0  && wb_flag[AG_index_in_total] > 0)
            {
//                Json::Value Instruction_wb;
//                Instruction_wb["level_index"] = level_index;
//                Instruction_wb["operation"] = "WB";
//                Instruction_wb["source"] = AG_index_in_total;
//                Instruction_wb["replication_index"] = replication_index;
//                Instruction_wb["relative_length"] = node_offset_inference[AG_index_in_total];
//                Instruction_wb["element_num"] = Instruction_wb["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
//                int real_instruction_group_index = instruction_group_index;
//                Instruction_wb["instruction_group_index"] = real_instruction_group_index;
//                Instruction_wb["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
//                Instruction_wb["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
//                DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_wb);
            }
        }
    }
}

static int visit_stage5[MAX_NODE] = {0};
void InferencePipelineSchedule::ScheduleNaiveStage5Slow(Json::Value & DNNInfo, int node_index, int level_index, int instruction_group_index)
{
    // 这里的NodeList都是pipeline design中得到的Augmented NodeList
    int consumer_num = NodeList[node_index]["consumer_num"].asInt();
    if (consumer_num == 0)
    {
        return;
    }
    else
    {
        for (int i = 0; i < consumer_num; ++i)
        {
            int consumer_index = NodeList[node_index]["consumer_index"][i].asInt();
            int consumer_level = NodeList[consumer_index]["level_index"].asInt();
            int AG0_core_index = NodeList[consumer_index]["AG0_core_index"].asInt();
            int AG0_index_in_total = NodeList[consumer_index]["AG0_index_in_total"].asInt();
            std::string consumer_op = NodeList[consumer_index]["operation"].asCString();
            if( level_index != consumer_level)
            {
                if (strcmp(consumer_op.c_str(), "OP_CONV") == 0 || strcmp(consumer_op.c_str(), "OP_FC") == 0)
                {
                    ScheduleNaiveStage5Slow(DNNInfo, consumer_index, consumer_level, instruction_group_index);
                }
            }
            else if (visit_stage5[consumer_index] == 0) // 这一句是为了解决一个node会有多个同level的生产者，这样每个该level的生产者都会处理一下该node，造成重复。这里设置的意义是只运行一次后处理即可。
            {
                visit_stage5[consumer_index] = 1;
                if (strcmp(consumer_op.c_str(), "OP_ELTWISE") == 0)
                {
                    int elt_type = NodeList[consumer_index]["param"]["eletype"].asInt();
                    std::string elt_operation;
                    switch (elt_type)
                    {   case 2: elt_operation = "VADD"; break;
                        case 4: elt_operation = "VSUB"; break; }
                    int provider_num_of_consumer = NodeList[consumer_index]["provider_num"].asInt();
                    // 可能会有多个量相加
                    for (int j = 0; j < provider_num_of_consumer; ++j)
                    {
                        int provider_index = NodeList[consumer_index]["provider_index"][j].asInt();
                        if(NodeList[provider_index]["AG0_core_index"] == AG0_core_index && NodeList[provider_index]["AG0_index_in_total"] == AG0_index_in_total)
                            continue;
                        int effective_provider_index = NodeList[provider_index]["AG0_node_index"].asInt();
                        Json::Value Instruction;
                        Instruction["level_index"] = NodeList[consumer_index]["level_index"];
                        Instruction["operation"] = "ELTWISE";
                        Instruction["operation_type"] = elt_operation;
                        Instruction["node_index"] = consumer_index;
                        Instruction["source_1"] = AG0_index_in_total;
                        Instruction["source_2"] = NodeList[provider_index]["AG0_index_in_total"];
                        Instruction["destination"] = AG0_index_in_total;
                        // 这个relative_length是未考虑复制块的情况，所以弃用
                        // Instruction["relative_length"] = node_offset_inference[AG0_index_in_total];
                        Instruction["relative_length"] = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
                        Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[AG0_index_in_total];
                        Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
//                        int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                        int real_instruction_group_index = instruction_group_index;
                        DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
                    }
                }
                else if (strcmp(consumer_op.c_str(), "OP_CONCAT") == 0)
                {
                    int provider_num_of_consumer = NodeList[consumer_index]["provider_num"].asInt();
                    // 可能会有多个量相加
                    int output_channel_element_size_concat = 0;
                    for (int j = 0; j < provider_num_of_consumer; ++j)
                    {
                        int provider_index = NodeList[consumer_index]["provider_index"][j].asInt();
                        int effective_provider_index = NodeList[provider_index]["AG0_node_index"].asInt();
                        output_channel_element_size_concat += NodeList[effective_provider_index]["output_dim"][1].asInt();
                    }
                    int accumulated_offset = 0;
                    for (int j = 0; j < provider_num_of_consumer; ++j)
                    {
                        int provider_index = NodeList[consumer_index]["provider_index"][j].asInt();
                        int provider_AG0_index = NodeList[provider_index]["AG0_index_in_total"].asInt();
                        int effective_provider_index = NodeList[provider_index]["AG0_node_index"].asInt();
//                        Json::Value Instruction;
//                        Instruction["level_index"] = NodeList[consumer_index]["level_index"];
//                        Instruction["operation"] = "CONCAT";
//                        Instruction["operation_type"] = "VM";
//                        Instruction["node_index"] = consumer_index;
//                        Instruction["source"] = NodeList[provider_index]["AG0_index_in_total"];
//                        Instruction["destination"] = AG0_index_in_total;
////                        Instruction["relative_length"] = node_offset_inference[provider_AG0_index];  // 未考虑重复块
//                        Instruction["relative_length"] = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
//                        Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[provider_AG0_index];
//                        Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
////                        int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
//                        int real_instruction_group_index = instruction_group_index;
//                        DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
                        // 下面这个代码是将CONCAT代码展开来，即具体形式。
                        {
                            // 这个output_channel_num是完整的
                            // int output_channel_num = NodeList[provider_index]["output_dim"][2].asInt() * NodeList[provider_index]["output_dim"][3].asInt(); // H*W
                            // 这个output_channel_num不完全
                            // int output_channel_num = node_offset_inference[provider_AG0_index];
                            // 这个output_channel_num是可行的
                            int output_channel_num = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
                            int output_channel_element_size = NodeList[provider_index]["output_dim"][1].asInt();
                            if (j != 0)
                            {
                                int last_provider_index = NodeList[consumer_index]["provider_index"][j-1].asInt();
                                accumulated_offset += NodeList[last_provider_index]["output_dim"][1].asInt();;
                            }
                            for (int k = 0; k < output_channel_num; ++k)
                            {
                                int input_cycle = DNNInfo["6_input_cycle_record"][effective_provider_index][k].asInt();
                                int rs_offset = input_cycle * output_channel_element_size;
                                int rd_offset = input_cycle * output_channel_element_size_concat + accumulated_offset;
                                Json::Value Instruction_detail;
                                Instruction_detail["level_index"] = NodeList[consumer_index]["level_index"];
                                Instruction_detail["input_cycle"] = input_cycle;
                                Instruction_detail["operation"] = "CONCAT";
                                Instruction_detail["operation_type"] = "VM-detail";
                                Instruction_detail["node_index"] = consumer_index;
                                Instruction_detail["source"] = NodeList[provider_index]["AG0_index_in_total"];
                                Instruction_detail["destination"] = AG0_index_in_total;
                                Instruction_detail["rs_offset"] = rs_offset;
                                Instruction_detail["rd_offset"] = rd_offset;
                                Instruction_detail["relative_length"] = 1;
                                Instruction_detail["element_num"] = Instruction_detail["relative_length"].asInt() * AG_output_element_size[provider_AG0_index];
                                Instruction_detail["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
//                                int real_instruction_group_index = instruction_group_index;
                                DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction_detail);
                            }
                        }
                    }
                }
                else if (strcmp(consumer_op.c_str(), "OP_RELU") == 0 || strcmp(consumer_op.c_str(), "OP_TANH") == 0 || strcmp(consumer_op.c_str(), "OP_SIGMOID") == 0)
                {
                    // 如果该consumer的生产者是CONV或FC，那就跳过。
                    if (strcmp(NodeList[node_index]["operation"].asCString(), "OP_CONV") != 0 && strcmp(NodeList[node_index]["operation"].asCString(), "OP_FC") != 0)
                    {
                        int effective_provider_index = NodeList[node_index]["AG0_node_index"].asInt();
                        Json::Value Instruction;
                        Instruction["level_index"] = NodeList[consumer_index]["level_index"];
                        Instruction["operation"] = consumer_op;
                        Instruction["node_index"] = consumer_index;
                        Instruction["source"] = AG0_index_in_total;
                        Instruction["destination"] = AG0_index_in_total;
//                        Instruction["relative_length"] = node_offset_inference[AG0_index_in_total];
                        Instruction["relative_length"] = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
                        Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[AG0_index_in_total];
                        Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
//                        int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                        int real_instruction_group_index = instruction_group_index;
                        DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
                    }
                }
                else if (strcmp(consumer_op.c_str(), "OP_POOL") == 0)
                {
                    bool output_visit_flag[100000];
                    int effective_provider_index = NodeList[node_index]["AG0_node_index"].asInt();
                    int ready_input_num = DNNInfo["6_input_cycle_record"][effective_provider_index].size(); // the input of pool
                    int input_element_in_total = NodeList[consumer_index]["input_dim"][1].asInt() * NodeList[consumer_index]["input_dim"][2].asInt() * NodeList[consumer_index]["input_dim"][3].asInt();
                    Json::Value PoolInfo = DNNInfo["5_pool_info"][consumer_index]["pool_info"]["input_index"];
                    for (int j = 0; j < ready_input_num; ++j)
                    {
                        int input_index = DNNInfo["6_input_cycle_record"][effective_provider_index][j].asInt();
                        int associated_output_num = PoolInfo[input_index].size();
                        for (int k = 0; k < associated_output_num; ++k)
                        {
                            int output_index = PoolInfo[input_index][k].asInt();
                            Json::Value Instruction;
                            Instruction["level_index"] = NodeList[consumer_index]["level_index"];
                            Instruction["operation"] = "POOL";
                            Instruction["input_index"] = input_index;
                            Instruction["output_index"] = output_index;
                            Instruction["node_index"] = consumer_index;
                            Instruction["relative_length"] = 1;
                            Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[AG0_index_in_total];
                            Instruction["input_element_in_total"] = input_element_in_total;
                            Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
                            if (output_visit_flag[output_index] == 0) // 如果提前没有访问过，就先把输入向量搬运过去。
                            {
                                output_visit_flag[output_index] = 1;
                                Instruction["operation_type"] = "VM";
                                Instruction["source"] = AG0_index_in_total;
                                Instruction["destination"] = AG0_index_in_total;
                                Instruction["rs_offset"] = input_index * Instruction["element_num"].asInt();
                                Instruction["rd_offset_in_output"] = output_index * Instruction["element_num"].asInt();
                                Instruction["rd_offset"] = input_element_in_total + Instruction["rd_offset_in_output"].asInt();
                            }
                            else
                            {
                                Instruction["operation_type"] = "VVMAX";
                                Instruction["source_1"] = AG0_index_in_total;
                                Instruction["source_2"] = AG0_index_in_total;
                                Instruction["destination"] = AG0_index_in_total;
                                Instruction["rs1_offset"] = input_index * Instruction["element_num"].asInt();
                                Instruction["rs2_offset_in_output"] = output_index * Instruction["element_num"].asInt();
                                Instruction["rs2_offset"] = input_element_in_total + Instruction["rs_offset_in_output"].asInt();
                                Instruction["rd_offset"] = Instruction["rs2_offset"];
                            }
                            int real_instruction_group_index = instruction_group_index;
                            DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
                        }
                    }
                }
                else
                {
                    std::cout << "  not considered post op:" << consumer_op << std::endl;
//                    int effective_provider_index = NodeList[node_index]["AG0_node_index"].asInt();
//                    Json::Value Instruction;
//                    Instruction["level_index"] = NodeList[consumer_index]["level_index"];
//                    Instruction["operation"] = consumer_op;
//                    Instruction["node_index"] = consumer_index;
//                    Instruction["source"] = AG0_index_in_total;
//                    Instruction["destination"] = AG0_index_in_total;
//                    Instruction["relative_length"] = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
//                    Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[AG0_index_in_total];
//                    Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
//                    int real_instruction_group_index = instruction_group_index;
//                    DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
                }
                ScheduleNaiveStage5Slow(DNNInfo, consumer_index, level_index, instruction_group_index);
            }
        }
    }
}


int InferencePipelineSchedule::GetInputChannelFromOutputIndexSlow(Json::Value &DNNInfo, int node_index, int output_index, bool is_last)
{
    //// 这里为什么DNNInfo["5_node_list_augmented"]就很慢
    Json::Value Node = NodeList[node_index];
    Json::Value Params = Node["param"];
    int input_H = Node["input_dim"][2].asInt();
    int input_W = Node["input_dim"][3].asInt();

    int conv_kernel_w = Params["kernel_w"].asInt();
    int conv_kernel_h = Params["kernel_h"].asInt();
    int conv_padding_h0 = Params["pad_h0"].asInt();
    int conv_padding_h1 = Params["pad_h1"].asInt();
    int conv_padding_w0 = Params["pad_w0"].asInt();
    int conv_padding_w1 = Params["pad_w1"].asInt();
    int conv_stride_w = Params["stride_w"].asInt();
    int conv_stride_h = Params["stride_h"].asInt();

    int output_W = floor(float(input_W + conv_padding_w0 + conv_padding_w1 - conv_kernel_w) / float(conv_stride_w)) + 1;
    int output_H = floor(float(input_H + conv_padding_h0 + conv_padding_h1 - conv_kernel_h) / float(conv_stride_h)) + 1;
    int info_output_W = Node["output_dim"][3].asInt();
    int info_output_H = Node["output_dim"][2].asInt();
    if (info_output_W != output_W || info_output_H != output_H)
    {
        std::cout << " Output Size Doesn't Match" << std::endl;
        return -1;
    }

    int i = output_index / output_W;
    int j = output_index % output_W;
    int start_address = i * conv_stride_h * input_W + j *  conv_stride_w;
    if (i != 0)
        start_address -= conv_padding_h0 * input_W;
    if (j != 0)
        start_address -= conv_padding_w0;
    int start_row = start_address / input_W;
    int start_col = start_address % input_W;

    int conv_h_num = conv_kernel_h;
    if (i == 0)
        conv_h_num -= conv_padding_h0;
    else if (i == output_H-1)
        if (start_row + conv_kernel_h > input_H)
            conv_h_num -= conv_padding_h1;

    int conv_w_num = conv_kernel_w;
    if (j == 0)
        conv_w_num -= conv_padding_w0;
    else if (j == output_W-1)
        if (start_col + conv_kernel_w > input_W)
            conv_w_num -= conv_padding_w1;

    int h = 0;
    int w = 0;
    if (is_last)
    {
        h = conv_h_num-1;
        w = conv_w_num-1;
    }
    int position = start_address + w + h * input_W; // input_index
    return position;
}

static int visit_stage6[MAX_NODE] = {0};
void InferencePipelineSchedule::ScheduleNaiveStage6Slow(Json::Value & DNNInfo, int node_index, int level_index, int mode, int instruction_group_index)
{
    // 每次向前跳一步。所以只用检测visit_stage6[node_index]是否等于0即可。
    if (visit_stage6[node_index] != 0)
        return;
    visit_stage6[node_index] = 1;
    int consumer_num = NodeList[node_index]["consumer_num"].asInt();
    if (consumer_num == 0)
    {
        int provider_AG_index = NodeList[node_index]["AG0_index_in_total"].asInt();
        int provider_core = NodeList[node_index]["AG0_core_index"].asInt();
        int output_dim_num_t = NodeList[node_index]["output_dim_num"].asInt();
        int output_element_num = 1;
        for (int k = 0; k < output_dim_num_t; ++k)
        {
            output_element_num *= NodeList[node_index]["output_dim"][k].asInt();
        }
        Json::Value Instruction_st;
        Instruction_st["level_index"] = level_index;
        Instruction_st["level_diff"] = 0;
        Instruction_st["operation"] = "ST-OUTPUT";
        Instruction_st["source"] = provider_AG_index;
        Instruction_st["destination"] = provider_AG_index;
        Json::Value offset_st;
        offset_st["rs_offset"] = 0;
        offset_st["rd_offset_between_inference"] = output_element_num;
        Instruction_st["offset"] = offset_st;
        Instruction_st["element_num"] = output_element_num;
        Instruction_st["instruction_group_index"] = instruction_group_index;
        DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_st);
        return;
    }
    else
    {
        for (int i = 0; i < consumer_num; ++i)
        {
            int consumer_index = NodeList[node_index]["consumer_index"][i].asInt();
            int consumer_level = NodeList[consumer_index]["level_index"].asInt();
            int consumer_core = NodeList[consumer_index]["AG0_core_index"].asInt();
            int provider_core = NodeList[node_index]["AG0_core_index"].asInt();
            int provider_AG_index = NodeList[node_index]["AG0_index_in_total"].asInt();
            // 注意这时的node_index一般都是ReLU，而不是CONV或FC。所以要找到其生产者CONV或FC，所以需要effective_node_index
            int effective_node_index = NodeList[node_index]["AG0_node_index"].asInt();
            int consumer_AG_index = NodeList[consumer_index]["AG0_index_in_total"].asInt();
            switch (mode)
            {
                case 0:
                {
                    if (consumer_level == level_index)
                    {
                        if (NodeList[consumer_index]["AG0_core_index"].asInt() != NodeList[node_index]["AG0_core_index"].asInt() ||
                            NodeList[consumer_index]["AG0_index_in_total"].asInt() != NodeList[node_index]["AG0_index_in_total"].asInt())
                        {
                            if (consumer_core != provider_core)
                            {
//                                std::cout << "[Comm] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                          << "core_" << consumer_core << " node_" << consumer_index << " AG_" << consumer_AG_index << std::endl;
                                Json::Value Instruction_send;
                                Instruction_send["level_index"] = NodeList[consumer_index]["level_index"];
                                Instruction_send["operation"] = "SEND";
                                Instruction_send["stage"] = 6;
                                Instruction_send["to_core"] = consumer_core;
                                Instruction_send["source"] = provider_AG_index;
                                int AG0_index_in_total = NodeList[node_index]["AG0_index_in_total"].asInt();
//                                Instruction_send["relative_length"] = node_offset_inference[AG0_index_in_total];
                                Instruction_send["relative_length"] = DNNInfo["6_input_cycle_record"][effective_node_index].size();
                                Instruction_send["element_num"] = Instruction_send["relative_length"].asInt() * AG_output_element_size[provider_AG_index];
//                                int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                                int real_instruction_group_index = instruction_group_index;
                                Instruction_send["instruction_group_index"] = real_instruction_group_index;
                                DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_send);

                                Json::Value Instruction_recv;
                                Instruction_recv["level_index"] = NodeList[consumer_index]["level_index"];
                                Instruction_recv["operation"] = "RECV";
                                Instruction_recv["stage"] = 6;
                                Instruction_recv["from_core"] = provider_core;
                                Instruction_recv["destination"] = provider_AG_index;
//                                Instruction_recv["relative_length"] = node_offset_inference[NodeList[node_index]["AG0_index_in_total"].asInt()];
                                Instruction_recv["relative_length"] = DNNInfo["6_input_cycle_record"][effective_node_index].size();
                                Instruction_recv["element_num"] = Instruction_recv["relative_length"].asInt() * AG_output_element_size[provider_AG_index];
                                Instruction_recv["instruction_group_index"] = real_instruction_group_index;
                                DNNInfo["6_post_instruction_ir"][real_instruction_group_index]["core_list"][consumer_core]["instruction_ir_list"].append(Instruction_recv);
                            }
                        }
                        else if (NodeList[consumer_index]["copy_offset_flag"].asInt() == 1)
                        {
                            std::cout << "Need To Copy The Result" << std::endl;
                        }
                    }
                    break;
                }
                case 1:
                {
                    if (consumer_level-level_index == 1)
                    {
                        if (strcmp(NodeList[consumer_index]["operation"].asCString(),"OP_CONV") == 0)
                        {
                            int comm_num = DNNInfo["6_recv_info"]["node_list"][consumer_index].size();
                            for (int j = 0; j < comm_num; ++j)
                            {
                                int recv_core = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["core_index"].asInt();
                                int recv_replication = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["replication_index"].asInt();
                                int recv_AG_index = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["AG_index"].asInt();
                                if (strcmp(NodeList[node_index]["operation"].asCString(),"OP_INPUT") == 0) // Load Data From Global Memory
                                {
                                    int effective_consumer_index = NodeList[consumer_index]["effective_node_index"].asInt();
                                    int first_output_index = DNNInfo["2_AG_partition"][effective_consumer_index]["replication"][recv_replication]["input_cycle_this_start"].asInt();
                                    int last_output_index = DNNInfo["2_AG_partition"][effective_consumer_index]["replication"][recv_replication]["input_cycle_this_end"].asInt();
                                    int channel_num = GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, last_output_index, 1) - GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, first_output_index, 0);
                                    int channel_length = NodeList[consumer_index]["param"]["input_channel"].asInt();
                                    int input_dim_num = NodeList[node_index]["output_dim_num"].asInt();
                                    int input_element_num = 1;
                                    for (int k = 0; k < input_dim_num; ++k)
                                    {
                                        input_element_num *= NodeList[node_index]["output_dim"][k].asInt();
                                    }
                                    Json::Value Instruction_ld;
                                    Instruction_ld["level_index"] = 0;
                                    Instruction_ld["level_diff"] = 0;
                                    Instruction_ld["operation"] = "LD-INPUT";
                                    Instruction_ld["source"] = 0;
                                    Instruction_ld["destination"] = recv_AG_index;
                                    Json::Value offset_ld;
                                    offset_ld["rs_offset_between_inference"] = input_element_num;
                                    offset_ld["rs_offset_in_inference"] = channel_length * GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, first_output_index, 0);;
                                    offset_ld["rd_offset"] = 0;
                                    Instruction_ld["offset"] = offset_ld;
                                    Instruction_ld["element_num"] = channel_num * channel_length;
                                    Instruction_ld["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][recv_core]["instruction_ir_list"].append(Instruction_ld);
                                }
                                else if (recv_core != provider_core)
                                {
//                                    std::cout << "[Comm] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                              << "core_" << recv_core << " node_" << consumer_index << " AG_" << recv_AG_index << " replication_" << recv_replication << std::endl;
                                    int effective_consumer_index = NodeList[consumer_index]["effective_node_index"].asInt();
                                    int first_output_index = DNNInfo["2_AG_partition"][effective_consumer_index]["replication"][recv_replication]["input_cycle_this_start"].asInt();
                                    int last_output_index = DNNInfo["2_AG_partition"][effective_consumer_index]["replication"][recv_replication]["input_cycle_this_end"].asInt();
//                                    std::cout << " start_position:" << GetInputChannelFromOutputIndex(DNNInfo, consumer_index, first_output_index, 0) << std::endl;
//                                    std::cout << " end_position:" << GetInputChannelFromOutputIndex(DNNInfo, consumer_index, last_output_index, 1) << std::endl;
                                    int channel_num = GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, last_output_index, 1) - GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, first_output_index, 0);
                                    int channel_length = NodeList[consumer_index]["param"]["input_channel"].asInt();

                                    Json::Value Instruction_send;
                                    Instruction_send["level_index"] = level_index;
                                    Instruction_send["operation"] = "SEND";
                                    Instruction_send["stage"] = 7;
                                    Instruction_send["to_core"] = recv_core;
                                    Instruction_send["source"] = provider_AG_index;
                                    Instruction_send["offset"] = channel_length * GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, first_output_index, 0);
                                    Instruction_send["element_num"] = channel_num * channel_length;
                                    Instruction_send["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_send);

                                    Json::Value Instruction_recv;
                                    Instruction_recv["level_index"] = level_index;
                                    Instruction_recv["operation"] = "RECV";
                                    Instruction_recv["stage"] = 7;
                                    Instruction_recv["from_core"] = provider_core;
                                    Instruction_recv["destination"] = recv_AG_index;
                                    Instruction_recv["element_num"] = channel_num * channel_length;
                                    Instruction_recv["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][recv_core]["instruction_ir_list"].append(Instruction_recv);
                                }
                            }
                        }
                        else if (strcmp(NodeList[consumer_index]["operation"].asCString(),"OP_FC") == 0)
                        {
                            int comm_num = DNNInfo["6_recv_info"]["node_list"][consumer_index].size();
                            for (int j = 0; j < comm_num; ++j)
                            {
                                if (strcmp(NodeList[NodeList[consumer_index]["provider_index"][0].asInt()]["operation"].asCString(),"OP_INPUT") == 0)
                                    continue;
                                int recv_core = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["core_index"].asInt();
                                int recv_AG_index = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["AG_index"].asInt();
                                if (strcmp(NodeList[node_index]["operation"].asCString(),"OP_INPUT") == 0) // Load Data From Global Memory
                                {
                                    int start_offset = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["start_offset_element"].asInt();
                                    int recv_element = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["recv_element"].asInt();
                                    int input_dim_num = NodeList[node_index]["output_dim_num"].asInt();
                                    int input_element_num = 1;
                                    for (int k = 0; k < input_dim_num; ++k)
                                    {
                                        input_element_num *= NodeList[node_index]["output_dim"][k].asInt();
                                    }

                                    Json::Value Instruction_ld;
                                    Instruction_ld["level_index"] = 0;
                                    Instruction_ld["level_diff"] = 0;
                                    Instruction_ld["operation"] = "LD-INPUT";
                                    Instruction_ld["source"] = 0;
                                    Instruction_ld["destination"] = recv_AG_index;
                                    Json::Value offset_ld;
                                    offset_ld["rs_offset_between_inference"] = input_element_num;
                                    offset_ld["rs_offset_in_inference"] = start_offset;
                                    offset_ld["rd_offset"] = 0;
                                    Instruction_ld["offset"] = offset_ld;
                                    Instruction_ld["element_num"] = recv_element;
                                    Instruction_ld["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][recv_core]["instruction_ir_list"].append(Instruction_ld);
                                }
                                else if (recv_core != provider_core)
                                {
//                                    std::cout << "[Comm] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                              << "core_" << recv_core << " node_" << consumer_index << " AG_" << recv_AG_index  << std::endl;
                                    int start_offset = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["start_offset_element"].asInt();
                                    int recv_element = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["recv_element"].asInt();
//                                    std::cout << " start_offset:" << start_offset << " recv_element:" << recv_element << std::endl;

                                    Json::Value Instruction_send;
                                    Instruction_send["level_index"] = level_index;
                                    Instruction_send["operation"] = "SEND";
                                    Instruction_send["stage"] = 7;
                                    Instruction_send["to_core"] = recv_core;
                                    Instruction_send["source"] = provider_AG_index;
                                    Instruction_send["offset"] = start_offset;
                                    Instruction_send["element_num"] = recv_element;
                                    Instruction_send["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_send);

                                    Json::Value Instruction_recv;
                                    Instruction_recv["level_index"] = level_index;
                                    Instruction_recv["operation"] = "RECV";
                                    Instruction_recv["stage"] = 7;
                                    Instruction_recv["from_core"] = provider_core;
                                    Instruction_recv["destination"] = recv_AG_index;
                                    Instruction_recv["element_num"] = recv_element;
                                    Instruction_recv["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][recv_core]["instruction_ir_list"].append(Instruction_recv);
                                }
                            }
                        }
                        else
                        {
                            if (consumer_core != provider_core)
                            {
//                                std::cout << "[Comm] from core_" << provider_core <<" node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                          << "core_" << consumer_core<<" node_" << consumer_index  << " AG_" << consumer_AG_index << std::endl;
                                int output_dim_num = NodeList[node_index]["output_dim_num"].asInt();
                                int output_dim = 1;
                                for (int j = 0; j < output_dim_num; ++j)
                                {
                                    output_dim *= NodeList[node_index]["output_dim"][j].asInt();
                                }
//                                std::cout << output_dim << std::endl;

                                Json::Value Instruction_send;
                                Instruction_send["level_index"] = level_index;
                                Instruction_send["operation"] = "SEND";
                                Instruction_send["stage"] = 7;
                                Instruction_send["to_core"] = consumer_core;
                                Instruction_send["source"] = provider_AG_index;
                                Instruction_send["offset"] = 0;
                                Instruction_send["element_num"] = output_dim;
                                Instruction_send["instruction_group_index"] = instruction_group_index;
                                DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_send);

                                Json::Value Instruction_recv;
                                Instruction_recv["level_index"] = level_index;
                                Instruction_recv["operation"] = "RECV";
                                Instruction_recv["stage"] = 7;
                                Instruction_recv["from_core"] = provider_core;
                                Instruction_recv["destination"] = provider_AG_index; // the same to send_source
                                Instruction_recv["element_num"] = output_dim;
                                Instruction_recv["instruction_group_index"] = instruction_group_index;
                                DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][consumer_core]["instruction_ir_list"].append(Instruction_recv);
                            }
                        }
                    }
                    else if  (consumer_level-level_index > 1)
                    {
                        if (strcmp(NodeList[consumer_index]["operation"].asCString(),"OP_CONV") == 0)
                        {
                            int comm_num = DNNInfo["6_recv_info"]["node_list"][consumer_index].size();
                            for (int j = 0; j < comm_num; ++j)
                            {
                                int recv_core = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["core_index"].asInt();
                                int recv_replication = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["replication_index"].asInt();
                                int recv_AG_index = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["AG_index"].asInt();
                                if (recv_core != provider_core)
                                {
                                    if (strcmp(NodeList[NodeList[consumer_index]["provider_index"][0].asInt()]["operation"].asCString(),"OP_INPUT") == 0)
                                        continue;
//                                    std::cout << "[Cache] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                              << "core_" << recv_core << " node_" << consumer_index << " AG_" << recv_AG_index << " replication_" << recv_replication << std::endl;
                                    int effective_consumer_index = NodeList[consumer_index]["effective_node_index"].asInt();
                                    int first_output_index = DNNInfo["2_AG_partition"][effective_consumer_index]["replication"][recv_replication]["input_cycle_this_start"].asInt();
                                    int last_output_index = DNNInfo["2_AG_partition"][effective_consumer_index]["replication"][recv_replication]["input_cycle_this_end"].asInt();
//                                    std::cout << " start_position:" << GetInputChannelFromOutputIndex(DNNInfo, consumer_index, first_output_index, 0) << std::endl;
//                                    std::cout << " end_position:" << GetInputChannelFromOutputIndex(DNNInfo, consumer_index, last_output_index, 1) << std::endl;
                                    int channel_num = GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, last_output_index, 1) - GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, first_output_index, 0);
                                    int channel_length = NodeList[consumer_index]["param"]["input_channel"].asInt();

                                    Json::Value Instruction_st;
                                    Instruction_st["level_index"] = level_index;
                                    Instruction_st["level_diff"] = consumer_level - level_index;
                                    Instruction_st["operation"] = "ST";
                                    Instruction_st["source"] = provider_AG_index;
                                    Instruction_st["destination"] = recv_AG_index;
                                    Json::Value offset_st;
                                    offset_st["rs_offset"] = channel_length * GetInputChannelFromOutputIndexSlow(DNNInfo, consumer_index, first_output_index, 0);
                                    offset_st["rd_offset_unit"] = (channel_num * channel_length);
                                    Instruction_st["offset"] = offset_st;
                                    Instruction_st["element_num"] = channel_num * channel_length;
                                    Instruction_st["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_st);

                                    Json::Value Instruction_ld;
                                    Instruction_ld["level_index"] = consumer_level-1;
                                    Instruction_ld["level_diff"] = consumer_level - level_index;
                                    Instruction_ld["operation"] = "LD";
                                    Instruction_ld["source"] = recv_AG_index;
                                    Instruction_ld["destination"] = recv_AG_index;
                                    Json::Value offset_ld;
                                    offset_ld["rs_offset_unit"] = (channel_num * channel_length);
                                    offset_ld["rd_offset"] = 0;
                                    Instruction_ld["offset"] = offset_ld;
                                    Instruction_ld["element_num"] = channel_num * channel_length;
                                    Instruction_ld["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][recv_core]["instruction_ir_list"].append(Instruction_ld);
                                }
                            }
                        }
                        else if (strcmp(NodeList[consumer_index]["operation"].asCString(),"OP_FC") == 0)
                        {
                            int comm_num = DNNInfo["6_recv_info"]["node_list"][consumer_index].size();
                            for (int j = 0; j < comm_num; ++j)
                            {
                                if (strcmp(NodeList[NodeList[consumer_index]["provider_index"][0].asInt()]["operation"].asCString(),"OP_INPUT") == 0)
                                    continue;
                                int recv_core = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["core_index"].asInt();
                                int recv_AG_index = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["AG_index"].asInt();
                                if (recv_core != provider_core)
                                {
//                                    std::cout << "[Cache] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                              << "core_" << recv_core << " node_" << consumer_index << " AG_" << recv_AG_index  << std::endl;
                                    int start_offset = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["start_offset_element"].asInt();
                                    int recv_element = DNNInfo["6_recv_info"]["node_list"][consumer_index][j]["recv_element"].asInt();
//                                    std::cout << " start_offset:" << start_offset << " recv_element:" << recv_element << std::endl;

                                    Json::Value Instruction_st;
                                    Instruction_st["level_index"] = level_index;
                                    Instruction_st["level_diff"] = consumer_level - level_index;
                                    Instruction_st["operation"] = "ST";
                                    Instruction_st["source"] = provider_AG_index;
                                    Instruction_st["destination"] = recv_AG_index;
                                    Json::Value offset_st;
                                    offset_st["rs_offset"] = start_offset;
                                    offset_st["rd_offset_unit"] = recv_element;
                                    Instruction_st["offset"] = offset_st;
                                    Instruction_st["element_num"] = recv_element;
                                    Instruction_st["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_st);

                                    Json::Value Instruction_ld;
                                    Instruction_ld["level_index"] = consumer_level-1;
                                    Instruction_ld["level_diff"] = consumer_level - level_index;
                                    Instruction_ld["operation"] = "LD";
                                    Instruction_ld["source"] = recv_AG_index;
                                    Instruction_ld["destination"] = recv_AG_index;
                                    Json::Value offset_ld;
                                    offset_ld["rs_offset_unit"] = recv_element;
                                    offset_ld["rd_offset"] = 0;
                                    Instruction_ld["offset"] = offset_ld;
                                    Instruction_ld["element_num"] = recv_element;
                                    Instruction_ld["instruction_group_index"] = instruction_group_index;
                                    DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][recv_core]["instruction_ir_list"].append(Instruction_ld);
                                }
                            }
                        }
                        else
                        {
                            if (consumer_core != provider_core)
                            {
//                                std::cout << "[Cache] from core_" << provider_core <<" node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                          << "core_" << consumer_core<<" node_" << consumer_index  << " AG_" << consumer_AG_index << std::endl;
                                int output_dim_num = NodeList[node_index]["output_dim_num"].asInt();
                                int output_dim = 1;
                                for (int j = 0; j < output_dim_num; ++j)
                                {
                                    output_dim *= NodeList[node_index]["output_dim"][j].asInt();
                                }
//                                std::cout << output_dim << std::endl;

                                Json::Value Instruction_st;
                                Instruction_st["level_index"] = level_index;
                                Instruction_st["level_diff"] = consumer_level - level_index;
                                Instruction_st["operation"] = "ST";
                                Instruction_st["source"] = provider_AG_index;
                                Instruction_st["destination"] = provider_AG_index;
                                Json::Value offset_st;
                                offset_st["rs_offset"] = 0;
                                offset_st["rd_offset_unit"] = output_dim;
                                Instruction_st["offset"] = offset_st;
                                Instruction_st["element_num"] = output_dim;
                                Instruction_st["instruction_group_index"] = instruction_group_index;
                                DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_st);

                                Json::Value Instruction_ld;
                                Instruction_ld["level_index"] = consumer_level-1;
                                Instruction_ld["level_diff"] = consumer_level - level_index;
                                Instruction_ld["operation"] = "LD";
                                Instruction_ld["source"] = provider_AG_index;
                                Instruction_ld["destination"] = provider_AG_index; // the same to send_source
                                Json::Value offset_ld;
                                offset_ld["rs_offset_unit"] = output_dim;
                                offset_ld["rd_offset"] = 0;
                                Instruction_ld["offset"] = offset_ld;
                                Instruction_ld["element_num"] = output_dim;
                                Instruction_ld["instruction_group_index"] = instruction_group_index;
                                DNNInfo["6_post_instruction_ir"][instruction_group_index]["core_list"][consumer_core]["instruction_ir_list"].append(Instruction_ld);
                            }
                        }
                    }
                }
            }
            ScheduleNaiveStage6Slow(DNNInfo, consumer_index, consumer_level, mode, instruction_group_index);
        }
    }
}


void InferencePipelineSchedule::AddSeparateLineSlow(Json::Value & DNNInfo, int instruction_group_index)
{
    for (int i = 0; i < core_num; ++i)
    {
        Json::Value Instruction_sep;
        Instruction_sep["operation"] = "sep";
        DNNInfo["6_base_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_sep);
    }
}

void InferencePipelineSchedule::FillTheWholeInstructionGroupSlow(Json::Value & DNNInfo)
{
    // Clean And Fill node_offset_inference
    for (int i = 0; i < MAX_AG; ++i)
        node_offset_inference[i] = 0;
    int AG_num = DNNInfo["3_hierarchy_map"]["whole"].size();
    for (int i = 0; i < AG_num; ++i)
    {
        int AG_index_in_total = DNNInfo["3_hierarchy_map"]["whole_index"][i].asInt();
        int input_cycle_this_replication = DNNInfo["3_hierarchy_map"]["whole"][i][0]["input_cycle_this_replication"].asInt();
        node_offset_inference[AG_index_in_total] = input_cycle_this_replication;
    }

    // Clean And Fill DNNInfo["6_input_cycle_record"]
    DNNInfo["6_input_cycle_record"].resize(0);
    int effective_node_index = DNNInfo["2_AG_partition"].size();
    for (int i = 0; i < effective_node_index; ++i)
    {
        int node_index = DNNInfo["2_AG_partition"][i]["index"].asInt();
        int input_cycle_in_total = DNNInfo["2_AG_partition"][i]["input_cycle_in_total"].asInt();
        for (int j = 0; j < input_cycle_in_total; ++j)
        {
            DNNInfo["6_input_cycle_record"][node_index].append(j);
        }
    }
}

int InferencePipelineSchedule::GetEffectiveInstructionGroupNumSlow(Json::Value & DNNInfo)
{
    int effective_instruction_group_num = 0;
    int AG_num_in_total = DNNInfo["3_hierarchy_map"]["whole"].size();
    for (int i = 0; i < AG_num_in_total; ++i)
    {
        // 得到整个结构最终instruction_group_num
        int AG_index = DNNInfo["3_hierarchy_map"]["whole_index"][i].asInt();
        int AG_instruction_group_num = DNNInfo["3_hierarchy_map"]["whole"][i][0]["instruction_group_num"].asInt();
//        std::cout << "AG_index:" << AG_index << "    instruction_group_num:" << AG_instruction_group_num << std::endl;
        if (AG_instruction_group_num > effective_instruction_group_num)
            effective_instruction_group_num = AG_instruction_group_num;
        // 为每个CONV或FC节点生成instruction_group_num
        int node_index = DNNInfo["3_hierarchy_map"]["whole"][i][0]["node_index"].asInt();
        if (!NodeList[node_index].isMember("instruction_group_num"))
        {
            NodeList[node_index]["instruction_group_num"] = AG_instruction_group_num;
        }
        else if (NodeList[node_index]["instruction_group_num"].asInt() < AG_instruction_group_num)
        {
            NodeList[node_index]["instruction_group_num"] = AG_instruction_group_num;
        }
    }
    return effective_instruction_group_num;
}

//extern std::map <int, std::vector<int>> pool_info;

void InferencePipelineSchedule::ResetPostStartAndEndAddress(int origin_length, int assumed_core_num)
{
    post_start_address.clear();
    post_end_address.clear();
    std::vector<int> output_core_allocated;
    for (int i = 0; i < assumed_core_num; ++i)
        output_core_allocated.push_back(ceil(float(origin_length) / float(assumed_core_num)));
    int minus_num = ceil(float(origin_length) / float(assumed_core_num)) * assumed_core_num - origin_length;
    for (int i = 0; i < minus_num; ++i)
        output_core_allocated[assumed_core_num-1-i] -= 1;
    int start_address;
    int end_address = -1;
    for (int i = 0; i < assumed_core_num; ++i)
    {
        start_address = end_address + 1;
        end_address = start_address + output_core_allocated[i] - 1;
        post_start_address.push_back(start_address);
        post_end_address.push_back(end_address);
    }
}

void InferencePipelineSchedule::ScheduleNaiveScheduleOnePostOperationSlow(Json::Value &DNNInfo,int instruction_group_index, int post_node_index)
{
    int appointed_core_num = 23;
//    std::cout << post_node_index << std::endl;
    Json::Value PostOperationNode = NodeList[post_node_index];
    int level_index = PostOperationNode["level_index"].asInt();
    int node_index = PostOperationNode["node_index"].asInt();
    int copy_offset_flag = PostOperationNode["copy_offset_flag"].asInt();

    if (strcmp(PostOperationNode["operation"].asCString(), "OP_POOL") == 0)
    {
        int output_channel_length = PostOperationNode["output_dim"][1].asInt();
        int output_channel_num_total = PostOperationNode["output_dim"][2].asInt() * PostOperationNode["output_dim"][3].asInt(); // == input_sliding_window_num
        ResetPostStartAndEndAddress(output_channel_num_total, appointed_core_num);
        int load_offset = 0;
        for (int i = 0; i < appointed_core_num; ++i)
        {
            int output_channel_start = post_start_address[i];
            int output_channel_end = post_end_address[i];
            int input_channel_start = GetInputChannelFromOutputIndexSlow(DNNInfo, post_node_index, output_channel_start, 0);
            int input_channel_end = GetInputChannelFromOutputIndexSlow(DNNInfo, post_node_index, output_channel_end, 1);

            Json::Value Instruction_ld;
            Instruction_ld["level_index"] = NodeList[post_node_index]["level_index"];
            Instruction_ld["level_diff"] = 0;
            Instruction_ld["operation"] = "POST-LD";
            Instruction_ld["source"] = NodeList[post_node_index]["AG0_index_in_total"];
            Instruction_ld["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
            Json::Value offset_ld;
            offset_ld["rs_offset"] = input_channel_start * output_channel_length; // POOL:input_channel_length == output_channel_length
            offset_ld["rd_offset"] = 0;
            Instruction_ld["offset"] = offset_ld;
            Instruction_ld["element_num"] = (input_channel_end - input_channel_start + 1) * output_channel_length;
            load_offset += Instruction_ld["element_num"].asInt();
            Instruction_ld["instruction_group_index"] = instruction_group_index;
            DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_ld);

            Json::Value PoolInfo = DNNInfo["5_pool_info"][post_node_index]["pool_info"]["output_index"];
            for (int j = output_channel_start; j <= output_channel_end; ++j)
            {
                int associated_input_num = PoolInfo[j].size();
//                int associated_input_num = pool_info[j].size();
                for (int k = 0; k < associated_input_num; ++k)
                {
                    int output_channel_index = j;
                    int input_channel_index = PoolInfo[j][k].asInt();
//                    int input_channel_index = pool_info[j][k];
                    Json::Value Instruction_pool;
                    Instruction_pool["node_index"] = PostOperationNode["node_index"];
                    Instruction_pool["level_index"] = PostOperationNode["level_index"];
                    Instruction_pool["operation"] = "POST-POOL";
                    Instruction_pool["input_index"] = input_channel_index;
                    Instruction_pool["output_index"] = output_channel_index;
                    Instruction_pool["element_num"] = output_channel_length;
                    Instruction_pool["copy_offset_flag"] = PostOperationNode["copy_offset_flag"];
                    if (k == 0) // 如果提前没有访问过，就先把输入向量搬运过去。
                    {
                        Instruction_pool["operation_type"] = "VM";
                        Instruction_pool["source"] = NodeList[post_node_index]["AG0_index_in_total"];
                        Instruction_pool["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
                        Instruction_pool["rs_offset"] = (input_channel_index - input_channel_start) * output_channel_length;
                        Instruction_pool["rd_offset"] = (output_channel_index - output_channel_start) * output_channel_length;
                    }
                    else
                    {
                        Instruction_pool["operation_type"] = "VVMAX";
                        Instruction_pool["source_1"] = NodeList[post_node_index]["AG0_index_in_total"];
                        Instruction_pool["source_2"] = NodeList[post_node_index]["AG0_index_in_total"];
                        Instruction_pool["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
                        Instruction_pool["rs1_offset"] = (input_channel_index - input_channel_start) * output_channel_length;
                        Instruction_pool["rs2_offset"] = (output_channel_index - output_channel_start) * output_channel_length;
                        Instruction_pool["rd_offset"] = Instruction_pool["rs2_offset"];
                    }
                    DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_pool);
                }
            }

            Json::Value Instruction_st;
            Instruction_st["level_index"] = level_index;
            Instruction_st["level_diff"] = 0;
            Instruction_st["operation"] = "POST-ST";
            Instruction_st["source"] = NodeList[post_node_index]["AG0_index_in_total"];
            Instruction_st["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
            Json::Value offset_st;
            offset_st["rs_offset"] = load_offset;
            offset_st["rd_offset"] =  output_channel_start * output_channel_length; // POOL:input_channel_length == output_channel_length
            Instruction_st["offset"] = offset_st;
            Instruction_st["element_num"] = (output_channel_end - output_channel_start + 1) * output_channel_length;
            Instruction_st["instruction_group_index"] = instruction_group_index;
            DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_st);
        }
    }
    else if (strcmp(PostOperationNode["operation"].asCString(), "OP_RELU") == 0)
    {
        int output_channel_length = PostOperationNode["output_dim"][1].asInt();
        int output_channel_num_total = PostOperationNode["output_dim"][2].asInt() * PostOperationNode["output_dim"][3].asInt();
        ResetPostStartAndEndAddress(output_channel_num_total, appointed_core_num);
        for (int i = 0; i < appointed_core_num; ++i)
        {
            int input_channel_start = post_start_address[i];
            int input_channel_end = post_end_address[i];
            int load_offset = 0;
            std::vector<int> load_address;


            Json::Value Instruction_ld;
            Instruction_ld["level_index"] = level_index;
            Instruction_ld["level_diff"] = 0;
            Instruction_ld["operation"] = "POST-LD";
            Instruction_ld["source"] = NodeList[post_node_index]["AG0_index_in_total"];
            Instruction_ld["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
            Json::Value offset_ld;
            offset_ld["rs_offset"] = input_channel_start * output_channel_length;
            offset_ld["rd_offset"] = load_offset;
            Instruction_ld["offset"] = offset_ld;
            Instruction_ld["element_num"] = (input_channel_end - input_channel_start + 1) * output_channel_length;
            load_address.push_back(load_offset);
            load_offset += Instruction_ld["element_num"].asInt();
            Instruction_ld["instruction_group_index"] = instruction_group_index;
            DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_ld);


            for (int k = input_channel_start; k <= input_channel_end; ++k)
            {
                Json::Value Instruction_elt;
                Instruction_elt["level_index"] = level_index;
                Instruction_elt["level_diff"] = 0;
                Instruction_elt["operation"] = "POST-RELU";
                Instruction_elt["source"] = NodeList[post_node_index]["AG0_index_in_total"];
                Instruction_elt["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
                Json::Value offset_elt;
                offset_elt["rs_offset"] = (k-input_channel_start) * output_channel_length;
                offset_elt["rd_offset"] = load_offset + (k-input_channel_start) * output_channel_length;
                Instruction_elt["offset"] = offset_elt;
                Instruction_elt["element_num"] = output_channel_length;
                Instruction_elt["instruction_group_index"] = instruction_group_index;
                DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_elt);
            }

            Json::Value Instruction_st;
            Instruction_st["level_index"] = level_index;
            Instruction_st["level_diff"] = 0;
            Instruction_st["operation"] = "POST-ST";
            Instruction_st["source"] = NodeList[post_node_index]["AG0_index_in_total"];
            Instruction_st["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
            Json::Value offset_st;
            offset_st["rs_offset"] = load_offset; // 最终该load_offset就是load全部元素量
            offset_st["rd_offset"] = input_channel_start * output_channel_length;
            Instruction_st["offset"] = offset_st;
            Instruction_st["element_num"] = (input_channel_end - input_channel_start + 1) * output_channel_length;
            Instruction_st["instruction_group_index"] = instruction_group_index;
            DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_st);
        }
    }
    else if (strcmp(PostOperationNode["operation"].asCString(), "OP_ELTWISE") == 0)
    {
        int output_channel_length = PostOperationNode["output_dim"][1].asInt();
        int output_channel_num_total = PostOperationNode["output_dim"][2].asInt() * PostOperationNode["output_dim"][3].asInt();
        ResetPostStartAndEndAddress(output_channel_num_total, appointed_core_num);
        for (int i = 0; i < appointed_core_num; ++i)
        {
            int input_channel_start = post_start_address[i];
            int input_channel_end = post_end_address[i];
            int load_offset = 0;
            std::vector<int> load_address;
            for (int j = 0; j < PostOperationNode["provider_num"].asInt(); ++j)
            {
                int provider_index = PostOperationNode["provider_index"][j].asInt();
                int provider_channel_length = NodeList[provider_index]["output_dim"][1].asInt();
                Json::Value Instruction_ld;
                Instruction_ld["level_index"] = level_index;
                Instruction_ld["level_diff"] = 0;
                Instruction_ld["operation"] = "POST-LD";
                Instruction_ld["source"] = NodeList[provider_index]["AG0_index_in_total"];
                Instruction_ld["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
                Json::Value offset_ld;
                offset_ld["rs_offset"] = input_channel_start * provider_channel_length;
                offset_ld["rd_offset"] = load_offset;
                Instruction_ld["offset"] = offset_ld;
                Instruction_ld["element_num"] = (input_channel_end - input_channel_start + 1) * provider_channel_length;
                load_address.push_back(load_offset);
                load_offset += Instruction_ld["element_num"].asInt();
                Instruction_ld["instruction_group_index"] = instruction_group_index;
                DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_ld);
            }

            for (int j = 1; j < PostOperationNode["provider_num"].asInt(); ++j)
            {
                int provider_index = PostOperationNode["provider_index"][j].asInt();
                int provider_channel_length = NodeList[provider_index]["output_dim"][1].asInt();
                for (int k = input_channel_start; k <= input_channel_end; ++k)
                {
                    Json::Value Instruction_elt;
                    Instruction_elt["level_index"] = level_index;
                    Instruction_elt["level_diff"] = 0;
                    Instruction_elt["operation"] = "POST-VECTOR";
                    Instruction_elt["source_1"] = NodeList[post_node_index]["AG0_index_in_total"];
                    Instruction_elt["source_2"] = NodeList[post_node_index]["AG0_index_in_total"];
                    Instruction_elt["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
                    Json::Value offset_elt;
                    if (j == 1)
                        offset_elt["rs1_offset"] = load_address[0] + (k-input_channel_start) * output_channel_length;
                    else
                        offset_elt["rs1_offset"] = load_offset + (k-input_channel_start) * output_channel_length;
                    offset_elt["rs2_offset"] = load_address[j] + (k-input_channel_start) * output_channel_length;
                    offset_elt["rd_offset"] = load_offset + (k-input_channel_start) * output_channel_length;
                    Instruction_elt["offset"] = offset_elt;
                    Instruction_elt["element_num"] = provider_channel_length;
                    Instruction_elt["instruction_group_index"] = instruction_group_index;
                    DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_elt);
                }
            }

            Json::Value Instruction_st;
            Instruction_st["level_index"] = level_index;
            Instruction_st["level_diff"] = 0;
            Instruction_st["operation"] = "POST-ST";
            Instruction_st["source"] = NodeList[post_node_index]["AG0_index_in_total"];
            Instruction_st["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
            Json::Value offset_st;
            offset_st["rs_offset"] = load_offset; // 最终该load_offset就是load全部元素量
            offset_st["rd_offset"] = input_channel_start * output_channel_length;
            Instruction_st["offset"] = offset_st;
            Instruction_st["element_num"] = (input_channel_end - input_channel_start + 1) * output_channel_length;
            Instruction_st["instruction_group_index"] = instruction_group_index;
            DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_st);
        }
    }
    else if (strcmp(PostOperationNode["operation"].asCString(), "OP_CONCAT") == 0)
    {
        int output_channel_length = PostOperationNode["output_dim"][1].asInt();
        int output_channel_num_total = PostOperationNode["output_dim"][2].asInt() * PostOperationNode["output_dim"][3].asInt();
        ResetPostStartAndEndAddress(output_channel_num_total, appointed_core_num);
        int store_offset = 0;
        for (int i = 0; i < appointed_core_num; ++i)
        {
            // For CONCAT, input_channel_index == output_channel_index
            int input_channel_start = post_start_address[i];
            int input_channel_end = post_end_address[i];
            int load_total = 0;
            int load_offset = 0;
            for (int j = 0; j < PostOperationNode["provider_num"].asInt(); ++j)
            {
                int provider_index = PostOperationNode["provider_index"][j].asInt();
                int provider_channel_length = NodeList[provider_index]["output_dim"][1].asInt();
                Json::Value Instruction_ld;
                Instruction_ld["level_index"] = level_index;
                Instruction_ld["level_diff"] = 0;
                Instruction_ld["operation"] = "POST-LD";
                Instruction_ld["source"] = NodeList[provider_index]["AG0_index_in_total"];
                Instruction_ld["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
                Json::Value offset_ld;
                offset_ld["rs_offset"] = input_channel_start * provider_channel_length;
                offset_ld["rd_offset"] = load_offset;
                Instruction_ld["offset"] = offset_ld;
                Instruction_ld["element_num"] = (input_channel_end - input_channel_start + 1) * provider_channel_length;
                load_offset += Instruction_ld["element_num"].asInt();
                Instruction_ld["instruction_group_index"] = instruction_group_index;
                DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_ld);
            }

            int source_offset = 0;
            int destination_offset = 0;
            for (int j = 0; j < PostOperationNode["provider_num"].asInt(); ++j)
            {
                int provider_index = PostOperationNode["provider_index"][j].asInt();
                int provider_channel_length = NodeList[provider_index]["output_dim"][1].asInt();
                for (int k = input_channel_start; k <= input_channel_end; ++k)
                {
                    Json::Value Instruction_vm;
                    Instruction_vm["level_index"] = level_index;
                    Instruction_vm["level_diff"] = 0;
                    Instruction_vm["operation"] = "POST-VM";
                    Instruction_vm["source"] = NodeList[post_node_index]["AG0_index_in_total"];
                    Instruction_vm["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
                    Json::Value offset_vm;
                    offset_vm["rs_offset"] = source_offset;
                    offset_vm["rd_offset"] = (k-input_channel_start) * output_channel_length + destination_offset + (input_channel_end-input_channel_start+1)*output_channel_length;
                    Instruction_vm["offset"] = offset_vm;
                    Instruction_vm["element_num"] = provider_channel_length;
                    source_offset += Instruction_vm["element_num"].asInt();
                    Instruction_vm["instruction_group_index"] = instruction_group_index;
                    DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_vm);
                }
                destination_offset += provider_channel_length;
            }

            Json::Value Instruction_st;
            Instruction_st["level_index"] = level_index;
            Instruction_st["level_diff"] = 0;
            Instruction_st["operation"] = "POST-ST";
            Instruction_st["source"] = NodeList[post_node_index]["AG0_index_in_total"];
            Instruction_st["destination"] = NodeList[post_node_index]["AG0_index_in_total"];
            Json::Value offset_st;
            offset_st["rs_offset"] = load_offset; // 最终该load_offset就是load全部元素量
            offset_st["rd_offset"] = store_offset;
            Instruction_st["offset"] = offset_st;
            Instruction_st["element_num"] = (input_channel_end - input_channel_start + 1) * output_channel_length;
            store_offset += Instruction_st["element_num"].asInt();
            Instruction_st["instruction_group_index"] = instruction_group_index;
            DNNInfo["6_post_multi_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_st);
        }
    }
}

//void InferencePipelineSchedule::ScheduleNaivePickOnePostOperation(Json::Value &DNNInfo)
//{
//    std::set <int> complete_node;
//    std::set <int> wait_node;
//    std::set <int> ready_node;
//    for (int i = 0; i < node_num; ++i)
//    {
//        Json::Value Node = NodeList[i];
//        if (strcmp(Node["operation"].asCString(), "OP_INPUT") == 0 || strcmp(Node["operation"].asCString(), "OP_CONV") == 0 || strcmp(Node["operation"].asCString(), "OP_FC") == 0)
//        {
//            complete_node.insert(i);
//        }
//        else if(strcmp(Node["operation"].asCString(), "OP_RELU") == 0)
//        {
//            int provider_node_index = Node["provider_index"][0].asInt();
//            if (strcmp(NodeList[provider_node_index]["operation"].asCString(), "OP_CONV") == 0 || strcmp(NodeList[provider_node_index]["operation"].asCString(), "OP_FC") == 0)
//            {
//                complete_node.insert(i);
//            }
//            else
//            {
//                wait_node.insert(i);
//            }
//        }
//        else
//        {
//            wait_node.insert(i);
//        }
//    }
//
//    int post_cycle = 0;
//    while (wait_node.size() != 0)
//    {
//        std::cout << "post_cycle" << post_cycle << std::endl;
//        for (std::set<int>::iterator i = wait_node.begin(); i != wait_node.end(); i++)
//        {
//            int node_index = *i;
//            bool ready = true;
//            for (int j = 0; j < NodeList[node_index]["provider_index"].size(); ++j)
//            {
//                int provider_index = NodeList[node_index]["provider_index"][j].asInt();
//                if (complete_node.count(provider_index) == 0)
//                    ready = false;
//            }
//            if (ready)
//            {
//                ready_node.insert(node_index);
//            }
//        }
//
//        for (std::set<int>::iterator i = ready_node.begin(); i != ready_node.end(); i++)
//        {
//            std::cout << "  " << *i << std::endl;
//            complete_node.insert(*i);
//            wait_node.erase(*i);
//        }
//        ready_node.clear();
//        post_cycle++;
//
//    }
//}

void InferencePipelineSchedule::ScheduleNaivePickOnePostOperationSlow(Json::Value &DNNInfo)
{
    std::set <int> complete_node;
    std::set <int> wait_node;
    std::set <int> ready_node;
    for (int i = 0; i < node_num; ++i)
    {
        Json::Value Node = NodeList[i];
        if (strcmp(Node["operation"].asCString(), "OP_INPUT") == 0 || strcmp(Node["operation"].asCString(), "OP_CONV") == 0 || strcmp(Node["operation"].asCString(), "OP_FC") == 0)
        {
            complete_node.insert(i);
        }
        else if(strcmp(Node["operation"].asCString(), "OP_RELU") == 0)
        {
            int provider_node_index = Node["provider_index"][0].asInt();
            if (strcmp(NodeList[provider_node_index]["operation"].asCString(), "OP_CONV") == 0 || strcmp(NodeList[provider_node_index]["operation"].asCString(), "OP_FC") == 0)
            {
                complete_node.insert(i);
            }
            else
            {
                wait_node.insert(i);
            }
        }
        else
        {
            wait_node.insert(i);
        }
    }

    for (std::set<int>::iterator i = wait_node.begin(); i != wait_node.end(); i++)
    {
        int node_index = *i;
        bool ready = true;
        for (int j = 0; j < NodeList[node_index]["provider_num"].asInt(); ++j)
        {
            int provider_index = NodeList[node_index]["provider_index"][j].asInt();
            if (complete_node.count(provider_index) == 0)
                ready = false;
        }
        if (ready)
        {
            ready_node.insert(node_index);
        }
    }

    int post_cycle = 0;
    while (wait_node.size() != 0)
    {
//        std::cout << "post_cycle " << post_cycle << std::endl;

//        std::cout << "  ready : ";
//        for (std::set<int>::iterator i = ready_node.begin(); i != ready_node.end(); i++)
//        {
//            std::cout << *i << " ";
//        }
//        std::cout << std::endl;

        int pick = *ready_node.begin();
//        std::cout << "  pick : " << pick << std::endl;
        std::string post_operation = NodeList[pick]["operation"].asCString();
        if (post_operation == "OP_CONCAT" || post_operation == "OP_RELU" || post_operation == "OP_POOL" || post_operation == "OP_ELTWISE")
            ScheduleNaiveScheduleOnePostOperationSlow(DNNInfo, post_cycle, pick);
        else
            std::cout << post_operation << std::endl;
        wait_node.erase(pick);
        complete_node.insert(pick);
        ready_node.clear();

        for (std::set<int>::iterator i = wait_node.begin(); i != wait_node.end(); i++)
        {
            int node_index = *i;
            bool ready = true;
            for (int j = 0; j < NodeList[node_index]["provider_num"].asInt(); ++j)
            {
                int provider_index = NodeList[node_index]["provider_index"][j].asInt();
                if (complete_node.count(provider_index) == 0)
                    ready = false;
            }
            if (ready)
            {
                ready_node.insert(node_index);
            }
        }
        post_cycle++;
    }
}


const static int inference_start = 100;
const static int inference_end = 100;
void InferencePipelineSchedule::ScheduleNaiveSlow(Json::Value &DNNInfo)
{
    // TODO：未考虑是否死锁。或许会出现这种情况。
    CoreList = DNNInfo["6_physical_core_AG_map"]["core_list"];
    int effective_instruction_group_num = GetEffectiveInstructionGroupNumSlow(DNNInfo);

    int instruction_group_num = user_given_instruction_group_num > effective_instruction_group_num ? effective_instruction_group_num : user_given_instruction_group_num;
    DNNInfo["6_base_instruction_ir"].resize(instruction_group_num);
    DNNInfo["6_post_instruction_ir"][0]["core_list"].resize(core_num);
    for (int j = 0; j < instruction_group_num; ++j)
    {
        DNNInfo["6_base_instruction_ir"][j]["core_list"].resize(core_num);

        for (int k = 0; k < operation_cycle_before_comm; k++)
        {
            ScheduleNaiveStage1Slow(DNNInfo, j, 1);
            ScheduleNaiveStage2Slow(DNNInfo, j);
            for (int & n : add_flag) {n = 0;}
        }
        //// Stage3的作用是融合同一个复制块的计算结果，得到完整的结果
        ScheduleNaiveStage3Slow(DNNInfo, j);
        for (int & n : comm_flag) {n = 0;}
        //// StageACT的作用是为每个复制块的计算结果添加激活层
        ScheduleNaiveStageActSlow(DNNInfo, j);
        for (int & n : activate_flag) {n = 0;}
        for (int & n : node_offset_instruction_group) {n = 0;}
        for (int l = 0; l < MAX_AG; ++l) {node_offset_inference_old[l] = node_offset_inference[l];}
    }
//    AddSeparateLineSlow(DNNInfo, instruction_group_num-1);
    //// 情况并且填满node_offset_inference和input_cycle_record，主要是为了后面这些操作是处理完整数据
    FillTheWholeInstructionGroupSlow(DNNInfo);
    //// Stage4的作用是把不同复制块的数据传到一起，以方便下一步后处理的开展
    ScheduleNaiveStage4Slow(DNNInfo, 0);
    //// mode为0的Stage6两个作用:同level节点间传输数据、产生copy_offset_flag
    ScheduleNaiveStage6Slow(DNNInfo, 0, 0, 0, 0);
    for (int & n : visit_stage6) {n = 0;}
    //// Stage5的作用是添加后处理指令
    ScheduleNaiveStage5Slow(DNNInfo, 0, 0, 0);
    for (int & n : visit_stage5) {n = 0;}
    for (int & n : wb_flag) {n = 0;}
    //// mode为1的Stage6的作用是将本轮推理周期产生的数据进行传递，以便下一个推理周期的运行
//    clock_t timestamp_1 = clock();
    ScheduleNaiveStage6Slow(DNNInfo, 0, 0, 1, 0);
//    clock_t timestamp_2 = clock();
//    std::cout << double(timestamp_2 - timestamp_1) / CLOCKS_PER_SEC << "s" << std::endl;
    for (int & n : visit_stage6) {n = 0;}
    for (int & n : node_offset_inference) {n = 0;}
    for (int & n : AG_accumulated_num) {n = 0;}
    ScheduleNaivePickOnePostOperationSlow(DNNInfo);

}



void InferencePipelineSchedule::ScheduleShowInstructionSlow(Json::Value &DNNInfo)
{
    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        std::cout << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;

//        std::cout << std::endl;
//        int instruction_group_num_1 = static_cast<int>(DNNInfo["6_base_instruction_ir"].size());
//        for (int i = 0; i < instruction_group_num_1; ++i)
//        {
//            std::cout << std::endl;
//            std::cout << "========================================= base instruction_group " << i << " =========================================" << std::endl;
//            for (int j = 0; j < core_num; ++j)
//            {
//                std::cout << "core " << j << std::endl;
//                int instruction_num = DNNInfo["6_base_instruction_ir"][i]["core_list"][j]["instruction_ir_list"].size();
//                for (int k = 0; k < instruction_num; ++k)
//                {
//                    Json::Value Instruction = DNNInfo["6_base_instruction_ir"][i]["core_list"][j]["instruction_ir_list"][k];
//                    int instruction_level_index = Instruction["level_index"].asInt();
//                    if (instruction_level_index > inf)
//                    {
//                        continue;
//                    }
//                    ShowSingleInstructionSlow(Instruction, inf);
//                }
//            }
//        }

//        std::cout << std::endl;
//        std::cout << "========================================= post instruction_group " << " =========================================" << std::endl;
//        for (int j = 0; j < core_num; ++j)
//        {
//            std::cout << "core " << j << std::endl;
//            int instruction_num = DNNInfo["6_post_instruction_ir"][0]["core_list"][j]["instruction_ir_list"].size();
//            for (int k = 0; k < instruction_num; ++k)
//            {
//                Json::Value Instruction = DNNInfo["6_post_instruction_ir"][0]["core_list"][j]["instruction_ir_list"][k];
//                int instruction_level_index = Instruction["level_index"].asInt();
//                if (instruction_level_index > inf)
//                {
//                    continue;
//                }
//                ShowSingleInstructionSlow(Instruction, inf);
//            }
//        }

//        std::cout << std::endl;
//        int instruction_group_num_2 = static_cast<int>(DNNInfo["6_post_multi_core_instruction_ir"].size());
//        for (int i = 0; i < instruction_group_num_2; ++i)
//        {
//            std::cout << std::endl;
//            std::cout << "========================================= post multi core instruction_group " << i << " =========================================" << std::endl;
//            int post_multi_core_num = DNNInfo["6_post_multi_core_instruction_ir"][i]["core_list"].size();
//            for (int j = 0; j < post_multi_core_num; ++j)
//            {
//                std::cout << "core " << j << std::endl;
//                int instruction_num = DNNInfo["6_post_multi_core_instruction_ir"][i]["core_list"][j]["instruction_ir_list"].size();
//                for (int k = 0; k < instruction_num; ++k)
//                    {
//                    Json::Value Instruction = DNNInfo["6_post_multi_core_instruction_ir"][i]["core_list"][j]["instruction_ir_list"][k];
//                    int instruction_level_index = Instruction["level_index"].asInt();
//                    if (instruction_level_index > inf)
//                    {
//                        continue;
//                    }
//                    ShowSingleInstructionSlow(Instruction, inf);
//                }
//            }
//        }

    }
}


void InferencePipelineSchedule::ScheduleSaveInstructionSlow(Json::Value &DNNInfo)
{
    std::ofstream OutFile("../slow.txt", std::ios::out | std::ios::trunc);

    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        OutFile << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;
        int instruction_group_num_1 = static_cast<int>(DNNInfo["6_base_instruction_ir"].size());
        for (int i = 0; i < instruction_group_num_1; ++i)
        {
            OutFile << "========================================= base instruction_group " << i << " =========================================" << std::endl;
            for (int j = 0; j < core_num; ++j)
            {
                OutFile << "core " << j << std::endl;
                int instruction_num = DNNInfo["6_base_instruction_ir"][i]["core_list"][j]["instruction_ir_list"].size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    Json::Value Instruction = DNNInfo["6_base_instruction_ir"][i]["core_list"][j]["instruction_ir_list"][k];
                    int instruction_level_index = Instruction["level_index"].asInt();
                    if (instruction_level_index > inf)
                    {
                        continue;
                    }
                    SaveSingleInstructionSlow(OutFile, Instruction, inf);
                }
            }
        }

        OutFile << "========================================= post instruction_group " << " =========================================" << std::endl;
        for (int j = 0; j < core_num; ++j)
        {
            OutFile << "core " << j << std::endl;
            int instruction_num = DNNInfo["6_post_instruction_ir"][0]["core_list"][j]["instruction_ir_list"].size();
            for (int k = 0; k < instruction_num; ++k)
            {
                Json::Value Instruction = DNNInfo["6_post_instruction_ir"][0]["core_list"][j]["instruction_ir_list"][k];
                int instruction_level_index = Instruction["level_index"].asInt();
                if (instruction_level_index > inf)
                {
                    continue;
                }
                SaveSingleInstructionSlow(OutFile, Instruction, inf);
            }
        }

        int instruction_group_num_2 = static_cast<int>(DNNInfo["6_post_multi_core_instruction_ir"].size());
        int index = 0;
        for (int i = 0; i < instruction_group_num_2; ++i)
        {
            int post_multi_core_num = DNNInfo["6_post_multi_core_instruction_ir"][i]["core_list"].size();
            if (DNNInfo["6_post_multi_core_instruction_ir"][i]["core_list"][0]["instruction_ir_list"].size() == 0)
            {
                continue;
            }
            OutFile << "========================================= post multi core instruction_group " << index << " =========================================" << std::endl;
            for (int j = 0; j < post_multi_core_num; ++j)
            {
                OutFile << "core " << j << std::endl;
                int instruction_num = DNNInfo["6_post_multi_core_instruction_ir"][i]["core_list"][j]["instruction_ir_list"].size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    Json::Value Instruction = DNNInfo["6_post_multi_core_instruction_ir"][i]["core_list"][j]["instruction_ir_list"][k];
                    int instruction_level_index = Instruction["level_index"].asInt();
                    if (instruction_level_index > inf)
                    {
                        continue;
                    }
                    SaveSingleInstructionSlow(OutFile, Instruction, inf);
                }
            }
            index ++;
        }

    }
    OutFile.close();
}

void InferencePipelineSchedule::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/6_es.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}