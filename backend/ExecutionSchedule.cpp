//
// Created by SXT on 2022/8/24.
//

#include "ExecutionSchedule.h"

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


void ExecutionSchedule::ScheduleExecution(Json::Value &DNNInfo)
{
    SchedulePreparation(DNNInfo);
    ScheduleNaive(DNNInfo);
}

void ExecutionSchedule::SchedulePreparation(Json::Value &DNNInfo)
{
    //// 注意Naive写法是针对没有split_AG的情况。每个AG只有一个对应的Core

    // 根据4_physical_crossbar_placement信息提供的Core上Crossbar的关系得到Core上AG的关系
    int core_num = DNNInfo["3_virtual_core_crossbar_map"].size();
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

            DNNInfo["6_physical_core_AG_map"]["core_list"][core_index]["AG_list"].append(AGInfo);
            DNNInfo["6_physical_core_AG_map"]["core_list"][core_index]["node_list"].append(node_index);
        }
    }

    //// 得到两个调度过程中需要的值（每个结点Rep0 AG0所在的核、以及每个结点每个Rep的AG0所在的核）
    int node_num = DNNInfo["2_AG_partition"].size();
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

    for (int i = 0; i < core_num; ++i)
    {
        std::cout << i << std::endl;
        int AG_num = DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"].size();
        for (int j = 0; j < AG_num; ++j)
        {
            std::cout << "    " << DNNInfo["6_physical_core_AG_map"]["core_list"][i]["node_list"][j]
                      << "    " << DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["replication_index"]
                      << "    " << DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["AG_index_in_replication"]
                      << "   | " << DNNInfo["6_physical_core_AG_map"]["core_list"][i]["AG_list"][j]["AG_index_in_total"] << std::endl;
        }
    }
}

void ExecutionSchedule::ScheduleNaiveStage1(Json::Value &  DNNInfo, int instruction_group_index, bool append_instruction)
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
                Instruction["node_index"] = node_index;
                Instruction["AGP"] = AGP;
                Instruction["agp_index"] = agp_index;
                Instruction["operation"] = "MVMUL";
                Instruction["destination"] = Instruction["AG_index_in_total"];
                Instruction["source"] = Instruction["AG_index_in_total"];


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
                Offset["rs1"] = 0;
                Offset["rs2"] = 0;
                Offset["value"] = node_offset_inference[node_index]*output_element_num + agp_offset;
                // 暂时先不考虑AGP的存在了
