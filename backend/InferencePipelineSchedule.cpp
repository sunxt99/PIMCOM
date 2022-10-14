//
// Created by SXT on 2022/9/16.
//

#include "InferencePipelineSchedule.h"



static int AG_accumulated_num[MAX_AG] = {0};
static int AG_output_element_size[MAX_AG] = {0};

static int activate_flag[MAX_AG] = {0};
static int add_flag[MAX_AG] = {0};
static int comm_flag[MAX_AG] = {0};
static int wb_flag[MAX_AG] = {0};

static int node_offset_instruction_group[MAX_AG] = {0};
static int node_offset_inference[MAX_AG] = {0};
static int node_offset_inference_old[MAX_AG] = {0};

InferencePipelineSchedule::InferencePipelineSchedule(enum Mode RunMode)
{
    ScheduleMode = RunMode;
    core_num = PIMCOM_3_virtual_core_crossbar_map.size();
    node_num = PIMCOM_node_list.size();
    AG_num_total = PIMCOM_3_hierarchy_map.whole.size();
}

void InferencePipelineSchedule::ScheduleExecution()
{
    SchedulePreparation();
    switch (ScheduleMode)
    {
        case Exploration:
        {
            ScheduleNaiveForEvaluation();
            break;
        }
        case Generation:
        {
            ScheduleNaive();
            break;
        }
    }
    Clear();
}

void InferencePipelineSchedule::SchedulePreparation()
{
    //// 其他操作都放到了Hierarchy Mapping阶段完成
    //// 得到两个调度过程中需要的值（每个结点Rep0 AG0所在的核、以及每个结点每个Rep的AG0所在的核）
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list.size();
        for (int j = 0; j < AG_num; ++j)
        {
            int AG_index_in_replication = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].AG_index_in_replication;
            if (AG_index_in_replication == 0)
            {
                int node_index = PIMCOM_4_virtual_core_AG_map.core_list[i].node_list[j];
                int replication_index = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].replication_index;
                PIMCOM_4_first_AG_info.node_list[node_index].replication_list[replication_index] = i;
            }
        }
    }

    //// 得到周期结束需要传输数据的核
    int node_appearance_num[MAX_NODE] = {0};
    int node_appearance_element[MAX_NODE] = {0};
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list.size();
        int pre_node_index = PIMCOM_4_virtual_core_AG_map.core_list[i].node_list[0];
        int pre_replication_index = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[0].replication_index;
        int pre_AG_index = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[0].AG_index_in_total;
        int pre_AG_index_in_replication = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[0].AG_index_in_replication;

        int pre_core_index = i;
        int pre_AG_height = PIMCOM_3_hierarchy_map.whole[pre_AG_index][0].height_end - PIMCOM_3_hierarchy_map.whole[pre_AG_index][0].height_start + 1;
        struct node_recv_info RecvInfo;
        RecvInfo.replication_index = pre_replication_index;
        RecvInfo.AG_index = pre_AG_index;
        RecvInfo.AG_index_in_replication = pre_AG_index_in_replication;
        RecvInfo.core_index = pre_core_index;
        RecvInfo.node_index = pre_node_index;
        if ( PIMCOM_node_list[pre_node_index].operation == "OP_FC")
        {
            RecvInfo.start_offset_num = node_appearance_num[pre_node_index];
            RecvInfo.start_offset_element = node_appearance_element[pre_node_index];
            RecvInfo.recv_num = 1;
            RecvInfo.recv_element = pre_AG_height;
        }
        PIMCOM_4_recv_info.node_list[pre_node_index].push_back(RecvInfo);
        node_appearance_num[pre_node_index]++;
        node_appearance_element[pre_node_index] += pre_AG_height;
        for (int j = 1; j < AG_num; ++j)
        {
            int node_index = PIMCOM_4_virtual_core_AG_map.core_list[i].node_list[j];
            int replication_index = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].replication_index;
            int AG_index = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].AG_index_in_total;
            int AG_index_in_replication = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].AG_index_in_replication;
            int AG_height = PIMCOM_3_hierarchy_map.whole[AG_index][0].height_end - PIMCOM_3_hierarchy_map.whole[AG_index][0].height_start + 1;

            if (node_index != pre_node_index || pre_replication_index != replication_index)
            {
                struct node_recv_info RecvInfo2;
                RecvInfo2.replication_index = replication_index;
                RecvInfo2.AG_index = AG_index;
                RecvInfo2.AG_index_in_replication = AG_index_in_replication;
                RecvInfo2.core_index = i;
                RecvInfo2.node_index = node_index;
                if (PIMCOM_node_list[node_index].operation == "OP_FC")
                {
                    RecvInfo2.start_offset_num = node_appearance_num[node_index];
                    RecvInfo2.start_offset_element = node_appearance_element[node_index];
                    RecvInfo2.recv_num = 1;
                    RecvInfo2.recv_element = AG_height;
                }
                PIMCOM_4_recv_info.node_list[node_index].push_back(RecvInfo2);
            }
            else
            {
                if (PIMCOM_node_list[node_index].operation == "OP_FC")
                {
                    int already_num = PIMCOM_4_recv_info.node_list[node_index].size();
                    PIMCOM_4_recv_info.node_list[node_index][already_num - 1].recv_num += 1;
                    PIMCOM_4_recv_info.node_list[node_index][already_num - 1].recv_element += AG_height;
                }
            }
            node_appearance_num[node_index]++;
            node_appearance_element[node_index] += AG_height;
            pre_replication_index = replication_index;
            pre_node_index = node_index;
        }
    }
}

clock_t USE = 0;
void InferencePipelineSchedule::ScheduleNaiveStage1( int instruction_group_index, bool append_instruction)
{
    //// 首先为每个AG生成MVMUL操作
    for (int i = 0; i < core_num; ++i)
    {
        if (PIMCOM_4_core_instruction_group_num[i] < instruction_group_index)
            continue;
        int AG_num = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list.size();
        struct core_schedule current_core = PIMCOM_4_virtual_core_AG_map.core_list[i];
        for (int j = 0; j < AG_num; ++j)
        {
            int AG_index_in_total = current_core.AG_list[j].AG_index_in_total;
            // 注意: 这里的start和end都包括，也就是input_cycle数量为end-start+1
            int input_cycle_this_replication_start = current_core.AG_list[j].input_cycle_this_replication_start;
            int input_cycle_this_replication_end = current_core.AG_list[j].input_cycle_this_replication_end;

            if (input_cycle_this_replication_start + AG_accumulated_num[AG_index_in_total] <= input_cycle_this_replication_end) // 例子。每个节点的输入个数为Line11所示。
            {
                int node_index = current_core.node_list[j];
                int replication_index = current_core.AG_list[j].replication_index;
                int AG_index_in_replication = current_core.AG_list[j].AG_index_in_replication;
                if (append_instruction)
                {
                    int AG_num_per_replication = current_core.AG_list[j].AG_num_per_replication;
                    int input_cycle_in_total = current_core.AG_list[j].input_cycle_in_total;
                    int replication_num = current_core.AG_list[j].replication_num;
                    int AGP = current_core.AG_list[j].AGP;
                    int agp_index = current_core.AG_list[j].agp_index;
                    int agp_offset = current_core.AG_list[j].agp_offset;
                    int level_index = current_core.AG_list[j].level_index;
                    int input_element_num = current_core.AG_list[j].input_element_num;
                    int output_element_num = current_core.AG_list[j].output_element_num;

                    struct INST Instruction;
                    Instruction.type = MVMUL;
                    Instruction.operation = "MVMUL";
                    Instruction.AG_num_per_replication = AG_num_per_replication;
                    Instruction.input_cycle_index = input_cycle_this_replication_start + AG_accumulated_num[AG_index_in_total];
                    Instruction.AG_index_in_total = AG_index_in_total;
                    Instruction.replication_index = replication_index;
                    Instruction.replication_num = replication_num;
                    Instruction.input_cycle_in_total = input_cycle_in_total;
                    Instruction.AG_index_in_replication = AG_index_in_replication;
                    Instruction.input_cycle_this_replication_start = input_cycle_this_replication_start;
                    Instruction.input_cycle_this_replication_end = input_cycle_this_replication_end;
                    Instruction.conv_or_fc = PIMCOM_node_list[node_index].operation;
                    Instruction.node_index = node_index;
                    Instruction.AGP = AGP;
                    Instruction.agp_index = agp_index;
                    Instruction.destination = Instruction.AG_index_in_total;
                    Instruction.source = Instruction.AG_index_in_total;
                    Instruction.level_index = level_index;

                    // get the input_element_num and output_element_num
                    Instruction.input_element_num = input_element_num;
                    Instruction.output_element_num = output_element_num;
//                    AG_output_element_size[AG_index_in_total] = output_element_num;
                    Instruction.rs_offset = 0;
                    Instruction.rd_offset = node_offset_inference[AG_index_in_total] * output_element_num + agp_offset;
                    Instruction.instruction_group_index = instruction_group_index;
                    PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction);
                }
                clock_t time1 = clock();
//                for AG_output_element_size
                if (AG_accumulated_num[AG_index_in_total] == 0)
                {
                    int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
                    int crossbar_num = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list.size();
                    int crossbar_start_index = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list[0];
                    int crossbar_end_index = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list[crossbar_num - 1];
                    int input_element_num = PIMCOM_2_virtual_crossbar[crossbar_start_index].height_end - PIMCOM_2_virtual_crossbar[crossbar_start_index].height_start + 1;
                    int output_element_num = PIMCOM_2_virtual_crossbar[crossbar_end_index].width_end - PIMCOM_2_virtual_crossbar[crossbar_start_index].width_start + 1;
                    AG_output_element_size[AG_index_in_total] = output_element_num;
                }

//                 for stage_2 ADD
                if (j != 0)
                {
                    if (node_index == PIMCOM_4_virtual_core_AG_map.core_list[i].node_list[j-1] && replication_index == PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j-1].replication_index)
                    {
                        add_flag[AG_index_in_total] = 1;
                    }
                }

                if (AG_index_in_replication == 0)
                {
                    activate_flag[AG_index_in_total] = 1;
                }

//                // for stage_3 SEND/RECV
//                if (AG_index_in_replication != 0)
//                {
//                    int estimate_first_AG_index = j - AG_index_in_replication;
//                    if (estimate_first_AG_index < 0 ||
//                        current_core.AG_list[estimate_first_AG_index].AG_index_in_replication != 0 ||
//                        current_core.AG_list[estimate_first_AG_index].replication_index != replication_index ||
//                        current_core.node_list[estimate_first_AG_index] != node_index)
//                    {
//                        comm_flag[AG_index_in_total] = 1;
//                    }
//                }
                //// for stage_3 SEND/RECV (具体的判断条件在函数中写)
                comm_flag[AG_index_in_total] = 1;


//                // for stage_4 WB
//                if (AG_index_in_replication == 0)
//                {
//                    wb_flag[AG_index_in_total] = 1;
//                }
                //// for stage_4 WB (具体的判断条件在函数中写)
                wb_flag[AG_index_in_total] = 1;

                // consider the offset
                node_offset_instruction_group[AG_index_in_total] += 1;
                node_offset_inference[AG_index_in_total] += 1;

                // record the input_cycle_index
                if (AG_index_in_replication == 0)
                    PIMCOM_4_input_cycle_record[node_index].push_back(input_cycle_this_replication_start+AG_accumulated_num[AG_index_in_total]);

                AG_accumulated_num[AG_index_in_total] += 1;
                clock_t time2 = clock();
                USE += time2 - time1;
            }
        }
    }
}