//                int whole_output_width = DNNInfo["node_list"][node_index]["W"].asInt();
//                Offset["value"] = AGP == 1 ? (node_offset_instruction_group[node_index]*output_element_num + agp_offset) : (node_offset_instruction_group[node_index]*whole_output_width + agp_offset) ;
                Instruction["offset"] = Offset;

                Instruction["instruction_group_index"] = instruction_group_index;
                if(append_instruction)
                    DNNInfo["6_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction);

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

void ExecutionSchedule::ScheduleNaiveStage2(Json::Value &  DNNInfo, int instruction_group_index)
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

                    Instruction["relative_length"] = 1;
                    Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[Instruction["source_1"].asInt()];
                    Instruction["instruction_group_index"] = instruction_group_index;
                    Json::Value Offset;
                    Offset["rd"] = 1;
                    Offset["rs1"] = 1;
                    Offset["rs2"] = 1;
                    Offset["value"] = (node_offset_inference[AG_index_in_total]-1)*Instruction["element_num"].asInt();
                    Instruction["offset"] = Offset;
                    DNNInfo["6_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction);
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

void ExecutionSchedule::ScheduleNaiveStage3(Json::Value &  DNNInfo, int instruction_group_index)
{
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
                    Instruction_send["operation"] = "SEND";
                    Instruction_send["to_core"] = RecvCore;
                    Instruction_send["source"] = AG_index_in_total;
                    Instruction_send["relative_length"] = node_offset_instruction_group[AG_index_in_total];
                    Instruction_send["element_num"] = Instruction_send["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    Instruction_send["instruction_group_index"] = instruction_group_index;
                    Instruction_send["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_send["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    DNNInfo["6_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_send);

                    Json::Value Instruction_recv;
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
                    DNNInfo["6_core_instruction_ir"][instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].append(Instruction_recv);

                    Json::Value Instruction_vadd;
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
                    DNNInfo["6_core_instruction_ir"][instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].append(Instruction_vadd);

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

void ExecutionSchedule::ScheduleNaiveStageAct(Json::Value &DNNInfo, int instruction_group_index)
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
            if (activate_flag[AG_index_in_total] == 1)
            {
                Json::Value Instruction;
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
                DNNInfo["6_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction);
            }
        }
    }
}

void ExecutionSchedule::ScheduleNaiveStage4(Json::Value &  DNNInfo, int operation_cycle_before_comm)
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
                    Instruction_send["operation"] = "SEND";
                    Instruction_send["to_core"] = RecvCore;
                    Instruction_send["source"] = AG_index_in_total;
                    Instruction_send["relative_length"] = node_offset_inference[AG_index_in_total];
                    Instruction_send["element_num"] = Instruction_send["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    int real_instruction_group_index = (node_offset_inference[AG_index_in_total]-1)/operation_cycle_before_comm;
                    Instruction_send["instruction_group_index"] = real_instruction_group_index;
                    Instruction_send["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_send["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_send);

                    Json::Value Instruction_recv;
                    Instruction_recv["operation"] = "RECV";
                    Instruction_recv["from_core"] = i;
                    Instruction_recv["destination"] = AG_index_in_total;
                    Instruction_recv["relative_length"] = node_offset_inference[AG_index_in_total];
                    Instruction_recv["element_num"] = Instruction_recv["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    Instruction_recv["instruction_group_index"] = real_instruction_group_index;
                    Instruction_recv["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_recv["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].append(Instruction_recv);

                    // 这里的WB指的是接受之后写回到正确的位置
                    Json::Value Instruction_wb;
                    Instruction_wb["operation"] = "WB";
                    Instruction_wb["source"] = AG_index_in_total;
                    Instruction_wb["replication_index"] = replication_index;
                    Instruction_wb["relative_length"] = node_offset_inference[AG_index_in_total];
                    Instruction_wb["element_num"] = Instruction_wb["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    Instruction_wb["instruction_group_index"] = real_instruction_group_index;
                    Instruction_wb["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_wb["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][RecvCore]["instruction_ir_list"].append(Instruction_wb);
                }
                else
                {
                    Json::Value Instruction_wb;
                    Instruction_wb["operation"] = "WB";
                    Instruction_wb["source"] = AG_index_in_total;
                    Instruction_wb["replication_index"] = replication_index;
                    Instruction_wb["relative_length"] = node_offset_inference[AG_index_in_total];
                    Instruction_wb["element_num"] = Instruction_wb["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                    int real_instruction_group_index = (node_offset_inference[AG_index_in_total]-1)/operation_cycle_before_comm;
                    Instruction_wb["instruction_group_index"] = real_instruction_group_index;
                    Instruction_wb["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                    Instruction_wb["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                    DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_wb);
                }
            }
            else if (AG_index_in_replication == 0 && replication_index == 0  && wb_flag[AG_index_in_total] > 0)
            {
                Json::Value Instruction_wb;
                Instruction_wb["operation"] = "WB";
                Instruction_wb["source"] = AG_index_in_total;
                Instruction_wb["replication_index"] = replication_index;
                Instruction_wb["relative_length"] = node_offset_inference[AG_index_in_total];
                Instruction_wb["element_num"] = Instruction_wb["relative_length"].asInt() * AG_output_element_size[AG_index_in_total];
                int real_instruction_group_index = (node_offset_inference[AG_index_in_total]-1)/operation_cycle_before_comm;
                Instruction_wb["instruction_group_index"] = real_instruction_group_index;
                Instruction_wb["AGP"] = CoreList[i]["AG_list"][j]["AGP"].asInt();
                Instruction_wb["agp_index"] = CoreList[i]["AG_list"][j]["agp_index"].asInt();
                DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_wb);
            }
        }
    }
}

static int visit_stage5[MAX_NODE] = {0};
void ExecutionSchedule::ScheduleNaiveStage5(Json::Value & DNNInfo, int operation_cycle_before_comm, int node_index, int level_index)
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
                    ScheduleNaiveStage5(DNNInfo, operation_cycle_before_comm, consumer_index, consumer_level);
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
                        int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                        DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
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
                        Json::Value Instruction;
                        Instruction["operation"] = "CONCAT";
                        Instruction["operation_type"] = "VM";
                        Instruction["node_index"] = consumer_index;
                        Instruction["source"] = NodeList[provider_index]["AG0_index_in_total"];
                        Instruction["destination"] = AG0_index_in_total;
//                        Instruction["relative_length"] = node_offset_inference[provider_AG0_index];  // 未考虑重复块
                        Instruction["relative_length"] = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
                        Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[provider_AG0_index];
                        Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
                        int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                        DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
                        // 下面这个代码是将CONCAT代码展开来，即具体形式。
//                        {
//                            // 这个output_channel_num是完整的
//                            // int output_channel_num = NodeList[provider_index]["output_dim"][2].asInt() * NodeList[provider_index]["output_dim"][3].asInt(); // H*W
//                            // 这个output_channel_num不完全
//                            // int output_channel_num = node_offset_inference[provider_AG0_index];
//                            // 这个output_channel_num是可行的
//                            int output_channel_num = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
//                            int output_channel_element_size = NodeList[provider_index]["output_dim"][1].asInt();
//                            if (j != 0)
//                            {
//                                int last_provider_index = NodeList[consumer_index]["provider_index"][j-1].asInt();
//                                accumulated_offset += NodeList[last_provider_index]["output_dim"][1].asInt();;
//                            }
//                            for (int k = 0; k < output_channel_num; ++k)
//                            {
//                                int input_cycle = DNNInfo["6_input_cycle_record"][effective_provider_index][k].asInt();
//                                int rs_offset = input_cycle * output_channel_element_size;
//                                int rd_offset = input_cycle * output_channel_element_size_concat + accumulated_offset;
//                                Json::Value Instruction_detail;
//                                Instruction_detail["input_cycle"] = input_cycle;
//                                Instruction_detail["operation"] = "CONCAT";
//                                Instruction_detail["operation_type"] = "VM-detail";
//                                Instruction_detail["node_index"] = consumer_index;
//                                Instruction_detail["source"] = NodeList[provider_index]["AG0_index_in_total"];
//                                Instruction_detail["destination"] = AG0_index_in_total;
//                                Instruction_detail["rs_offset"] = rs_offset;
//                                Instruction_detail["rd_offset"] = rd_offset;
//                                Instruction_detail["relative_length"] = 1;
//                                Instruction_detail["element_num"] = Instruction_detail["relative_length"].asInt() * AG_output_element_size[provider_AG0_index];
//                                Instruction_detail["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
//                                int real_instruction_group_index_detail = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
//                                DNNInfo["6_core_instruction_ir"][real_instruction_group_index_detail]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction_detail);
//                            }
//                        }
                    }
                }
                else if (strcmp(consumer_op.c_str(), "OP_RELU") == 0 || strcmp(consumer_op.c_str(), "OP_TANH") == 0 || strcmp(consumer_op.c_str(), "OP_SIGMOID") == 0)
                {
                    // 如果该consumer的生产者是CONV或FC，那就跳过。
                    if (strcmp(NodeList[node_index]["operation"].asCString(), "OP_CONV") != 0 && strcmp(NodeList[node_index]["operation"].asCString(), "OP_FC") != 0)
                    {
                        int effective_provider_index = NodeList[node_index]["AG0_node_index"].asInt();
                        Json::Value Instruction;
                        Instruction["operation"] = consumer_op;
                        Instruction["node_index"] = consumer_index;
                        Instruction["source"] = AG0_index_in_total;
                        Instruction["destination"] = AG0_index_in_total;
//                        Instruction["relative_length"] = node_offset_inference[AG0_index_in_total];
                        Instruction["relative_length"] = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
                        Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[AG0_index_in_total];
                        Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
                        int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                        DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
                    }
                }
//                else if (strcmp(consumer_op.c_str(), "OP_POOL") == 0)
//                {
//                    bool output_visit_flag[100000];
//                    int effective_provider_index = NodeList[node_index]["AG0_node_index"].asInt();
//                    int ready_input_num = DNNInfo["6_input_cycle_record"][effective_provider_index].size(); // the input of pool
//                    int input_element_in_total = NodeList[consumer_index]["input_dim"][1].asInt() * NodeList[consumer_index]["input_dim"][2].asInt() * NodeList[consumer_index]["input_dim"][3].asInt();
//                    Json::Value PoolInfo = NodeList[consumer_index]["pool_info"]["input_index"];
//                    for (int j = 0; j < ready_input_num; ++j)
//                    {
//                        int input_index = DNNInfo["6_input_cycle_record"][effective_provider_index][j].asInt();
//                        int associated_output_num = PoolInfo[input_index].size();
//                        for (int k = 0; k < associated_output_num; ++k)
//                        {
//                            int output_index = PoolInfo[input_index][k].asInt();
//                            Json::Value Instruction;
//                            Instruction["operation"] = "POOL";
//                            Instruction["input_index"] = input_index;
//                            Instruction["output_index"] = output_index;
//                            Instruction["node_index"] = consumer_index;
//                            Instruction["relative_length"] = 1;
//                            Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[AG0_index_in_total];
//                            Instruction["input_element_in_total"] = input_element_in_total;
//                            Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
//                            if (output_visit_flag[output_index] == 0) // 如果提前没有访问过，就先把输入向量搬运过去。
//                            {
//                                output_visit_flag[output_index] = 1;
//                                Instruction["operation_type"] = "VM";
//                                Instruction["source"] = AG0_index_in_total;
//                                Instruction["destination"] = AG0_index_in_total;
//                                Instruction["rs_offset"] = input_index * Instruction["element_num"].asInt();
//                                Instruction["rd_offset_in_output"] = output_index * Instruction["element_num"].asInt();
//                                Instruction["rd_offset"] = input_element_in_total + Instruction["rd_offset_in_output"].asInt();
//                            }
//                            else
//                            {
//                                Instruction["operation_type"] = "VVMAX";
//                                Instruction["source_1"] = AG0_index_in_total;
//                                Instruction["source_2"] = AG0_index_in_total;
//                                Instruction["destination"] = AG0_index_in_total;
//                                Instruction["rs1_offset"] = input_index * Instruction["element_num"].asInt();
//                                Instruction["rs2_offset_in_output"] = output_index * Instruction["element_num"].asInt();
//                                Instruction["rs2_offset"] = input_element_in_total + Instruction["rs_offset_in_output"].asInt();
//                                Instruction["rd_offset"] = Instruction["rs2_offset"];
//                            }
//                            int real_instruction_group_index = (node_offset_inference[AG0_index_in_total] - 1) / operation_cycle_before_comm;
//                            DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
//                        }
//                    }
//                }
                else
                {
                    int effective_provider_index = NodeList[node_index]["AG0_node_index"].asInt();
                    Json::Value Instruction;
                    Instruction["operation"] = consumer_op;
                    Instruction["node_index"] = consumer_index;
                    Instruction["source"] = AG0_index_in_total;
                    Instruction["destination"] = AG0_index_in_total;
//                    Instruction["relative_length"] = node_offset_inference[AG0_index_in_total];
                    Instruction["relative_length"] = DNNInfo["6_input_cycle_record"][effective_provider_index].size();
                    Instruction["element_num"] = Instruction["relative_length"].asInt() * AG_output_element_size[AG0_index_in_total];
                    Instruction["copy_offset_flag"] = NodeList[consumer_index]["copy_offset_flag"];
                    int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                    DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][AG0_core_index]["instruction_ir_list"].append(Instruction);
                }
                ScheduleNaiveStage5(DNNInfo, operation_cycle_before_comm, consumer_index, level_index);
            }
        }
    }
}