void InferencePipelineSchedule::ScheduleNaiveStage1ForEvaluation(int instruction_group_index, int operation_cycle_index, bool append_instruction)
{
    //// 首先为每个AG生成MVMUL操作
    for (int i = 0; i < core_num; ++i)
    {
        if (PIMCOM_4_core_instruction_group_num[i] < instruction_group_index)
            continue;
        int AG_num = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list.size();
        struct core_schedule current_core = PIMCOM_4_virtual_core_AG_map.core_list[i];
        for (int j = 0; j < AG_num; ++j)
        {
            int AG_index_in_total = current_core.AG_list[j].AG_index_in_total;
            // 注意: 这里的start和end都包括，也就是input_cycle数量为end-start+1
            int input_cycle_this_replication_start = current_core.AG_list[j].input_cycle_this_replication_start;
            int input_cycle_this_replication_end = current_core.AG_list[j].input_cycle_this_replication_end;

//            if (input_cycle_this_replication_start + AG_accumulated_num[AG_index_in_total] <= input_cycle_this_replication_end)
            if (input_cycle_this_replication_start + operation_cycle_index <= input_cycle_this_replication_end)
            {
                int node_index = current_core.node_list[j];
                int replication_index = current_core.AG_list[j].replication_index;
                int AG_index_in_replication = current_core.AG_list[j].AG_index_in_replication;
                if (append_instruction)
                {
                    int AG_num_per_replication = current_core.AG_list[j].AG_num_per_replication;
                    int input_cycle_in_total = current_core.AG_list[j].input_cycle_in_total;
                    int replication_num = current_core.AG_list[j].replication_num;
                    int AGP = current_core.AG_list[j].AGP;
                    int agp_index = current_core.AG_list[j].agp_index;
                    int agp_offset = current_core.AG_list[j].agp_offset;
                    int level_index = current_core.AG_list[j].level_index;
                    int input_element_num = current_core.AG_list[j].input_element_num;
                    int output_element_num = current_core.AG_list[j].output_element_num;

                    struct INST Instruction;
                    Instruction.type = MVMUL;
                    Instruction.operation = "MVMUL";
                    Instruction.AG_num_per_replication = AG_num_per_replication;
                    Instruction.input_cycle_index = input_cycle_this_replication_start + operation_cycle_index;
                    Instruction.AG_index_in_total = AG_index_in_total;
                    Instruction.replication_index = replication_index;
                    Instruction.replication_num = replication_num;
                    Instruction.input_cycle_in_total = input_cycle_in_total;
                    Instruction.AG_index_in_replication = AG_index_in_replication;
                    Instruction.input_cycle_this_replication_start = input_cycle_this_replication_start;
                    Instruction.input_cycle_this_replication_end = input_cycle_this_replication_end;
                    Instruction.conv_or_fc = PIMCOM_node_list[node_index].operation;
                    Instruction.node_index = node_index;
                    Instruction.AGP = AGP;
                    Instruction.agp_index = agp_index;
                    Instruction.destination = Instruction.AG_index_in_total;
                    Instruction.source = Instruction.AG_index_in_total;
                    Instruction.level_index = level_index;

                    // get the input_element_num and output_element_num
                    Instruction.input_element_num = input_element_num;
                    Instruction.output_element_num = output_element_num;
                    Instruction.rs_offset = 0;
                    Instruction.rd_offset = node_offset_inference[AG_index_in_total] * output_element_num + agp_offset;
                    Instruction.instruction_group_index = instruction_group_index;
                    PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction);
                }

                clock_t time1 = clock();
                //// 这里AG_accumulated_num作用发生了变化。以前是记录每个AG出现的次数，现在只是指示该AG是否出现过。
                if (AG_accumulated_num[AG_index_in_total] == 0)
                {
                    int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
                    int crossbar_num = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list.size();
                    int crossbar_start_index = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list[0];
                    int crossbar_end_index = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list[crossbar_num - 1];
                    int input_element_num = PIMCOM_2_virtual_crossbar[crossbar_start_index].height_end - PIMCOM_2_virtual_crossbar[crossbar_start_index].height_start + 1;
                    int output_element_num = PIMCOM_2_virtual_crossbar[crossbar_end_index].width_end - PIMCOM_2_virtual_crossbar[crossbar_start_index].width_start + 1;
                    AG_output_element_size[AG_index_in_total] = output_element_num;
                    AG_accumulated_num[AG_index_in_total] = 1;
                }

                //// for stage_2 ADD
                if (j != 0)
                {
                    if (node_index == PIMCOM_4_virtual_core_AG_map.core_list[i].node_list[j-1] && replication_index == PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j-1].replication_index)
                    {
                        add_flag[AG_index_in_total] = 1;
                    }
                }

                if (AG_index_in_replication == 0)
                {
                    activate_flag[AG_index_in_total] = 1;
                }

                //// for stage_3 SEND/RECV (具体的判断条件在函数中写)
                comm_flag[AG_index_in_total] = 1;

                //// for stage_4 WB (具体的判断条件在函数中写)
                wb_flag[AG_index_in_total] = 1;

                //// consider the offset
                node_offset_instruction_group[AG_index_in_total] += 1;
                node_offset_inference[AG_index_in_total] += 1;

                //// record the input_cycle_index
                if (AG_index_in_replication == 0)
                {
//                    PIMCOM_4_input_cycle_record[node_index].push_back(input_cycle_this_replication_start+AG_accumulated_num[AG_index_in_total]);
                    PIMCOM_4_input_cycle_record[node_index].push_back(input_cycle_this_replication_start + operation_cycle_index);
                }
//                AG_accumulated_num[AG_index_in_total] += 1;
                clock_t time2 = clock();
                USE += time2 - time1;
            }
        }
    }
}