static int visit_stage6[MAX_NODE] = {0};
void ExecutionSchedule::ScheduleNaiveStage6(Json::Value & DNNInfo, int operation_cycle_before_comm, int node_index, int level_index, int mode)
{
    // 每次向前跳一步。所以只用检测visit_stage6[node_index]是否等于0即可。
    if (visit_stage6[node_index] != 0)
        return;
    visit_stage6[node_index] = 1;
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
                                std::cout << "[Comm] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
                                          << "core_" << consumer_core << " node_" << consumer_index << " AG_" << consumer_AG_index << std::endl;

                                Json::Value Instruction_send;
                                Instruction_send["operation"] = "SEND";
                                Instruction_send["to_core"] = consumer_core;
                                Instruction_send["source"] = provider_AG_index;
                                Instruction_send["relative_length"] = node_offset_inference[NodeList[node_index]["AG0_index_in_total"].asInt()];
                                Instruction_send["element_num"] = Instruction_send["relative_length"].asInt() * AG_output_element_size[provider_AG_index];
                                int AG0_index_in_total = NodeList[node_index]["AG0_index_in_total"].asInt();
                                int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                                Instruction_send["instruction_group_index"] = real_instruction_group_index;
                                DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][provider_core]["instruction_ir_list"].append(Instruction_send);

                                Json::Value Instruction_recv;
                                Instruction_recv["operation"] = "RECV";
                                Instruction_recv["from_core"] = provider_core;
                                Instruction_recv["destination"] = provider_AG_index;
                                Instruction_recv["relative_length"] = node_offset_inference[NodeList[node_index]["AG0_index_in_total"].asInt()];
                                Instruction_recv["element_num"] = Instruction_recv["relative_length"].asInt() * AG_output_element_size[provider_AG_index];
                                Instruction_recv["instruction_group_index"] = real_instruction_group_index;
                                DNNInfo["6_core_instruction_ir"][real_instruction_group_index]["core_list"][consumer_core]["instruction_ir_list"].append(Instruction_recv);
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
                        if (consumer_core != provider_core)
                        {
                            std::cout << "[Comm] from core_" << provider_core <<" node_" << node_index << " AG_" << provider_AG_index << " TO "
                                      << "core_" << consumer_core<<" node_" << consumer_index  << " AG_" << consumer_AG_index << std::endl;
                        }
                    }
                    else if  (consumer_level-level_index > 1)

                    {
                        if (consumer_core != provider_core)
                        {
                            std::cout << "[Cache] from core_" << provider_core <<" node_" << node_index << " AG_" << provider_AG_index << " TO "
                                      << "core_" << consumer_core<<" node_" << consumer_index  << " AG_" << consumer_AG_index << std::endl;
                        }
                    }
                }
            }
            ScheduleNaiveStage6(DNNInfo, operation_cycle_before_comm, consumer_index, consumer_level, mode);
        }
    }
}