void InferencePipelineSchedule::ScheduleNaiveStage2( int instruction_group_index, bool append_instruction)
{
    //// 同一结点且同一权重块的AG之间的结果融合（VADD）
    for (int i = 0; i < core_num; ++i)
    {
        if (PIMCOM_4_core_instruction_group_num[i] < instruction_group_index)
            continue;
        struct core_schedule current_core = PIMCOM_4_virtual_core_AG_map.core_list[i];
        int AG_num = current_core.AG_list.size();
        int node_index = current_core.node_list[0];
        int replication_index = current_core.AG_list[0].replication_index;
        int p = 1;
        for (int j = 1; j < AG_num; ++j)
        {
            int current_node_index = current_core.node_list[j];
            int current_replication_index = current_core.AG_list[j].replication_index;
            int AG_index_in_total = current_core.AG_list[j].AG_index_in_total;
            int level_index = current_core.AG_list[j].level_index;
            if (node_index == current_node_index && replication_index == current_replication_index)
            {
                if (add_flag[AG_index_in_total] == 1)
                {
                    struct INST Instruction;
                    Instruction.type = VEC2OP;
                    Instruction.operation = "VADD";
                    Instruction.destination = current_core.AG_list[j-p].AG_index_in_total;
                    Instruction.source_1 = current_core.AG_list[j-p].AG_index_in_total;
                    Instruction.source_2 = current_core.AG_list[j].AG_index_in_total;
                    Instruction.AGP = current_core.AG_list[j].AGP;
                    Instruction.agp_index = current_core.AG_list[j].agp_index;
                    Instruction.level_index = level_index;
                    Instruction.relative_length = 1;
                    Instruction.element_num = Instruction.relative_length * AG_output_element_size[Instruction.source_1];
                    Instruction.instruction_group_index = instruction_group_index;
                    Instruction.rd_offset = (node_offset_inference[AG_index_in_total] - 1)* Instruction.element_num;
                    Instruction.rs1_offset = Instruction.rd_offset;
                    Instruction.rs2_offset = Instruction.rd_offset;
                    if(append_instruction)
                        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction);
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

static int comm_index = 0;
void InferencePipelineSchedule::ScheduleNaiveStage3(int instruction_group_index, bool append_instruction)
{
    //// 结果发送与融合
    for (int i = 0; i < core_num; ++i)
    {
        if (PIMCOM_4_core_instruction_group_num[i] < instruction_group_index)
            continue;
        struct core_schedule current_core = PIMCOM_4_virtual_core_AG_map.core_list[i];
        int AG_num = current_core.AG_list.size();
        for (int j = 0; j < AG_num; ++j)
        {
            int node_index = current_core.node_list[j];
            int replication_index = current_core.AG_list[j].replication_index;
            int AG_index_in_replication = current_core.AG_list[j].AG_index_in_replication;
            int AG_index_in_total = current_core.AG_list[j].AG_index_in_total;
            int level_index = current_core.AG_list[j].level_index;

            if (AG_index_in_replication != 0  && comm_flag[AG_index_in_total] == 1)
            {
                //// 这样要保证同一Rep的AG连续且递增，不够灵活
//                bool SendRecv = false;
//                int estimate_first_AG_index =  j - AG_index_in_replication;
//                if (estimate_first_AG_index < 0 ||
//                    current_core.AG_list[estimate_first_AG_index].AG_index_in_replication != 0 ||
//                    current_core.AG_list[estimate_first_AG_index].replication_index != replication_index ||
//                    current_core.node_list[estimate_first_AG_index] != node_index )
//                {
//                    SendRecv = true;
//                }
//                if(SendRecv)

                //// 只要求递增即可，可以不连续
                bool SendRecv = true;
                for (int k = 1; k <= AG_index_in_replication; ++k)
                {
                    int estimate_first_AG_index = j - k;
                    if (estimate_first_AG_index >= 0 &&
                        current_core.AG_list[estimate_first_AG_index].AG_index_in_replication == 0 &&
                        current_core.AG_list[estimate_first_AG_index].replication_index == replication_index &&
                        current_core.node_list[estimate_first_AG_index] == node_index )
                    {
                        SendRecv = false;
                        break;
                    }
                }
                if (SendRecv)
                {
                    int RecvCore = PIMCOM_4_first_AG_info.node_list[node_index].replication_list[replication_index];
                    // 添加发送接收指令和结果融合指令。
                    struct INST Instruction_send;
                    Instruction_send.type = COMM;
                    Instruction_send.level_index = level_index;
                    Instruction_send.operation = "SEND";
                    Instruction_send.to_core = RecvCore;
                    Instruction_send.source = AG_index_in_total;
                    Instruction_send.relative_length = node_offset_instruction_group[AG_index_in_total];
                    Instruction_send.element_num = Instruction_send.relative_length * AG_output_element_size[AG_index_in_total];
                    Instruction_send.instruction_group_index = instruction_group_index;
                    Instruction_send.AGP = current_core.AG_list[j].AGP;
                    Instruction_send.agp_index = current_core.AG_list[j].agp_index;
                    Instruction_send.comm_index = comm_index;
                    Instruction_send.instruction_index_in_core = PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.size();
                    if(append_instruction)
                        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_send);

                    struct INST Instruction_recv;
                    Instruction_recv.type = COMM;
                    Instruction_recv.level_index = level_index;
                    Instruction_recv.operation = "RECV";
                    Instruction_recv.from_core = i;
                    // 注意，另一个核的AGx对应的AG_index_in_total一定没在该Core中出现过。所以可以作为地址的一种表示。
                    Instruction_recv.destination = AG_index_in_total;
                    Instruction_recv.relative_length = node_offset_instruction_group[AG_index_in_total];
                    Instruction_recv.element_num = Instruction_recv.relative_length * AG_output_element_size[AG_index_in_total];
                    Instruction_recv.instruction_group_index = instruction_group_index;
                    Instruction_recv.AGP = current_core.AG_list[j].AGP;
                    Instruction_recv.agp_index = current_core.AG_list[j].agp_index;
                    Instruction_recv.comm_index = comm_index;
                    Instruction_recv.instruction_index_in_core = PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[RecvCore].instruction_ir_list.size();
                    if(append_instruction)
                        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[RecvCore].instruction_ir_list.push_back(Instruction_recv);

                    struct INST Instruction_vadd;
                    Instruction_vadd.type = VEC2OP;
                    Instruction_vadd.level_index = level_index;
                    Instruction_vadd.operation = "VADD";
                    struct core_schedule recv_core = PIMCOM_4_virtual_core_AG_map.core_list[RecvCore];
                    int tmp_AG_num = recv_core.AG_list.size();
                    // tmp_ag_total_index是RecvCore中同node同rep的AG0对应的位置
                    int tmp_ag_total_index = 0;
                    for (int k = 0; k < tmp_AG_num; ++k)
                    {
                        if( recv_core.node_list[k] == node_index &&
                            recv_core.AG_list[k].AG_index_in_replication == 0 &&
                            recv_core.AG_list[k].replication_index == replication_index )
                        {
                            tmp_ag_total_index = recv_core.AG_list[k].AG_index_in_total;
                        }
                    }
                    Instruction_vadd.source_1 = tmp_ag_total_index;
                    Instruction_vadd.source_2 = AG_index_in_total;
                    Instruction_vadd.destination = tmp_ag_total_index;
                    Instruction_vadd.AGP = current_core.AG_list[j].AGP;
                    Instruction_vadd.agp_index = current_core.AG_list[j].agp_index;
                    Instruction_vadd.rd_offset = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[AG_index_in_total];
                    Instruction_vadd.rs1_offset = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[AG_index_in_total];
                    Instruction_vadd.rs2_offset = 0;
                    Instruction_vadd.relative_length = node_offset_instruction_group[node_index];
                    Instruction_vadd.element_num = Instruction_vadd.relative_length * AG_output_element_size[AG_index_in_total];
                    Instruction_vadd.instruction_group_index = instruction_group_index;
                    if(append_instruction)
                        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[RecvCore].instruction_ir_list.push_back(Instruction_vadd);

                    comm_index++;
                    // 因为之前经过了信息融合，所以不需要多次发送接收。跳过后面同一Rep的其他AG。
                    while (j < AG_num && current_core.AG_list[j].replication_index == replication_index)
                    {
                        j += 1;
                    }
                }
            }
        }
    }
}




void InferencePipelineSchedule::ScheduleNaiveStageAct(int instruction_group_index, bool append_instruction)
{
    for (int i = 0; i < core_num; ++i)
    {
        if (PIMCOM_4_core_instruction_group_num[i] < instruction_group_index)
            continue;
        struct core_schedule current_core = PIMCOM_4_virtual_core_AG_map.core_list[i];
        int AG_num = current_core.AG_list.size();
        for (int j = 0; j < AG_num; ++j)
        {
            int AG_index_in_total = current_core.AG_list[j].AG_index_in_total;
            int AG_index_in_replication = current_core.AG_list[j].AG_index_in_replication;
            int AG_num_per_replication = current_core.AG_list[j].AG_num_per_replication;
            int node_index = current_core.node_list[j];
            int level_index = current_core.AG_list[j].level_index;
            if (activate_flag[AG_index_in_total] == 1)
            {
                struct INST Instruction_act;
                Instruction_act.type = VEC1OP;
                Instruction_act.level_index = level_index;
                Instruction_act.operation = "VRELU";
                Instruction_act.relative_length = node_offset_instruction_group[AG_index_in_total];
                Instruction_act.source = AG_index_in_total;
                Instruction_act.destination = AG_index_in_total;
                Instruction_act.element_num = Instruction_act.relative_length * AG_output_element_size[Instruction_act.source];
                Instruction_act.instruction_group_index = instruction_group_index;
                Instruction_act.rd_offset = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[Instruction_act.source];
                Instruction_act.rs_offset = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[Instruction_act.source];
                if(append_instruction)
                    PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_act);
            }
        }
    }
}

void InferencePipelineSchedule::ScheduleNaiveStage4(int instruction_group_index)
{
    //// 结果传递与写回
    for (int i = 0; i < core_num; ++i)
    {
        if (PIMCOM_4_core_instruction_group_num[i] < instruction_group_index)
            continue;
        struct core_schedule current_core = PIMCOM_4_virtual_core_AG_map.core_list[i];
        int AG_num = current_core.AG_list.size();
        for (int j = 0; j < AG_num; ++j)
        {
            int node_index = current_core.node_list[j];
            int replication_index = current_core.AG_list[j].replication_index;
            int AG_index_in_replication = current_core.AG_list[j].AG_index_in_replication;
            int AG_index_in_total = current_core.AG_list[j].AG_index_in_total;
            int replication_num = current_core.AG_list[j].replication_num;
            int AG_num_per_replication = current_core.AG_list[j].AG_num_per_replication;
            int level_index = current_core.AG_list[j].level_index;
            // replication_index不等于0 且 AG_index_in_replication=0 的AG0需要把结果传递给Rep0-AG0
            // 还要看AGx和AG0是否在同一个核，这样就避免了同核之间的SEND/RECV
            if (AG_index_in_replication == 0 && replication_index != 0  && wb_flag[AG_index_in_total] == 1)
            {
                int estimated_rep0_ag0 = j - replication_index * AG_num_per_replication;
                if (estimated_rep0_ag0 < 0 ||
                    current_core.AG_list[estimated_rep0_ag0].AG_index_in_replication != 0 ||
                    current_core.AG_list[estimated_rep0_ag0].replication_index != 0 ||
                    current_core.node_list[estimated_rep0_ag0] != node_index )
                {
                    int RecvCore = PIMCOM_4_first_AG_info.node_list[node_index].replication_list[0];
                    struct INST Instruction_send;
                    Instruction_send.type = COMM;
                    Instruction_send.level_index = level_index;
                    Instruction_send.operation = "SEND";
                    Instruction_send.to_core = RecvCore;
                    Instruction_send.source = AG_index_in_total;
                    Instruction_send.relative_length = node_offset_inference[AG_index_in_total];
                    Instruction_send.element_num = Instruction_send.relative_length * AG_output_element_size[AG_index_in_total];
//                    int real_instruction_group_index = (Instruction_send["relative_length"].asInt()-1)/operation_cycle_before_comm;
                    int real_instruction_group_index = instruction_group_index;
                    Instruction_send.instruction_group_index = real_instruction_group_index;
                    Instruction_send.AGP = current_core.AG_list[j].AGP;
                    Instruction_send.agp_index = current_core.AG_list[j].agp_index;
                    PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_send);

                    struct INST Instruction_recv;
                    Instruction_recv.type = COMM;
                    Instruction_recv.level_index = level_index;
                    Instruction_recv.operation = "RECV";
                    Instruction_recv.from_core = i;
                    Instruction_recv.destination = AG_index_in_total;
                    Instruction_recv.relative_length = node_offset_inference[AG_index_in_total];
                    Instruction_recv.element_num = Instruction_recv.relative_length * AG_output_element_size[AG_index_in_total];
                    Instruction_recv.instruction_group_index = real_instruction_group_index;
                    Instruction_recv.AGP = current_core.AG_list[j].AGP;
                    Instruction_recv.agp_index = current_core.AG_list[j].agp_index;
                    PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[RecvCore].instruction_ir_list.push_back(Instruction_recv);

//                     这里的WB指的是接受之后写回到正确的位置
//                    struct INST Instruction_wb;
//                    Instruction_wb.level_index = level_index;
//                    Instruction_wb.operation = "WB";
//                    Instruction_wb.source = AG_index_in_total;
//                    Instruction_wb.replication_index = replication_index;
//                    Instruction_wb.relative_length = node_offset_inference[AG_index_in_total];
//                    Instruction_wb.element_num = Instruction_wb.relative_length * AG_output_element_size[AG_index_in_total];
//                    Instruction_wb.instruction_group_index = real_instruction_group_index;
//                    Instruction_wb.AGP = current_core.AG_list[j].AGP;
//                    Instruction_wb.agp_index = current_core.AG_list[j].agp_index;
//                    PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[RecvCore].instruction_ir_list.push_back(Instruction_wb);
                }
                else
                {
//                    struct INST Instruction_wb;
//                    Instruction_wb.level_index = level_index;
//                    Instruction_wb.operation = "WB";
//                    Instruction_wb.source = AG_index_in_total;
//                    Instruction_wb.replication_index = replication_index;
//                    Instruction_wb.relative_length = node_offset_inference[AG_index_in_total];
//                    Instruction_wb.element_num = Instruction_wb.relative_length * AG_output_element_size[AG_index_in_total];
//                    int real_instruction_group_index = instruction_group_index;
//                    Instruction_wb.instruction_group_index = real_instruction_group_index;
//                    Instruction_wb.AGP = current_core.AG_list[j].AGP;
//                    Instruction_wb.agp_index = current_core.AG_list[j].agp_index;
//                    PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_wb);
                }
            }
            else if (AG_index_in_replication == 0 && replication_index == 0  && wb_flag[AG_index_in_total] > 0)
            {
//                struct INST Instruction_wb;
//                Instruction_wb.level_index = level_index;
//                Instruction_wb.operation = "WB";
//                Instruction_wb.source = AG_index_in_total;
//                Instruction_wb.replication_index = replication_index;
//                Instruction_wb.relative_length = node_offset_inference[AG_index_in_total];
//                Instruction_wb.element_num = Instruction_wb.relative_length * AG_output_element_size[AG_index_in_total];
//                int real_instruction_group_index = instruction_group_index;
//                Instruction_wb.instruction_group_index = real_instruction_group_index;
//                Instruction_wb.AGP = current_core.AG_list[j].AGP;
//                Instruction_wb.agp_index = current_core.AG_list[j].agp_index;
//                PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_wb);
            }
        }
    }
}


void InferencePipelineSchedule::ScheduleNaiveStage4WithoutWB(int instruction_group_index)
{
    //// 结果传递与写回
    for (int i = 0; i < core_num; ++i)
    {
        if (PIMCOM_4_core_instruction_group_num[i] < instruction_group_index)
            continue;
        struct core_schedule current_core = PIMCOM_4_virtual_core_AG_map.core_list[i];
        int AG_num = current_core.AG_list.size();
        for (int j = 0; j < AG_num; ++j)
        {
            int node_index = current_core.node_list[j];
            int replication_index = current_core.AG_list[j].replication_index;
            int AG_index_in_replication = current_core.AG_list[j].AG_index_in_replication;
            int AG_index_in_total = current_core.AG_list[j].AG_index_in_total;
            int replication_num = current_core.AG_list[j].replication_num;
            int AG_num_per_replication = current_core.AG_list[j].AG_num_per_replication;
            int level_index = current_core.AG_list[j].level_index;
            // replication_index不等于0 且 AG_index_in_replication=0 的AG0需要把结果传递给Rep0-AG0
            // 还要看AGx和AG0是否在同一个核，这样就避免了同核之间的SEND/RECV
            if (AG_index_in_replication == 0 && replication_index != 0  && wb_flag[AG_index_in_total] > 0)
            {
                bool SendRecvWB = true;
                for (int k = 1; k <= replication_index * AG_num_per_replication; ++k)
                {
                    int estimated_rep0_ag0 = j - k;
                    if (estimated_rep0_ag0 >= 0 &&
                        current_core.AG_list[estimated_rep0_ag0].AG_index_in_replication == 0 &&
                        current_core.AG_list[estimated_rep0_ag0].replication_index == 0 &&
                        current_core.node_list[estimated_rep0_ag0] == node_index )
                    {
                        SendRecvWB = false;
                        break;
                    }
                }
                if (SendRecvWB)
                {
                    int RecvCore = PIMCOM_4_first_AG_info.node_list[node_index].replication_list[0];
                    struct INST Instruction_send;
                    Instruction_send.type = COMM;
                    Instruction_send.level_index = level_index;
                    Instruction_send.operation = "SEND";
                    Instruction_send.to_core = RecvCore;
                    Instruction_send.source = AG_index_in_total;
                    Instruction_send.relative_length = node_offset_inference[AG_index_in_total];
                    Instruction_send.element_num = Instruction_send.relative_length * AG_output_element_size[AG_index_in_total];
//                    int real_instruction_group_index = (Instruction_send["relative_length"].asInt()-1)/operation_cycle_before_comm;
                    int real_instruction_group_index = instruction_group_index;
                    Instruction_send.instruction_group_index = real_instruction_group_index;
                    Instruction_send.AGP = current_core.AG_list[j].AGP;
                    Instruction_send.agp_index = current_core.AG_list[j].agp_index;
                    PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_send);

                    struct INST Instruction_recv;
                    Instruction_recv.type = COMM;
                    Instruction_recv.level_index = level_index;
                    Instruction_recv.operation = "RECV";
                    Instruction_recv.from_core = i;
                    Instruction_recv.destination = AG_index_in_total;
                    Instruction_recv.relative_length = node_offset_inference[AG_index_in_total];
                    Instruction_recv.element_num = Instruction_recv.relative_length * AG_output_element_size[AG_index_in_total];
                    Instruction_recv.instruction_group_index = real_instruction_group_index;
                    Instruction_recv.AGP = current_core.AG_list[j].AGP;
                    Instruction_recv.agp_index = current_core.AG_list[j].agp_index;
                    PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[RecvCore].instruction_ir_list.push_back(Instruction_recv);
                }
            }
        }
    }
}


static int visit_stage5[MAX_NODE] = {0};
void InferencePipelineSchedule::ScheduleNaiveStage5( int node_index, int level_index, int instruction_group_index)
{
    int consumer_num = PIMCOM_node_list[node_index].consumer_num;
    if (consumer_num == 0)
    {
        return;
    }
    else
    {
        for (int i = 0; i < consumer_num; ++i)
        {
            int consumer_index = PIMCOM_node_list[node_index].consumer_index[i];
            int consumer_level = PIMCOM_node_list[consumer_index].level_index;
            int AG0_core_index = PIMCOM_node_list[consumer_index].AG0_core_index;
            int AG0_index_in_total = PIMCOM_node_list[consumer_index].AG0_index_in_total;
            std::string consumer_op = PIMCOM_node_list[consumer_index].operation;
            if (level_index != consumer_level)
            {
                if (consumer_op ==  "OP_CONV" || consumer_op == "OP_FC")
                {
                    ScheduleNaiveStage5(consumer_index, consumer_level, instruction_group_index);
                }
            }
            else if (visit_stage5[consumer_index] == 0) // 这一句是为了解决一个node会有多个同level的生产者，这样每个该level的生产者都会处理一下该node，造成重复。这里设置的意义是只运行一次后处理即可。
            {
                visit_stage5[consumer_index] = 1;
                if (consumer_op == "OP_ELTWISE")
                {
                    int elt_type = PIMCOM_node_list[consumer_index].param.eletype;
                    std::string elt_operation;
                    switch (elt_type)
                    {   case 2: elt_operation = "VADD"; break;
                        case 4: elt_operation = "VSUB"; break; }
                    int provider_num_of_consumer = PIMCOM_node_list[consumer_index].provider_num;
                    // 可能会有多个量相加
                    for (int j = 0; j < provider_num_of_consumer; ++j)
                    {
                        int provider_index = PIMCOM_node_list[consumer_index].provider_index[j];
                        if(PIMCOM_node_list[provider_index].AG0_core_index == AG0_core_index && PIMCOM_node_list[provider_index].AG0_index_in_total == AG0_index_in_total)
                            continue;
                        int effective_provider_index = PIMCOM_node_list[provider_index].AG0_node_index;
                        struct INST Instruction_elt;
                        Instruction_elt.type = VEC2OP;
                        Instruction_elt.level_index = PIMCOM_node_list[consumer_index].level_index;
                        Instruction_elt.operation = elt_operation;
                        Instruction_elt.stage = "ELTWISE";
                        Instruction_elt.node_index = consumer_index;
                        Instruction_elt.source_1 = AG0_index_in_total;
                        Instruction_elt.source_2 = PIMCOM_node_list[provider_index].AG0_index_in_total;
                        Instruction_elt.destination = AG0_index_in_total;
                        // 这个relative_length是未考虑复制块的情况，所以弃用
                        // Instruction["relative_length"] = node_offset_inference[AG0_index_in_total];
                        Instruction_elt.relative_length =PIMCOM_4_input_cycle_record[effective_provider_index].size();
                        Instruction_elt.element_num = Instruction_elt.relative_length * AG_output_element_size[AG0_index_in_total];
                        Instruction_elt.copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
//                        int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                        int real_instruction_group_index = instruction_group_index;
                        PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[AG0_core_index].instruction_ir_list.push_back(Instruction_elt);
                    }
                }
                else if (strcmp(consumer_op.c_str(), "OP_CONCAT") == 0)
                {
                    int provider_num_of_consumer = PIMCOM_node_list[consumer_index].provider_num;
                    // 可能会有多个量相加
                    int output_channel_element_size_concat = 0;
                    for (int j = 0; j < provider_num_of_consumer; ++j)
                    {
                        int provider_index = PIMCOM_node_list[consumer_index].provider_index[j];
                        int effective_provider_index = PIMCOM_node_list[provider_index].AG0_node_index;
                        output_channel_element_size_concat += PIMCOM_node_list[effective_provider_index].output_dim[1];
                    }
                    int accumulated_offset = 0;
                    for (int j = 0; j < provider_num_of_consumer; ++j)
                    {
                        int provider_index = PIMCOM_node_list[consumer_index].provider_index[j];
                        int provider_AG0_index = PIMCOM_node_list[provider_index].AG0_index_in_total;
                        int consumer_level_index = PIMCOM_node_list[consumer_index].level_index;
                        int consumer_copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
                        int effective_provider_index = PIMCOM_node_list[provider_index].AG0_node_index;
                        // 这里的Instruction只是总体说明，实际上需要Instruction-detail。
//                        struct INST Instruction;
//                        Instruction.level_index = PIMCOM_node_list[consumer_index].level_index;
//                        Instruction.operation = "CONCAT";
//                        Instruction.type = "VM";
//                        Instruction.node_index = consumer_index;
//                        Instruction.source = PIMCOM_node_list[provider_index].AG0_index_in_total;
//                        Instruction.destination = AG0_index_in_total;
//                        Instruction.relative_length = PIMCOM_4_input_cycle_record[effective_provider_index].size();
//                        Instruction.element_num = Instruction.relative_length * AG_output_element_size[provider_AG0_index];
//                        Instruction.copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
//                        PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[AG0_core_index].instruction_ir_list.push_back(Instruction);
                        //// 下面这个代码是将CONCAT代码展开来，即具体形式。
                        {
                            // 这个output_channel_num是完整的
                            // int output_channel_num = PIMCOM_node_list[provider_index].output_dim[2] * PIMCOM_node_list[provider_index].output_dim[3]; // H*W
                            // 这个output_channel_num不完全
                            // int output_channel_num = node_offset_inference[provider_AG0_index];
                            // 这个output_channel_num是可行的
                            int output_channel_num = PIMCOM_4_input_cycle_record[effective_provider_index].size();
                            int output_channel_element_size = PIMCOM_node_list[provider_index].output_dim[1];
                            if (j != 0)
                            {
                                int last_provider_index = PIMCOM_node_list[consumer_index].provider_index[j-1];
                                accumulated_offset += PIMCOM_node_list[last_provider_index].output_dim[1];;
                            }
                            for (int k = 0; k < output_channel_num; ++k)
                            {
                                int input_cycle = PIMCOM_4_input_cycle_record[effective_provider_index][k];
                                int rs_offset = input_cycle * output_channel_element_size;
                                int rd_offset = input_cycle * output_channel_element_size_concat + accumulated_offset;
                                struct INST Instruction_detail;
                                Instruction_detail.type = VEC1OP;
                                Instruction_detail.level_index = consumer_level_index;
                                Instruction_detail.input_cycle = input_cycle;
                                Instruction_detail.operation = "VM";
                                Instruction_detail.stage = "CONCAT";
                                Instruction_detail.node_index = consumer_index;
                                Instruction_detail.source = provider_AG0_index;
                                Instruction_detail.destination = AG0_index_in_total;
                                Instruction_detail.rs_offset = rs_offset;
                                Instruction_detail.rd_offset = rd_offset;
                                Instruction_detail.relative_length = 1;
                                Instruction_detail.element_num = Instruction_detail.relative_length * AG_output_element_size[provider_AG0_index];
                                Instruction_detail.copy_offset_flag = consumer_copy_offset_flag;
                                PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[AG0_core_index].instruction_ir_list.push_back(Instruction_detail);
                            }
                        }
                    }
                }
                else if (consumer_op == "OP_RELU" || consumer_op == "OP_TANH" || consumer_op == "OP_SIGMOID")
                {
                    // 如果该consumer的生产者是CONV或FC，那就跳过。
                    if (PIMCOM_node_list[node_index].operation != "OP_CONV" && PIMCOM_node_list[node_index].operation != "OP_FC")
                    {
                        std::string act_type;
                        act_type = consumer_op == "OP_RELU" ? "VRELU" : (consumer_op == "OP_TANH" ? "VTANH" : "VSIGM");
                        int effective_provider_index = PIMCOM_node_list[node_index].AG0_node_index;
                        struct INST Instruction_act;
                        Instruction_act.type = VEC1OP;
                        Instruction_act.level_index = PIMCOM_node_list[consumer_index].level_index;
                        Instruction_act.operation = act_type;
                        Instruction_act.node_index = consumer_index;
                        Instruction_act.source = AG0_index_in_total;
                        Instruction_act.destination = AG0_index_in_total;
                        // TODO 这里的offset不确定
                        Instruction_act.rs_offset = 0;
                        Instruction_act.rd_offset = 0;
                        Instruction_act.relative_length = PIMCOM_4_input_cycle_record[effective_provider_index].size();
                        Instruction_act.element_num = Instruction_act.relative_length * AG_output_element_size[AG0_index_in_total];
                        Instruction_act.copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
                        int real_instruction_group_index = instruction_group_index;
                        PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[AG0_core_index].instruction_ir_list.push_back(Instruction_act);
                    }
                }
                else if (strcmp(consumer_op.c_str(), "OP_POOL") == 0)
                {
                    bool output_visit_flag[100000] = {0};
                    int effective_provider_index = PIMCOM_node_list[node_index].AG0_node_index;
                    // TODO 可能有bug
//                    int ready_input_num = PIMCOM_4_input_cycle_record[effective_provider_index].size();  // the input of pool
                    int ready_input_num = PIMCOM_node_list[consumer_index].input_dim[2] * PIMCOM_node_list[consumer_index].input_dim[3];
                    int input_element_in_total = PIMCOM_node_list[consumer_index].input_dim[1] * PIMCOM_node_list[consumer_index].input_dim[2] * PIMCOM_node_list[consumer_index].input_dim[3];
                    for (int j = 0; j < ready_input_num; ++j)
                    {
                        int input_index = PIMCOM_4_input_cycle_record[effective_provider_index][j];
                        int associated_output_num = PIMCOM_conv_pool_input_output_info[consumer_index].input_index[input_index].size();
                        for (int k = 0; k < associated_output_num; ++k)
                        {
                            int output_index = PIMCOM_conv_pool_input_output_info[consumer_index].input_index[input_index][k];
                            struct INST Instruction;
                            Instruction.level_index = PIMCOM_node_list[consumer_index].level_index;
                            Instruction.stage = "POOL";
                            Instruction.input_channel_index = input_index;
                            Instruction.output_channel_index = output_index;
                            Instruction.node_index = consumer_index;
                            Instruction.relative_length = 1;
                            Instruction.element_num = Instruction.relative_length * AG_output_element_size[AG0_index_in_total];
//                            Instruction.input_element_in_total = input_element_in_total;
                            Instruction.copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
                            if (output_visit_flag[output_index] == 0) // 如果提前没有访问过，就先把输入向量搬运过去。
                            {
                                output_visit_flag[output_index] = 1;
                                Instruction.type = VEC1OP;
                                Instruction.operation = "VM";
                                Instruction.source = AG0_index_in_total;
                                Instruction.destination = AG0_index_in_total;
                                Instruction.rs_offset = input_index * Instruction.element_num;
//                                Instruction.rd_offset_in_output = output_index * Instruction.element_num;
//                                Instruction.rd_offset = input_element_in_total + Instruction.rd_offset_in_output;
                                Instruction.rd_offset = input_element_in_total + output_index * Instruction.element_num;
                            }
                            else
                            {
                                Instruction.type = VEC2OP;
                                Instruction.operation = "VVMAX";
                                Instruction.source_1 = AG0_index_in_total;
                                Instruction.source_2 = AG0_index_in_total;
                                Instruction.destination = AG0_index_in_total;
                                Instruction.rs1_offset = input_index * Instruction.element_num;
//                                Instruction.rs2_offset_in_output = output_index * Instruction.element_num;
//                                Instruction.rs2_offset = input_element_in_total + Instruction.rs_offset_in_output;
                                Instruction.rs2_offset = input_element_in_total + input_index * Instruction.element_num;
                                Instruction.rd_offset = Instruction.rs2_offset;
                            }
                            int real_instruction_group_index = instruction_group_index;
                            PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[AG0_core_index].instruction_ir_list.push_back(Instruction);
                        }
                    }
                }
                else
                {
//                    int effective_provider_index = PIMCOM_node_list[node_index].AG0_node_index;
//                    struct INST Instruction;
//                    Instruction.type = VEC1OP;
//                    Instruction.level_index = PIMCOM_node_list[consumer_index].level_index;
//                    Instruction.operation = consumer_op;
                    std::cout << "  not considered post op:" << consumer_op << "  node_index:" << consumer_index << std::endl;
//                    Instruction.node_index = consumer_index;
//                    Instruction.source = AG0_index_in_total;
//                    Instruction.destination = AG0_index_in_total;
//                    Instruction.relative_length = PIMCOM_4_input_cycle_record[effective_provider_index].size();
//                    Instruction.element_num = Instruction.relative_length * AG_output_element_size[AG0_index_in_total];
//                    Instruction.copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
//                    int real_instruction_group_index = instruction_group_index;
//                    PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[AG0_core_index].instruction_ir_list.push_back(Instruction);
                }
                ScheduleNaiveStage5(consumer_index, level_index, instruction_group_index);
            }
        }
    }
}