void ExecutionSchedule::AddSeparateLine(Json::Value & DNNInfo, int instruction_group_index)
{
    for (int i = 0; i < core_num; ++i)
    {
        Json::Value Instruction_sep;
        Instruction_sep["operation"] = "sep";
        DNNInfo["6_core_instruction_ir"][instruction_group_index]["core_list"][i]["instruction_ir_list"].append(Instruction_sep);
    }
}


void ExecutionSchedule::ScheduleNaive(Json::Value &DNNInfo)
{
    // TODO：未考虑是否死锁。或许会出现这种情况。
    core_num = static_cast<int>(DNNInfo["6_physical_core_AG_map"]["core_list"].size());
    CoreList = DNNInfo["6_physical_core_AG_map"]["core_list"];
    node_num = static_cast<int>(DNNInfo["5_node_list_augmented"].size());
    NodeList = DNNInfo["5_node_list_augmented"];
    int inference_num = 1;
    int instruction_group_num = 2;
    int operation_cycle_before_comm = 2;
    DNNInfo["6_core_instruction_ir"].resize(instruction_group_num);

    for (int i = 0; i < inference_num; ++i)
    {
        for (int j = 0; j < instruction_group_num; ++j)
        {
            DNNInfo["6_core_instruction_ir"][j]["core_list"].resize(core_num);
            for (int k = 0; k < operation_cycle_before_comm; k++)
            {
                ScheduleNaiveStage1(DNNInfo, j, 0);
//                ScheduleNaiveStage2(DNNInfo, j);
                for (int & n : add_flag) {n = 0;}
            }
            //// Stage3的作用是融合同一个复制块的计算结果，得到完整的结果
//            ScheduleNaiveStage3(DNNInfo, j);
            for (int & n : comm_flag) {n = 0;}
            //// StageACT的作用是为每个复制块的计算结果添加激活层
//            ScheduleNaiveStageAct(DNNInfo, j);
            for (int & n : activate_flag) {n = 0;}
            for (int & n : node_offset_instruction_group) {n = 0;}
            for (int l = 0; l < MAX_NODE; ++l) {node_offset_inference_old[l] = node_offset_inference[l];}
        }
//        AddSeparateLine(DNNInfo, instruction_group_num-1);
        //// Stage4的作用是把不同复制块的数据传到一起，以方便下一步后处理的开展。
//        ScheduleNaiveStage4(DNNInfo, operation_cycle_before_comm);
        //// mode为0的Stage6两个作用:同level节点间传输数据、产生copy_offset_flag
        ScheduleNaiveStage6(DNNInfo, operation_cycle_before_comm, 0, 0, 0);
        for (int & n : visit_stage6) {n = 0;}
        //// Stage5的作用是添加后处理指令
        ScheduleNaiveStage5(DNNInfo, operation_cycle_before_comm, 0, 0);
        for (int & n : visit_stage5) {n = 0;}
        for (int & n : wb_flag) {n = 0;}
        //// mode为1的Stage6的作用是将本轮推理周期产生的数据进行传递，以便下一个推理周期的运行
//        ScheduleNaiveStage6(DNNInfo, operation_cycle_before_comm, 0, 0, 1);
        for (int & n : visit_stage6) {n = 0;}
        for (int & n : node_offset_inference) {n = 0;}
        for (int & n : AG_accumulated_num) {n = 0;}
    }
}