int InferencePipelineSchedule::GetInputChannelFromOutputIndex(int node_index, int output_index, bool is_last)
{
    struct PIMCOM_node Node = PIMCOM_node_list[node_index];
    struct param Params = Node.param;
    int input_H = Node.input_dim[2];
    int input_W = Node.input_dim[3];
    int conv_kernel_w = Params.kernel_w;
    int conv_kernel_h = Params.kernel_h;
    int conv_padding_h0 = Params.pad_h0;
    int conv_padding_h1 = Params.pad_h1;
    int conv_padding_w0 = Params.pad_w0;
    int conv_padding_w1 = Params.pad_w1;
    int conv_stride_w = Params.stride_w;
    int conv_stride_h = Params.stride_h;

    int output_W = floor(float(input_W + conv_padding_w0 + conv_padding_w1 - conv_kernel_w) / float(conv_stride_w)) + 1;
    int output_H = floor(float(input_H + conv_padding_h0 + conv_padding_h1 - conv_kernel_h) / float(conv_stride_h)) + 1;
    int info_output_W = Node.output_dim[3];
    int info_output_H = Node.output_dim[2];
    if (info_output_W != output_W || info_output_H != output_H)
    {
        std::cout << " Output Size Doesn't Match" << std::endl;
        return -1;
    }
    int normal_start_index_in_w = conv_padding_w0/conv_stride_w + (conv_padding_w0 % conv_stride_w == 0 ? 0 : 1);
    int normal_start_index_in_h = conv_padding_h0/conv_stride_h + (conv_padding_h0 % conv_stride_h == 0 ? 0 : 1);

    int i = output_index / output_W;
    int j = output_index % output_W;
    int start_address = i * conv_stride_h * input_W + j *  conv_stride_w;
    if (j < normal_start_index_in_w)
        start_address -= (j * conv_stride_w);
    else
        start_address -= conv_padding_w0;
    if (i < normal_start_index_in_h)
        start_address -= (i * conv_stride_h * input_W);
    else
        start_address -= conv_padding_h0 * input_W;
    
    int start_row = start_address / input_W;
    int start_col = start_address % input_W;

    int conv_w_num = conv_kernel_w;
    if (j < normal_start_index_in_w)
        conv_w_num = conv_w_num - conv_padding_w0 + j * conv_stride_w;
    if (start_col + conv_kernel_w > input_W)
        conv_w_num = conv_w_num - (start_col + conv_kernel_w - input_W);

    int conv_h_num = conv_kernel_h;
    if (i < normal_start_index_in_h)
        conv_h_num = conv_h_num - conv_padding_h0 + i * conv_stride_h;
    if (start_row + conv_kernel_h > input_H)
        conv_h_num = conv_h_num - (start_row + conv_kernel_h - input_H);

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
void InferencePipelineSchedule::ScheduleNaiveStage6( int node_index, int level_index, int mode, int instruction_group_index)
{
    // 每次向前跳一步。所以只用检测visit_stage6[node_index]是否等于0即可。
    if (visit_stage6[node_index] != 0)
        return;
    visit_stage6[node_index] = 1;
    int consumer_num = PIMCOM_node_list[node_index].consumer_num;
    if (consumer_num == 0)
    {
        int provider_AG_index = PIMCOM_node_list[node_index].AG0_index_in_total;
        int provider_core = PIMCOM_node_list[node_index].AG0_core_index;
        int output_dim_num_t = PIMCOM_node_list[node_index].output_dim_num;
        int output_element_num = 1;
        for (int k = 0; k < output_dim_num_t; ++k)
        {
            output_element_num *= PIMCOM_node_list[node_index].output_dim[k];
        }
        struct INST Instruction_st;
        Instruction_st.type = MEM;
        Instruction_st.level_index = level_index;
        Instruction_st.level_diff = 0;
        Instruction_st.operation = "ST";
        Instruction_st.node_index = node_index;
        Instruction_st.stage = "OUTPUT";
        Instruction_st.source = provider_AG_index;
        Instruction_st.destination = provider_AG_index;
        Instruction_st.rs_offset = 0;
        Instruction_st.rd_offset_between_inference = output_element_num;
        Instruction_st.rd_offset = -1;
        Instruction_st.element_num = output_element_num;
        Instruction_st.instruction_group_index = instruction_group_index;
        PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[provider_core].instruction_ir_list.push_back(Instruction_st);
        return;
    }
    else
    {
        for (int i = 0; i < consumer_num; ++i)
        {
            int consumer_index = PIMCOM_node_list[node_index].consumer_index[i];
            int consumer_level = PIMCOM_node_list[consumer_index].level_index;
            int consumer_core = PIMCOM_node_list[consumer_index].AG0_core_index;
            int provider_core = PIMCOM_node_list[node_index].AG0_core_index;
            int provider_AG_index = PIMCOM_node_list[node_index].AG0_index_in_total;
            // 注意这时的node_index一般都是ReLU，而不是CONV或FC。所以要找到其生产者CONV或FC，所以需要effective_node_index
            int effective_node_index = PIMCOM_node_list[node_index].AG0_node_index;
            int consumer_AG_index = PIMCOM_node_list[consumer_index].AG0_index_in_total;
            switch (mode)
            {
                case 0:
                {
                    if (consumer_level == level_index)
                    {
                        if (consumer_core != provider_core)
                        {
//                          std::cout << "[Comm] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                    << "core_" << consumer_core << " node_" << consumer_index << " AG_" << consumer_AG_index << std::endl;
                            struct INST Instruction_send;
                            Instruction_send.type = COMM;
                            Instruction_send.level_index = PIMCOM_node_list[consumer_index].level_index;
                            Instruction_send.operation = "SEND";
                            Instruction_send.to_core = consumer_core;
                            Instruction_send.source = provider_AG_index;
                            Instruction_send.rs_offset = 0;
                            Instruction_send.relative_length = PIMCOM_4_input_cycle_record[effective_node_index].size();
                            Instruction_send.element_num = Instruction_send.relative_length * AG_output_element_size[provider_AG_index];
//                          int real_instruction_group_index = (node_offset_inference[AG0_index_in_total]-1)/operation_cycle_before_comm;
                            int real_instruction_group_index = instruction_group_index;
                            Instruction_send.instruction_group_index = real_instruction_group_index;
                            PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[provider_core].instruction_ir_list.push_back(Instruction_send);

                            struct INST Instruction_recv;
                            Instruction_recv.type = COMM;
                            Instruction_recv.level_index = PIMCOM_node_list[consumer_index].level_index;
                            Instruction_recv.operation = "RECV";
                            Instruction_recv.from_core = provider_core;
                            Instruction_recv.destination = provider_AG_index;
                            Instruction_recv.rd_offset = 0;
                            Instruction_recv.relative_length = Instruction_send.relative_length;
                            Instruction_recv.element_num = Instruction_recv.relative_length * AG_output_element_size[provider_AG_index];
                            Instruction_recv.instruction_group_index = real_instruction_group_index;
                            PIMCOM_4_post_instruction_ir[real_instruction_group_index].core_list[consumer_core].instruction_ir_list.push_back(Instruction_recv);
                        }
                        else if (PIMCOM_node_list[consumer_index].copy_offset_flag == 1)
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
                        if (PIMCOM_node_list[consumer_index].operation == "OP_CONV")
                        {
                            int comm_num = PIMCOM_4_recv_info.node_list[consumer_index].size();
                            for (int j = 0; j < comm_num; ++j)
                            {
                                int recv_core = PIMCOM_4_recv_info.node_list[consumer_index][j].core_index;
                                int recv_replication = PIMCOM_4_recv_info.node_list[consumer_index][j].replication_index;
                                int recv_AG_index = PIMCOM_4_recv_info.node_list[consumer_index][j].AG_index;
                                if (PIMCOM_node_list[node_index].operation == "OP_INPUT") // Load Data From Global Memory
                                {
                                    int effective_consumer_index = PIMCOM_node_list[consumer_index].effective_node_index;
                                    int first_output_index = PIMCOM_2_AG_partition[effective_consumer_index].replication[recv_replication].input_cycle_this_start;
                                    int last_output_index = PIMCOM_2_AG_partition[effective_consumer_index].replication[recv_replication].input_cycle_this_end;
                                    int channel_num = GetInputChannelFromOutputIndex( consumer_index, last_output_index, 1) - GetInputChannelFromOutputIndex( consumer_index, first_output_index, 0);
                                    int channel_length = PIMCOM_node_list[consumer_index].param.input_channel;
                                    int input_dim_num = PIMCOM_node_list[node_index].output_dim_num;
                                    int input_element_num = 1;
                                    for (int k = 0; k < input_dim_num; ++k)
                                    {
                                        input_element_num *= PIMCOM_node_list[node_index].output_dim[k];
                                    }
                                    struct INST Instruction_ld;
                                    Instruction_ld.type = MEM;
                                    Instruction_ld.level_index = 0;
                                    Instruction_ld.level_diff = 0;
                                    Instruction_ld.operation = "LD";
                                    Instruction_ld.stage = "INPUT";
                                    Instruction_ld.source = -1;
                                    Instruction_ld.destination = recv_AG_index;
                                    // TODO: rs_offset
                                    Instruction_ld.rs_offset_between_inference = input_element_num;
                                    Instruction_ld.rs_offset_in_inference = channel_length * GetInputChannelFromOutputIndex(consumer_index, first_output_index, 0);
                                    Instruction_ld.rs_offset = -1;
                                    Instruction_ld.rd_offset = 0;
                                    Instruction_ld.element_num = channel_num * channel_length;
                                    Instruction_ld.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[recv_core].instruction_ir_list.push_back(Instruction_ld);
                                }
                                else if (recv_core != provider_core)
                                {
//                                    std::cout << "[Comm] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                              << "core_" << recv_core << " node_" << consumer_index << " AG_" << recv_AG_index << " replication_" << recv_replication << std::endl;
                                    int effective_consumer_index = PIMCOM_node_list[consumer_index].effective_node_index;
                                    int first_output_index = PIMCOM_2_AG_partition[effective_consumer_index].replication[recv_replication].input_cycle_this_start;
                                    int last_output_index = PIMCOM_2_AG_partition[effective_consumer_index].replication[recv_replication].input_cycle_this_end;
//                                    std::cout << " start_position:" << GetInputChannelFromOutputIndex(DNNInfo, consumer_index, first_output_index, 0) << std::endl;
//                                    std::cout << " end_position:" << GetInputChannelFromOutputIndex(DNNInfo, consumer_index, last_output_index, 1) << std::endl;
                                    int channel_num = GetInputChannelFromOutputIndex(consumer_index, last_output_index, 1) - GetInputChannelFromOutputIndex(consumer_index, first_output_index, 0);
                                    int channel_length = PIMCOM_node_list[consumer_index].param.input_channel;

                                    struct INST  Instruction_send;
                                    Instruction_send.type = COMM;
                                    Instruction_send.level_index = level_index;
                                    Instruction_send.operation = "SEND";
                                    Instruction_send.to_core = recv_core;
                                    Instruction_send.source = provider_AG_index;
                                    Instruction_send.relative_length = 0;
                                    Instruction_send.rs_offset = channel_length * GetInputChannelFromOutputIndex(consumer_index, first_output_index, 0);
                                    Instruction_send.element_num = channel_num * channel_length;
                                    Instruction_send.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[provider_core].instruction_ir_list.push_back(Instruction_send);

                                    struct INST  Instruction_recv;
                                    Instruction_recv.type = COMM;
                                    Instruction_recv.level_index = level_index;
                                    Instruction_recv.operation = "RECV";
                                    Instruction_recv.from_core = provider_core;
                                    Instruction_recv.relative_length = 0;
                                    Instruction_recv.destination = recv_AG_index;
                                    Instruction_recv.element_num = channel_num * channel_length;
                                    Instruction_recv.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[recv_core].instruction_ir_list.push_back(Instruction_recv);
                                }
                            }
                        }
                        else if (PIMCOM_node_list[consumer_index].operation == "OP_FC")
                        {
                            int comm_num = PIMCOM_4_recv_info.node_list[consumer_index].size();
                            for (int j = 0; j < comm_num; ++j)
                            {
                                if (PIMCOM_node_list[PIMCOM_node_list[consumer_index].provider_index[0]].operation == "OP_INPUT")
                                    continue;
                                int recv_core = PIMCOM_4_recv_info.node_list[consumer_index][j].core_index;
                                int recv_AG_index = PIMCOM_4_recv_info.node_list[consumer_index][j].AG_index;
                                if (PIMCOM_node_list[node_index].operation == "OP_INPUT") // Load Data From Global Memory
                                {
                                    int start_offset = PIMCOM_4_recv_info.node_list[consumer_index][j].start_offset_element;
                                    int recv_element = PIMCOM_4_recv_info.node_list[consumer_index][j].recv_element;
                                    int input_dim_num = PIMCOM_node_list[node_index].output_dim_num;
                                    int input_element_num = 1;
                                    for (int k = 0; k < input_dim_num; ++k)
                                    {
                                        input_element_num *= PIMCOM_node_list[node_index].output_dim[k];
                                    }

                                    struct INST  Instruction_ld;
                                    Instruction_ld.type = MEM;
                                    Instruction_ld.level_index = node_index;
                                    Instruction_ld.level_diff = 0;
                                    Instruction_ld.operation = "LD";
                                    Instruction_ld.node_index = 0;
                                    Instruction_ld.stage = "INPUT";
                                    Instruction_ld.source = 0;
                                    Instruction_ld.destination = recv_AG_index;
                                    Instruction_ld.rs_offset_between_inference = input_element_num;
                                    Instruction_ld.rs_offset_in_inference = start_offset;
                                    Instruction_ld.rs_offset = -1;
                                    Instruction_ld.rd_offset = 0;
                                    Instruction_ld.element_num = recv_element;
                                    Instruction_ld.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[recv_core].instruction_ir_list.push_back(Instruction_ld);
                                }
                                else if (recv_core != provider_core)
                                {
//                                    std::cout << "[Comm] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                              << "core_" << recv_core << " node_" << consumer_index << " AG_" << recv_AG_index  << std::endl;
                                    int start_offset = PIMCOM_4_recv_info.node_list[consumer_index][j].start_offset_element;
                                    int recv_element = PIMCOM_4_recv_info.node_list[consumer_index][j].recv_element;
//                                    std::cout << " start_offset:" << start_offset << " recv_element:" << recv_element << std::endl;

                                    struct INST  Instruction_send;
                                    Instruction_send.type = COMM;
                                    Instruction_send.level_index = level_index;
                                    Instruction_send.operation = "SEND";
                                    Instruction_send.stage = 7;
                                    Instruction_send.relative_length = 0;
                                    Instruction_send.to_core = recv_core;
                                    Instruction_send.source = provider_AG_index;
                                    Instruction_send.rs_offset = start_offset;
                                    Instruction_send.element_num = recv_element;
                                    Instruction_send.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[provider_core].instruction_ir_list.push_back(Instruction_send);

                                    struct INST  Instruction_recv;
                                    Instruction_recv.type = COMM;
                                    Instruction_recv.level_index = level_index;
                                    Instruction_recv.operation = "RECV";
                                    Instruction_recv.stage = 7;
                                    Instruction_recv.relative_length = 0;
                                    Instruction_recv.from_core = provider_core;
                                    Instruction_recv.destination = recv_AG_index;
                                    Instruction_recv.element_num = recv_element;
                                    Instruction_recv.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[recv_core].instruction_ir_list.push_back(Instruction_recv);
                                }
                            }
                        }
                        else
                        {
                            if (consumer_core != provider_core)
                            {
//                                std::cout << "[Comm] from core_" << provider_core <<" node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                          << "core_" << consumer_core<<" node_" << consumer_index  << " AG_" << consumer_AG_index << std::endl;
                                int output_dim_num = PIMCOM_node_list[node_index].output_dim_num;
                                int output_dim = 1;
                                for (int j = 0; j < output_dim_num; ++j)
                                {
                                    output_dim *= PIMCOM_node_list[node_index].output_dim[j];
                                }
//                                std::cout << output_dim << std::endl;

                                struct INST  Instruction_send;
                                Instruction_send.type = COMM;
                                Instruction_send.level_index = level_index;
                                Instruction_send.operation = "SEND";
                                Instruction_send.stage = 7;
                                Instruction_send.to_core = consumer_core;
                                Instruction_send.source = provider_AG_index;
                                Instruction_send.rs_offset = 0;
                                Instruction_send.element_num = output_dim;
                                Instruction_send.instruction_group_index = instruction_group_index;
                                PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[provider_core].instruction_ir_list.push_back(Instruction_send);

                                struct INST  Instruction_recv;
                                Instruction_recv.type = COMM;
                                Instruction_recv.level_index = level_index;
                                Instruction_recv.operation = "RECV";
                                Instruction_recv.stage = 7;
                                Instruction_recv.rd_offset = 0;
                                Instruction_recv.from_core = provider_core;
                                Instruction_recv.destination = provider_AG_index; // the same to send_source
                                Instruction_recv.element_num = output_dim;
                                Instruction_recv.instruction_group_index = instruction_group_index;
                                PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[consumer_core].instruction_ir_list.push_back(Instruction_recv);
                            }
                        }
                    }
                    else if  (consumer_level-level_index > 1)
                    {
                        if (PIMCOM_node_list[consumer_index].operation == "OP_CONV")
                        {
                            int comm_num = PIMCOM_4_recv_info.node_list[consumer_index].size();
                            for (int j = 0; j < comm_num; ++j)
                            {
                                int recv_core = PIMCOM_4_recv_info.node_list[consumer_index][j].core_index;
                                int recv_replication = PIMCOM_4_recv_info.node_list[consumer_index][j].replication_index;
                                int recv_AG_index = PIMCOM_4_recv_info.node_list[consumer_index][j].AG_index;
                                if (recv_core != provider_core)
                                {
                                    if (PIMCOM_node_list[PIMCOM_node_list[consumer_index].provider_index[0]].operation == "OP_INPUT")
                                        continue;
//                                    std::cout << "[Cache] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                              << "core_" << recv_core << " node_" << consumer_index << " AG_" << recv_AG_index << " replication_" << recv_replication << std::endl;
                                    int effective_consumer_index = PIMCOM_node_list[consumer_index].effective_node_index;
                                    int first_output_index = PIMCOM_2_AG_partition[effective_consumer_index].replication[recv_replication].input_cycle_this_start;
                                    int last_output_index = PIMCOM_2_AG_partition[effective_consumer_index].replication[recv_replication].input_cycle_this_end;
//                                    std::cout << " start_position:" << GetInputChannelFromOutputIndex(consumer_index, first_output_index, 0) << std::endl;
//                                    std::cout << " end_position:" << GetInputChannelFromOutputIndex(consumer_index, last_output_index, 1) << std::endl;
                                    int channel_num = GetInputChannelFromOutputIndex(consumer_index, last_output_index, 1) - GetInputChannelFromOutputIndex(consumer_index, first_output_index, 0);
                                    int channel_length = PIMCOM_node_list[consumer_index].param.input_channel;

                                    struct INST  Instruction_st;
                                    Instruction_st.type = MEM;
                                    Instruction_st.level_index = level_index;
                                    Instruction_st.level_diff = consumer_level - level_index;
                                    Instruction_st.operation = "ST";
                                    Instruction_st.node_index = node_index;
                                    Instruction_st.source = provider_AG_index;
                                    Instruction_st.destination = recv_AG_index;
                                    Instruction_st.rs_offset = channel_length * GetInputChannelFromOutputIndex(consumer_index, first_output_index, 0);
                                    Instruction_st.rd_offset_unit = (channel_num * channel_length);
                                    Instruction_st.rd_offset = -1;
                                    Instruction_st.element_num = channel_num * channel_length;
                                    Instruction_st.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[provider_core].instruction_ir_list.push_back(Instruction_st);

                                    struct INST  Instruction_ld;
                                    Instruction_ld.type = MEM;
                                    Instruction_ld.level_index = consumer_level-1;
                                    Instruction_ld.level_diff = consumer_level - level_index;
                                    Instruction_ld.operation = "LD";
                                    Instruction_ld.node_index = consumer_index;
                                    Instruction_ld.source = recv_AG_index;
                                    Instruction_ld.destination = recv_AG_index;
                                    Instruction_ld.rs_offset_unit = (channel_num * channel_length);
                                    Instruction_ld.rs_offset = -1;
                                    Instruction_ld.rd_offset = 0;
                                    Instruction_ld.element_num = channel_num * channel_length;
                                    Instruction_ld.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[recv_core].instruction_ir_list.push_back(Instruction_ld);
                                }
                            }
                        }
                        else if (PIMCOM_node_list[consumer_index].operation == "OP_FC")
                        {
                            int comm_num = PIMCOM_4_recv_info.node_list[consumer_index].size();
                            for (int j = 0; j < comm_num; ++j)
                            {
                                if (PIMCOM_node_list[PIMCOM_node_list[consumer_index].provider_index[0]].operation == "OP_INPUT")
                                    continue;
                                int recv_core = PIMCOM_4_recv_info.node_list[consumer_index][j].core_index;
                                int recv_AG_index = PIMCOM_4_recv_info.node_list[consumer_index][j].AG_index;
                                if (recv_core != provider_core)
                                {
//                                    std::cout << "[Cache] from core_" << provider_core << " node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                              << "core_" << recv_core << " node_" << consumer_index << " AG_" << recv_AG_index  << std::endl;
                                    int start_offset = PIMCOM_4_recv_info.node_list[consumer_index][j].start_offset_element;
                                    int recv_element = PIMCOM_4_recv_info.node_list[consumer_index][j].recv_element;
//                                    std::cout << " start_offset:" << start_offset << " recv_element:" << recv_element << std::endl;

                                    struct INST  Instruction_st;
                                    Instruction_st.type = MEM;
                                    Instruction_st.level_index = level_index;
                                    Instruction_st.level_diff = consumer_level - level_index;
                                    Instruction_st.operation = "ST";
                                    Instruction_st.source = provider_AG_index;
                                    Instruction_st.destination = recv_AG_index;
                                    Instruction_st.rs_offset = start_offset;
                                    Instruction_st.rd_offset_unit = recv_element;
                                    Instruction_st.rd_offset = -1;
                                    Instruction_st.element_num = recv_element;
                                    Instruction_st.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[provider_core].instruction_ir_list.push_back(Instruction_st);

                                    struct INST  Instruction_ld;
                                    Instruction_ld.type = MEM;
                                    Instruction_ld.level_index = consumer_level-1;
                                    Instruction_ld.level_diff = consumer_level - level_index;
                                    Instruction_ld.operation = "LD";
                                    Instruction_ld.source = recv_AG_index;
                                    Instruction_ld.destination = recv_AG_index;
                                    Instruction_ld.rs_offset_unit = recv_element;
                                    Instruction_ld.rs_offset = -1;
                                    Instruction_ld.rd_offset = 0;
                                    Instruction_ld.element_num = recv_element;
                                    Instruction_ld.instruction_group_index = instruction_group_index;
                                    PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[recv_core].instruction_ir_list.push_back(Instruction_ld);
                                }
                            }
                        }
                        else
                        {
                            if (consumer_core != provider_core)
                            {
//                                std::cout << "[Cache] from core_" << provider_core <<" node_" << node_index << " AG_" << provider_AG_index << " TO "
//                                          << "core_" << consumer_core<<" node_" << consumer_index  << " AG_" << consumer_AG_index << std::endl;
                                int output_dim_num = PIMCOM_node_list[node_index].output_dim_num;
                                int output_dim = 1;
                                for (int j = 0; j < output_dim_num; ++j)
                                {
                                    output_dim *= PIMCOM_node_list[node_index].output_dim[j];
                                }
//                                std::cout << output_dim << std::endl;

                                struct INST  Instruction_st;
                                Instruction_st.type = MEM;
                                Instruction_st.level_index = level_index;
                                Instruction_st.level_diff = consumer_level - level_index;
                                Instruction_st.operation = "ST";
                                Instruction_st.source = provider_AG_index;
                                Instruction_st.destination = provider_AG_index;
                                Instruction_st.rs_offset = 0;
                                Instruction_st.rd_offset = -1;
                                Instruction_st.rd_offset_unit = output_dim;
                                Instruction_st.element_num = output_dim;
                                Instruction_st.instruction_group_index = instruction_group_index;
                                PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[provider_core].instruction_ir_list.push_back(Instruction_st);

                                struct INST  Instruction_ld;
                                Instruction_ld.type = MEM;
                                Instruction_ld.level_index = consumer_level-1;
                                Instruction_ld.level_diff = consumer_level - level_index;
                                Instruction_ld.operation = "LD";
                                Instruction_ld.source = provider_AG_index;
                                Instruction_ld.destination = provider_AG_index; // the same to send_source
                                Instruction_ld.rs_offset_unit = output_dim;
                                Instruction_ld.rs_offset = -1;
                                Instruction_ld.rd_offset = 0;
                                Instruction_ld.element_num = output_dim;
                                Instruction_ld.instruction_group_index = instruction_group_index;
                                PIMCOM_4_post_instruction_ir[instruction_group_index].core_list[consumer_core].instruction_ir_list.push_back(Instruction_ld);
                            }
                        }
                    }
                }
            }
            ScheduleNaiveStage6(consumer_index, consumer_level, mode, instruction_group_index);
        }
    }
}

void InferencePipelineSchedule::AddSeparateLine( int instruction_group_index)
{
    for (int i = 0; i < core_num; ++i)
    {
        struct INST Instruction_sep;
        Instruction_sep.operation = "sep";
        Instruction_sep.level_index = 0;
        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_sep);
    }
}