void ExecutionSchedule::ScheduleShowInstruction(Json::Value &DNNInfo)
{
    int instruction_group_num = static_cast<int>(DNNInfo["6_core_instruction_ir"].size());
    for (int i = 0; i < instruction_group_num; ++i)
    {
        std::cout << std::endl;
        std::cout << "========================================= instruction_group " << i << " =========================================" << std::endl;
        for (int j = 0; j < core_num; ++j)
        {
            std::cout << "core " << j << std::endl;
            int instruction_num = DNNInfo["6_core_instruction_ir"][i]["core_list"][j]["instruction_ir_list"].size();
            for (int k = 0; k < instruction_num; ++k)
            {
                Json::Value Instruction = DNNInfo["6_core_instruction_ir"][i]["core_list"][j]["instruction_ir_list"][k];
                std::string Operation = Instruction["operation"].asCString();
                if (strcmp(Operation.c_str(), "RECV") == 0)
                    std::cout << "    [" << Operation << "]"
                              << " rd:" << Instruction["destination"]
                              <<  " from Core:" << Instruction["from_core"]
                              << " rlength:" << Instruction["relative_length"]
                              << " element_num:" << Instruction["element_num"] << std::endl;
                else if (strcmp(Operation.c_str(), "SEND") == 0)
                    std::cout << "    [" << Operation << "]"
                              << " rs:" << Instruction["source"]
                              <<  " to Core:" << Instruction["to_core"]
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
                            << std::setw(5) << " rs1:" << Instruction["source_1"]
                            << std::setw(5)<< " rs2:" << Instruction["source_2"]
                            << std::setw(5)<< " rd:" << Instruction["destination"]
                            << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
                            << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
                            << std::setw(12)<< " rs1_offset:" << std::setw(8) << Instruction["rs1_offset"]
                            << std::setw(12)<< " rs2_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) <<  Instruction["rs2_offset_in_output"]
                            << std::setw(12)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rs2_offset_in_output"]
                            << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
                            << std::setw(12)<< " element_num:" << Instruction["element_num"]
                              << std::endl;
                else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VM") == 0)
                    std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                              << std::setw(8) << " rs:" << Instruction["source"]
                              << std::setw(11)<< " rd:" << Instruction["destination"]
                              << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
                              << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
                              << std::setw(12)<< " rs_offset:" << std::setw(8) << Instruction["rs_offset"]
                              << std::setw(41)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rd_offset_in_output"]
                              << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
                              << std::setw(12)<< " element_num:" << Instruction["element_num"]
                              << std::endl;
                else if (strcmp(Operation.c_str(), "sep") != 0)
                    std::cout << "      【" << Operation << "】"
                              << " rs:" << Instruction["source"]
                              << " copy_offset:" << Instruction["copy_offset_flag"]
                              << " element_num:" << Instruction["element_num"]
//                              << " node:" << Instruction["node_index"]
                               << std::endl;
                else
                    std::cout << "    " << Operation << std::endl;
            }
        }
    }
}

void ExecutionSchedule::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/6_es.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}