void InferencePipelineSchedule::FillTheWholeInstructionGroup()
{
    // Clean And Fill node_offset_inference
    for (int i = 0; i < MAX_AG; ++i)
        node_offset_inference[i] = 0;
    int AG_num = PIMCOM_3_hierarchy_map.whole.size();
    for (int i = 0; i < AG_num; ++i)
    {
        int AG_index_in_total = PIMCOM_3_hierarchy_map.whole_index[i];
        int input_cycle_this_replication = PIMCOM_3_hierarchy_map.whole[i][0].input_cycle_this_replication;
        node_offset_inference[AG_index_in_total] = input_cycle_this_replication;
    }

    // Clean And Fill DNNInfo["6_input_cycle_record"]
    PIMCOM_4_input_cycle_record.clear();
    PIMCOM_4_input_cycle_record.resize(node_num);
    int effective_node_index = PIMCOM_2_AG_partition.size();
    for (int i = 0; i < effective_node_index; ++i)
    {
        int node_index = PIMCOM_2_AG_partition[i].index;
        int input_cycle_in_total = PIMCOM_2_AG_partition[i].input_cycle_in_total;
        for (int j = 0; j < input_cycle_in_total; ++j)
        {
            PIMCOM_4_input_cycle_record[node_index].push_back(j);
        }
    }
}

int InferencePipelineSchedule::GetEffectiveInstructionGroupNum()
{
    int effective_instruction_group_num = 0;
    int AG_num_in_total = PIMCOM_3_hierarchy_map.whole.size();
    for (int i = 0; i < AG_num_in_total; ++i)
    {
        // 得到整个结构最终instruction_group_num
        int AG_index = PIMCOM_3_hierarchy_map.whole_index[i];
        int AG_instruction_group_num = PIMCOM_3_hierarchy_map.whole[i][0].instruction_group_num;
        int core_index = PIMCOM_3_hierarchy_map.whole[i][0].vcore_index;
        if (AG_instruction_group_num > effective_instruction_group_num)
            effective_instruction_group_num = AG_instruction_group_num;
        // 得到全部出现过的instruction_group_num
        PIMCOM_4_unique_instruction_group_index.insert(AG_instruction_group_num);
        // 为每个CONV或FC节点生成instruction_group_num
        int node_index = PIMCOM_3_hierarchy_map.whole[i][0].node_index;
        if (PIMCOM_node_list[node_index].instruction_group_num == 0)
            PIMCOM_node_list[node_index].instruction_group_num = AG_instruction_group_num;
        else if (PIMCOM_node_list[node_index].instruction_group_num < AG_instruction_group_num)
            PIMCOM_node_list[node_index].instruction_group_num = AG_instruction_group_num;
        if (PIMCOM_4_core_instruction_group_num.count(core_index) == 0)
            PIMCOM_4_core_instruction_group_num[core_index] = AG_instruction_group_num;
        else if (PIMCOM_4_core_instruction_group_num[core_index] < AG_instruction_group_num)
            PIMCOM_4_core_instruction_group_num[core_index] = AG_instruction_group_num;
    }
    return effective_instruction_group_num;
}

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

void InferencePipelineSchedule::ScheduleNaiveScheduleOnePostOperation(int instruction_group_index, int post_node_index)
{
    int appointed_core_num = 23;
    struct PIMCOM_node PostOperationNode = PIMCOM_node_list[post_node_index];
    int level_index = PostOperationNode.level_index;
    int copy_offset_flag = PostOperationNode.copy_offset_flag;
    int AG0_index_in_total = PostOperationNode.AG0_index_in_total;
    if (PostOperationNode.operation == "OP_POOL")
    {
        int output_channel_length = PostOperationNode.output_dim[1];
        int output_channel_num_total = PostOperationNode.output_dim[2] * PostOperationNode.output_dim[3]; // == input_sliding_window_num
        ResetPostStartAndEndAddress(output_channel_num_total, appointed_core_num);
        int load_offset = 0;
        for (int i = 0; i < appointed_core_num; ++i)
        {
            int output_channel_start = post_start_address[i];
            int output_channel_end = post_end_address[i];
//            std::cout << output_channel_start << "  " << output_channel_end << std::endl;
            int input_channel_start = GetInputChannelFromOutputIndex(post_node_index, output_channel_start, 0);
            int input_channel_end = GetInputChannelFromOutputIndex(post_node_index, output_channel_end, 1);

            struct INST Instruction_ld;
            Instruction_ld.node_index = post_node_index;
            Instruction_ld.type = MEM;
            Instruction_ld.level_index = level_index;
            Instruction_ld.level_diff = 0;
            Instruction_ld.operation = "LD";
            Instruction_ld.stage = "POST";
            Instruction_ld.source = AG0_index_in_total;
            Instruction_ld.destination = AG0_index_in_total;
            Instruction_ld.rs_offset = input_channel_start * output_channel_length; // POOL:input_channel_length == output_channel_length
            Instruction_ld.rd_offset = 0;
            Instruction_ld.element_num = (input_channel_end - input_channel_start + 1) * output_channel_length;
            load_offset += Instruction_ld.element_num;
            Instruction_ld.instruction_group_index = instruction_group_index;
            PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_ld);

            for (int j = output_channel_start; j <= output_channel_end; ++j)
            {
                int associated_input_num = PIMCOM_conv_pool_input_output_info[post_node_index].output_index[j].size();
                for (int k = 0; k < associated_input_num; ++k)
                {
                    int output_channel_index = j;
                    int input_channel_index = PIMCOM_conv_pool_input_output_info[post_node_index].output_index[j][k];
                    struct INST Instruction_pool;
                    Instruction_pool.node_index = PostOperationNode.index;
                    Instruction_pool.level_index = level_index;
                    Instruction_pool.operation = "POST-POOL";
                    Instruction_pool.element_num = output_channel_length;
                    Instruction_pool.copy_offset_flag = PostOperationNode.copy_offset_flag;

                    if (k == 0) // 如果提前没有访问过，就先把输入向量搬运过去。
                    {
                        Instruction_pool.type = VEC1OP;
                        Instruction_pool.operation = "VM";
                        Instruction_pool.source = AG0_index_in_total;
                        Instruction_pool.destination = AG0_index_in_total;
                        Instruction_pool.rs_offset =  (input_channel_index - input_channel_start) * output_channel_length;
                        Instruction_pool.rd_offset = (output_channel_index - output_channel_start) * output_channel_length;
                    }
                    else
                    {
                        Instruction_pool.type = VEC2OP;
                        Instruction_pool.operation = "VVMAX";
                        Instruction_pool.source_1 = AG0_index_in_total;
                        Instruction_pool.source_2 = AG0_index_in_total;
                        Instruction_pool.destination = AG0_index_in_total;
                        Instruction_pool.rs1_offset = (input_channel_index - input_channel_start) * output_channel_length;
                        Instruction_pool.rs2_offset = (output_channel_index - output_channel_start) * output_channel_length;
                        Instruction_pool.rd_offset = Instruction_pool.rs2_offset;
                    }
                    PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_pool);
                }
            }

            struct INST Instruction_st;
            Instruction_st.node_index = post_node_index;
            Instruction_st.type = MEM;
            Instruction_st.level_index = level_index;
            Instruction_st.level_diff = 0;
            Instruction_st.operation = "ST";
            Instruction_st.stage = "POST";
            Instruction_st.source = AG0_index_in_total;
            Instruction_st.destination = AG0_index_in_total;
            Instruction_st.rs_offset = load_offset;
            Instruction_st.rd_offset = output_channel_start * output_channel_length; // POOL:input_channel_length == output_channel_length
            Instruction_st.element_num = (output_channel_end - output_channel_start + 1) * output_channel_length;
            Instruction_st.instruction_group_index = instruction_group_index;
            PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_st);
        }
    }
    else if (PostOperationNode.operation ==  "OP_RELU")
    {
        int output_channel_length = PostOperationNode.output_dim[1];
        int output_channel_num_total = PostOperationNode.output_dim[2] * PostOperationNode.output_dim[3];
        ResetPostStartAndEndAddress(output_channel_num_total, appointed_core_num);
        for (int i = 0; i < appointed_core_num; ++i)
        {
            int input_channel_start = post_start_address[i];
            int input_channel_end = post_end_address[i];
            int load_offset = 0;
            std::vector<int> load_address;
            struct INST Instruction_ld;
            Instruction_ld.node_index = post_node_index;
            Instruction_ld.type = MEM;
            Instruction_ld.level_index = level_index;
            Instruction_ld.level_diff = 0;
            Instruction_ld.operation = "LD";
            Instruction_ld.stage = "POST";
            Instruction_ld.source = AG0_index_in_total;
            Instruction_ld.destination = AG0_index_in_total;
            Instruction_ld.rs_offset = input_channel_start * output_channel_length;
            Instruction_ld.rd_offset = load_offset;
            Instruction_ld.element_num = (input_channel_end - input_channel_start + 1) * output_channel_length;
            load_address.push_back(load_offset);
            load_offset += Instruction_ld.element_num;
            Instruction_ld.instruction_group_index = instruction_group_index;
            PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_ld);
            
            for (int k = input_channel_start; k <= input_channel_end; ++k)
            {
                struct INST Instruction_elt;
                Instruction_elt.type = VEC1OP;
                Instruction_elt.level_index = level_index;
                Instruction_elt.level_diff = 0;
                Instruction_elt.operation = "VRELU";
                Instruction_elt.stage = "POST";
                Instruction_elt.source = AG0_index_in_total;
                Instruction_elt.destination = AG0_index_in_total;
                Instruction_elt.rs_offset = (k-input_channel_start) * output_channel_length;
                Instruction_elt.rd_offset = load_offset + (k-input_channel_start) * output_channel_length;
                Instruction_elt.element_num = output_channel_length;
                Instruction_elt.instruction_group_index = instruction_group_index;
                PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_elt);
            }

            struct INST Instruction_st;
            Instruction_st.node_index = post_node_index;
            Instruction_st.type = MEM;
            Instruction_st.level_index = level_index;
            Instruction_st.level_diff = 0;
            Instruction_st.operation = "ST";
            Instruction_st.stage = "POST";
            Instruction_st.source = AG0_index_in_total;
            Instruction_st.destination = AG0_index_in_total;
            Instruction_st.rs_offset = load_offset; // 最终该load_offset就是load全部元素量
            Instruction_st.rd_offset = input_channel_start * output_channel_length;
            Instruction_st.element_num = (input_channel_end - input_channel_start + 1) * output_channel_length;
            Instruction_st.instruction_group_index = instruction_group_index;
            PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_st);
        }
    }
    else if (PostOperationNode.operation ==  "OP_ELTWISE")
    {
        int output_channel_length = PostOperationNode.output_dim[1];
        int output_channel_num_total = PostOperationNode.output_dim[2] * PostOperationNode.output_dim[3];
        ResetPostStartAndEndAddress(output_channel_num_total, appointed_core_num);
        int elt_type = PostOperationNode.param.eletype;
        std::string elt_operation;
        switch (elt_type)
        {   case 2: elt_operation = "VADD"; break;
            case 4: elt_operation = "VSUB"; break; }

        for (int i = 0; i < appointed_core_num; ++i)
        {
            int input_channel_start = post_start_address[i];
            int input_channel_end = post_end_address[i];
            int load_offset = 0;
            std::vector<int> load_address;
            for (int j = 0; j < PostOperationNode.provider_num; ++j)
            {
                int provider_index = PostOperationNode.provider_index[j];
                int provider_channel_length = PIMCOM_node_list[provider_index].output_dim[1];
                struct INST Instruction_ld;
                Instruction_ld.type = MEM;
                Instruction_ld.node_index = post_node_index;
                Instruction_ld.level_index = level_index;
                Instruction_ld.level_diff = 0;
                Instruction_ld.operation = "LD";
                Instruction_ld.stage = "POST";
                Instruction_ld.source = PIMCOM_node_list[provider_index].AG0_index_in_total;
                Instruction_ld.destination = AG0_index_in_total;
                Instruction_ld.rs_offset = input_channel_start * provider_channel_length;
                Instruction_ld.rd_offset = load_offset;
                Instruction_ld.element_num = (input_channel_end - input_channel_start + 1) * provider_channel_length;
                load_address.push_back(load_offset);
                load_offset += Instruction_ld.element_num;
                Instruction_ld.instruction_group_index = instruction_group_index;
                PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_ld);
            }

            for (int j = 1; j < PostOperationNode.provider_num; ++j)
            {
                int provider_index = PostOperationNode.provider_index[j];
                int provider_channel_length = PIMCOM_node_list[provider_index].output_dim[1];
                for (int k = input_channel_start; k <= input_channel_end; ++k)
                {
                    struct INST Instruction_elt;
                    Instruction_elt.type = VEC2OP;
                    Instruction_elt.level_index = level_index;
                    Instruction_elt.level_diff = 0;
                    Instruction_elt.operation = elt_operation;
                    Instruction_elt.stage = "POST";
                    Instruction_elt.source_1 = AG0_index_in_total;
                    Instruction_elt.source_2 = AG0_index_in_total;
                    Instruction_elt.destination = AG0_index_in_total;
                    if (j == 1)
                        Instruction_elt.rs1_offset = load_address[0] + (k-input_channel_start) * output_channel_length;
                    else
                        Instruction_elt.rs1_offset = load_offset + (k-input_channel_start) * output_channel_length;
                    Instruction_elt.rs2_offset = load_address[j] + (k-input_channel_start) * output_channel_length;
                    Instruction_elt.rd_offset = load_offset + (k-input_channel_start) * output_channel_length;
                    Instruction_elt.element_num = provider_channel_length;
                    Instruction_elt.instruction_group_index = instruction_group_index;
                    PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_elt);
                }
            }

            struct INST Instruction_st;
            Instruction_st.node_index = post_node_index;
            Instruction_st.type = MEM;
            Instruction_st.level_index = level_index;
            Instruction_st.level_diff = 0;
            Instruction_st.operation = "ST";
            Instruction_st.stage = "POST";
            Instruction_st.source = AG0_index_in_total;
            Instruction_st.destination = AG0_index_in_total;
            Instruction_st.rs_offset = load_offset; // 最终该load_offset就是load全部元素量
            Instruction_st.rd_offset = input_channel_start * output_channel_length;
            Instruction_st.element_num = (input_channel_end - input_channel_start + 1) * output_channel_length;
            Instruction_st.instruction_group_index = instruction_group_index;
            PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_st);
        }
    }
    else if (PostOperationNode.operation ==  "OP_CONCAT")
    {
        int output_channel_length = PostOperationNode.output_dim[1];
        int output_channel_num_total = PostOperationNode.output_dim[2] * PostOperationNode.output_dim[3];
        ResetPostStartAndEndAddress(output_channel_num_total, appointed_core_num);
        int store_offset = 0;
        for (int i = 0; i < appointed_core_num; ++i)
        {
            // For CONCAT, input_channel_index == output_channel_index
            int input_channel_start = post_start_address[i];
            int input_channel_end = post_end_address[i];
            int load_total = 0;
            int load_offset = 0;
            for (int j = 0; j < PostOperationNode.provider_num; ++j)
            {
                int provider_index = PostOperationNode.provider_index[j];
                int provider_channel_length = PIMCOM_node_list[provider_index].output_dim[1];
                struct INST Instruction_ld;
                Instruction_ld.type = MEM;
                Instruction_ld.node_index = post_node_index;
                Instruction_ld.level_index = level_index;
                Instruction_ld.level_diff = 0;
                Instruction_ld.operation = "LD";
                Instruction_ld.stage = "POST";
                Instruction_ld.source = PIMCOM_node_list[provider_index].AG0_index_in_total;
                Instruction_ld.destination = PIMCOM_node_list[post_node_index].AG0_index_in_total;
                Instruction_ld.rs_offset = input_channel_start * provider_channel_length;
                Instruction_ld.rd_offset = load_offset;
                Instruction_ld.element_num = (input_channel_end - input_channel_start + 1) * provider_channel_length;
                load_offset += Instruction_ld.element_num;
                Instruction_ld.instruction_group_index = instruction_group_index;
                PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_ld);
            }

            int source_offset = 0;
            int destination_offset = 0;
            for (int j = 0; j < PostOperationNode.provider_num; ++j)
            {
                int provider_index = PostOperationNode.provider_index[j];
                int provider_channel_length = PIMCOM_node_list[provider_index].output_dim[1];
                for (int k = input_channel_start; k <= input_channel_end; ++k)
                {
                    struct INST Instruction_vm;
                    Instruction_vm.type = VEC1OP;
                    Instruction_vm.level_index = level_index;
                    Instruction_vm.level_diff = 0;
                    Instruction_vm.operation = "VM";
                    Instruction_vm.stage = "POST";
                    Instruction_vm.source = AG0_index_in_total;
                    Instruction_vm.destination = AG0_index_in_total;
                    Instruction_vm.rs_offset = source_offset;
                    Instruction_vm.rd_offset = (k-input_channel_start) * output_channel_length + destination_offset + (input_channel_end-input_channel_start+1)*output_channel_length;
                    Instruction_vm.element_num = provider_channel_length;
                    source_offset += Instruction_vm.element_num;
                    Instruction_vm.instruction_group_index = instruction_group_index;
                    PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_vm);
                }
                destination_offset += provider_channel_length;
            }

            struct INST Instruction_st;
            Instruction_st.type = MEM;
            Instruction_st.node_index = post_node_index;
            Instruction_st.level_index = level_index;
            Instruction_st.level_diff = 0;
            Instruction_st.operation = "ST";
            Instruction_st.stage = "POST";
            Instruction_st.source = AG0_index_in_total;
            Instruction_st.destination = AG0_index_in_total;
            Instruction_st.rs_offset = load_offset; // 最终该load_offset就是load全部元素量
            Instruction_st.rd_offset = store_offset;
            Instruction_st.element_num = (input_channel_end - input_channel_start + 1) * output_channel_length;
            store_offset += Instruction_st.element_num;
            Instruction_st.instruction_group_index = instruction_group_index;
            PIMCOM_4_post_multi_core_instruction_ir[instruction_group_index].core_list[i].instruction_ir_list.push_back(Instruction_st);
        }
    }
}

//void InferencePipelineSchedule::ScheduleNaivePickOnePostOperation()
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

void InferencePipelineSchedule::ScheduleNaivePickOnePostOperation()
{
    std::set <int> complete_node;
    std::set <int> wait_node;
    std::set <int> ready_node;
    for (int i = 0; i < node_num; ++i)
    {
        std::string operation = PIMCOM_node_list[i].operation;
        if (operation == "OP_INPUT" || operation == "OP_CONV"|| operation == "OP_FC")
        {
            complete_node.insert(i);
        }
        else if(operation == "OP_RELU")
        {
            int provider_node_index = PIMCOM_node_list[i].provider_index[0];
            std::string provider_operation = PIMCOM_node_list[provider_node_index].operation;
            if (provider_operation ==  "OP_CONV" || provider_operation ==  "OP_FC")
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
        for (int j = 0; j < PIMCOM_node_list[node_index].provider_num; ++j)
        {
            int provider_index = PIMCOM_node_list[node_index].provider_index[j];
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
        std::string post_operation = PIMCOM_node_list[pick].operation;
        if (post_operation == "OP_CONCAT" || post_operation == "OP_RELU" || post_operation == "OP_POOL" || post_operation == "OP_ELTWISE")
            ScheduleNaiveScheduleOnePostOperation(post_cycle, pick);
        else
            std::cout << post_operation << std::endl;
        wait_node.erase(pick);
        complete_node.insert(pick);
        ready_node.clear();

        for (std::set<int>::iterator i = wait_node.begin(); i != wait_node.end(); i++)
        {
            int node_index = *i;
            bool ready = true;
            for (int j = 0; j < PIMCOM_node_list[node_index].provider_num; ++j)
            {
                int provider_index = PIMCOM_node_list[node_index].provider_index[j];
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



void InferencePipelineSchedule::ScheduleNaive()
{
    clock_t time_use = 0;
    // TODO：未考虑是否死锁。或许会出现这种情况。
    int effective_instruction_group_num = GetEffectiveInstructionGroupNum();
    int instruction_group_num = user_given_instruction_group_num > effective_instruction_group_num ? effective_instruction_group_num : user_given_instruction_group_num;
    PIMCOM_4_base_instruction_ir.resize(instruction_group_num);
    PIMCOM_4_input_cycle_record.resize(node_num);
    bool append_instruction = 1;
    std::cout << " instruction_group_num:" << instruction_group_num << std::endl;
    for (int j = 0; j < instruction_group_num; ++j)
    {
        int instruction_group_index;
        if (AllInOnInstructionGroup)
            instruction_group_index = 0;
        else
            instruction_group_index = j;
        for (int k = 0; k < operation_cycle_before_comm; k++)
        {   clock_t timestamp_a = clock();
            ScheduleNaiveStage1(instruction_group_index, append_instruction);
            clock_t timestamp_b = clock();
            time_use += timestamp_b - timestamp_a;
            ScheduleNaiveStage2(instruction_group_index, append_instruction);
            for (int & n : add_flag) {n = 0;}
        }
        //// Stage3的作用是融合同一个复制块的计算结果，得到完整的结果
        ScheduleNaiveStage3(instruction_group_index, append_instruction);
        for (int & n : comm_flag) {n = 0;}
        //// StageACT的作用是为每个复制块的计算结果添加激活层
        ScheduleNaiveStageAct(instruction_group_index, append_instruction);
        for (int & n : activate_flag) {n = 0;}
        for (int & n : node_offset_instruction_group) {n = 0;}
        for (int l = 0; l < MAX_AG; ++l) {node_offset_inference_old[l] = node_offset_inference[l];}
    }
    //// 注意不能加sep，因为其type还不确定。
//    AddSeparateLine(instruction_group_num-1);
    //// 情况并且填满node_offset_inference和input_cycle_record，主要是为了后面这些操作是处理完整数据
//    FillTheWholeInstructionGroup();
    //// Stage4的作用是把不同复制块的数据传到一起，以方便下一步后处理的开展
//    ScheduleNaiveStage4WithoutWB(0);
    //// mode为0的Stage6两个作用:同level节点间传输数据、产生copy_offset_flag
//    ScheduleNaiveStage6(0, 0, 0, 0);
    for (int & n : visit_stage6) {n = 0;}
    //// Stage5的作用是添加后处理指令
//    ScheduleNaiveStage5(0, 0, 0);
    for (int & n : visit_stage5) {n = 0;}
    for (int & n : wb_flag) {n = 0;}
    //// mode为1的Stage6的作用是将本轮推理周期产生的数据进行传递，以便下一个推理周期的运行
//    ScheduleNaiveStage6(0, 0, 1, 0);
    for (int & n : visit_stage6) {n = 0;}
    for (int & n : node_offset_inference) {n = 0;}
    for (int & n : AG_accumulated_num) {n = 0;}
//    ScheduleNaivePickOnePostOperation();
    std::cout << double(USE) / CLOCKS_PER_SEC << "s" << std::endl;
    std::cout << double(time_use) / CLOCKS_PER_SEC << "s" << std::endl;
}


void InferencePipelineSchedule::ScheduleNaiveForEvaluation()
{
    clock_t time_use = 0;
    // TODO：未考虑是否死锁。或许会出现这种情况。
    int effective_instruction_group_num = GetEffectiveInstructionGroupNum();
    int instruction_group_num = user_given_instruction_group_num > effective_instruction_group_num ? effective_instruction_group_num : user_given_instruction_group_num;
    PIMCOM_4_base_instruction_ir.resize(instruction_group_num);
    PIMCOM_4_input_cycle_record.resize(node_num);
    bool append_instruction = 1;
    std::cout << "instruction_group_num:" << instruction_group_num << std::endl;
    int operation_cycle = 0;
    for (int j = 0; j < instruction_group_num; ++j)
    {
        // 同时记录两个
        if (PIMCOM_4_unique_instruction_group_index.count(j+1) == 0 && PIMCOM_4_unique_instruction_group_index.count(j) == 0)
        {
            operation_cycle += operation_cycle_before_comm;
            continue;
        }
//        std::cout << "instruction_group_index:" << j << " operation_cycle:" << operation_cycle << std::endl;
        PIMCOM_4_evaluation_instruction_group_index.push_back(j);
        for (int k = 0; k < operation_cycle_before_comm; k++)
        {
            clock_t timestamp_a = clock();
            ScheduleNaiveStage1ForEvaluation(j, operation_cycle, append_instruction);
            clock_t timestamp_b = clock();
            time_use += timestamp_b - timestamp_a;
            ScheduleNaiveStage2(j, append_instruction);
            for (int & n : add_flag) {n = 0;}
            operation_cycle += 1;
        }
        //// Stage3的作用是融合同一个复制块的计算结果，得到完整的结果
        ScheduleNaiveStage3(j, append_instruction);
        for (int & n : comm_flag) {n = 0;}
        //// StageACT的作用是为每个复制块的计算结果添加激活层
        ScheduleNaiveStageAct(j, append_instruction);
        for (int & n : activate_flag) {n = 0;}
        for (int & n : node_offset_instruction_group) {n = 0;}
        for (int l = 0; l < MAX_AG; ++l) {node_offset_inference_old[l] = node_offset_inference[l];}
    }
    //// 注意不能加sep，因为其type还不确定。
//    AddSeparateLine(instruction_group_num-1);
    //// 情况并且填满node_offset_inference和input_cycle_record，主要是为了后面这些操作是处理完整数据
//    FillTheWholeInstructionGroup();
    //// Stage4的作用是把不同复制块的数据传到一起，以方便下一步后处理的开展
//    ScheduleNaiveStage4WithoutWB(0);
    //// mode为0的Stage6两个作用:同level节点间传输数据、产生copy_offset_flag
//    ScheduleNaiveStage6(0, 0, 0, 0);
    for (int & n : visit_stage6) {n = 0;}
    //// Stage5的作用是添加后处理指令
//    ScheduleNaiveStage5(0, 0, 0);
    for (int & n : visit_stage5) {n = 0;}
    for (int & n : wb_flag) {n = 0;}
    //// mode为1的Stage6的作用是将本轮推理周期产生的数据进行传递，以便下一个推理周期的运行
//    ScheduleNaiveStage6(0, 0, 1, 0);
    for (int & n : visit_stage6) {n = 0;}
    for (int & n : node_offset_inference) {n = 0;}
    for (int & n : AG_accumulated_num) {n = 0;}
//    ScheduleNaivePickOnePostOperation();
    std::cout << "stage1 preparation: " << double(USE) / CLOCKS_PER_SEC << "s" << std::endl;
    std::cout << "stage1 total: " << double(time_use) / CLOCKS_PER_SEC << "s" << std::endl;
}


void InferencePipelineSchedule::Clear()
{
    for (int & n : AG_output_element_size) {n = 0;}
    for (int & n : node_offset_inference) {n = 0;}
    for (int & n : node_offset_inference_old) {n = 0;}
    comm_index = 0;
    USE = 0;
}


void InferencePipelineSchedule::ShowInstruction()
{
//    for (int inf = inference_start; inf <= inference_end ; ++inf)
//    {
//        std::cout << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;

//        std::cout << std::endl;
//        int instruction_group_num_1 = PIMCOM_4_base_instruction_ir.size();
//        for (int i = 0; i < instruction_group_num_1; ++i)
//        {
//            std::cout << std::endl;
//            std::cout << "========================================= base instruction_group " << i << " =========================================" << std::endl;
//            for (int j = 0; j < core_num; ++j)
//            {
//                std::cout << "core " << j << std::endl;
//                int instruction_num = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list.size();
//                for (int k = 0; k < instruction_num; ++k)
//                {
//                    struct INST Instruction = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list[k];
//                    int instruction_level_index = Instruction.level_index;
//                    if (instruction_level_index > inf)
//                    {
//                        continue;
//                    }
//                    ShowSingleInstructionFast(Instruction, inf);
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
//                ShowSingleInstructionFast(Instruction, inf);
//            }
//        }

//        std::cout << std::endl;
//        int instruction_group_num_2 = PIMCOM_4_post_multi_core_instruction_ir.size();
//        for (int i = 0; i < instruction_group_num_2; ++i)
//        {
//            std::cout << std::endl;
//            std::cout << "========================================= post multi core instruction_group " << i << " =========================================" << std::endl;
//            int post_multi_core_num = PIMCOM_4_post_multi_core_instruction_ir[i].core_list.size();
//            for (int j = 0; j < post_multi_core_num; ++j)
//            {
//                std::cout << "core " << j << std::endl;
//                int instruction_num = PIMCOM_4_post_multi_core_instruction_ir[i].core_list[j].instruction_ir_list.size();
//                for (int k = 0; k < instruction_num; ++k)
//                {
//                    struct INST Instruction = PIMCOM_4_post_multi_core_instruction_ir[i].core_list[j].instruction_ir_list[k];
//                    int instruction_level_index = Instruction.level_index;
//                    if (instruction_level_index > inf)
//                    {
//                        continue;
//                    }
//                    ShowSingleInstructionFast(Instruction, inf);
//                }
//            }
//        }
//    }
}


void InferencePipelineSchedule::SaveInstruction()
{
    std::ofstream OutFile("../fast.txt", std::ios::out | std::ios::trunc);
    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        OutFile << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;

        int instruction_group_num_1 = PIMCOM_4_base_instruction_ir.size();
        for (int i = 0; i < instruction_group_num_1; ++i)
        {
            OutFile << "========================================= base instruction_group " << i << " =========================================" << std::endl;
            for (int j = 0; j < core_num; ++j)
            {
                OutFile << "core " << j << std::endl;
                int instruction_num = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list.size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    struct INST Instruction = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list[k];
                    int instruction_level_index = Instruction.level_index;
                    if (instruction_level_index > inf)
                    {
                        continue;
                    }
                    SaveSingleInstructionFast(OutFile, Instruction, inf);
                }
            }
        }

        OutFile << "========================================= post instruction_group " << " =========================================" << std::endl;
        for (int j = 0; j < core_num; ++j)
        {
            OutFile << "core " << j << std::endl;
            int instruction_num = PIMCOM_4_post_instruction_ir[0].core_list[j].instruction_ir_list.size();
            for (int k = 0; k < instruction_num; ++k)
            {
                struct INST Instruction = PIMCOM_4_post_instruction_ir[0].core_list[j].instruction_ir_list[k];
                int instruction_level_index = Instruction.level_index;
                if (instruction_level_index > inf)
                {
                    continue;
                }
                SaveSingleInstructionFast(OutFile, Instruction, inf);
            }
        }

        int instruction_group_num_2 = PIMCOM_4_post_multi_core_instruction_ir.size();
        std::map<int, PIMCOM_4_instruction_ir>::iterator iter;
        int index = 0;
        for (iter = PIMCOM_4_post_multi_core_instruction_ir.begin(); iter != PIMCOM_4_post_multi_core_instruction_ir.end() ; iter++)
        {
            OutFile << "========================================= post multi core instruction_group " << index << " =========================================" << std::endl;
            int post_multi_core_num = iter->second.core_list.size();
            for (int j = 0; j < post_multi_core_num; ++j)
            {
                OutFile << "core " << j << std::endl;
                int instruction_num = iter->second.core_list[j].instruction_ir_list.size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    struct INST Instruction = iter->second.core_list[j].instruction_ir_list[k];
                    int instruction_level_index = Instruction.level_index;
                    if (instruction_level_index > inf)
                    {
                        continue;
                    }
                    SaveSingleInstructionFast(OutFile, Instruction, inf);
                }
            }
            index++;
        }
    }
    OutFile.close();
}



//void InferencePipelineSchedule::SaveJsonIR(, std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/6_es.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}