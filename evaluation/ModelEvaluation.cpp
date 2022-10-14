//
// Created by SXT on 2022/9/20.
//

#include "ModelEvaluation.h"

ModelEvaluation::ModelEvaluation(enum Mode RunMode)
{
    EvalMode = RunMode;
    instruction_group_num = PIMCOM_4_base_instruction_ir.size();
    core_num = PIMCOM_4_virtual_core_AG_map.core_list.size();
    std::cout << "instruction_group_num:" << PIMCOM_4_base_instruction_ir.size() << std::endl;
}

void ModelEvaluation::EvaluationInstruction()
{
    int MVMUL_num = 0;
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list.size();
        struct core_schedule current_core = PIMCOM_4_virtual_core_AG_map.core_list[i];
        for (int j = 0; j < AG_num; ++j)
        {
            int AG_index_in_total = current_core.AG_list[j].AG_index_in_total;
            // 注意: 这里的start和end都包括，也就是input_cycle数量为end-start+1
            int input_cycle_this_replication = current_core.AG_list[j].input_cycle_this_replication;
            MVMUL_num += input_cycle_this_replication;
        }
    }
}


void ModelEvaluation::EvaluationMVMUL()
{
    std::vector<int> EvaMVMUL;
    EvaMVMUL.resize(core_num);
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num_in_core = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list.size();
        for (int j = 0; j < AG_num_in_core; ++j)
        {
            EvaMVMUL[i] += PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].input_cycle_this_replication;
        }
    }
}


void ModelEvaluation::EvaluateCompute()
{
    switch (EvalMode)
    {
        case Generation:
        {
            if (AllInOnInstructionGroup)
            {
                //// One Instruction Group
                EvaluateRecursionSingleInstructionGroup(0, 0, 0);
                ShowEvaluationResultSingleInstructionGroup();
                ResetSingleInstructionGroup(0);
                break;
            }
            else
            {
                //// Whole Model (Generation)
                for (int i = 0; i < instruction_group_num; ++i)
                {
//                    std::cout << i << std::endl;
                    EvaluateRecursionSingleInstructionGroup(i, 0, 0);
                    if (i == instruction_group_num-1)
                        ShowEvaluationResultSingleInstructionGroup();
                    ResetSingleInstructionGroup(0);
                }
                break;
            }
        }
        case Exploration:
        {
            //// Whole Model Fast
            EvaluateRecursionWholeModel();
            ShowEvaluationResultWholeModel();
            break;
        }
    }
    Clear();
}


void ModelEvaluation::EvaluateMemory()
{
//    BaseMemoryUsage();
    BaseMemoryUsageImproved();
    ShowBaseMemoryUsage();
    Clear();
}


static int base_visited_record[MAX_AG] = {0};

static float base_core_memory_usage[MAX_CORE] = {0};
static float base_core_memory_usage_recv[MAX_CORE] = {0};
static float base_core_memory_usage_input[MAX_CORE] = {0};
static float base_core_memory_usage_output[MAX_CORE] = {0};

static float base_node_memory_usage[MAX_NODE] = {0};

static float base_AG_memory_usage[MAX_AG] = {0};
static float base_AG_memory_usage_input[MAX_AG] = {0};
static float base_AG_memory_usage_output[MAX_AG] = {0};

void ModelEvaluation::BaseMemoryUsage()
{
    //////////////////////////////////////////// Core Memory Static ////////////////////////////////////////////
    // 只遍历一个instruction_group，只有output需要乘上instruction_group_num。其他的都可以复用
    for (int i = 0; i < core_num; ++i)
    {
        float memory_usage = 0;
        std::vector<struct INST> InstructionIRList = PIMCOM_4_base_instruction_ir[0].core_list[i].instruction_ir_list;
        int instruction_num = InstructionIRList.size();
        for (int j = 0; j < instruction_num; ++j)
        {
            struct INST Instruction = InstructionIRList[j];
            if (Instruction.operation == "MVMUL")
            {
                if (Instruction.conv_or_fc == "OP_CONV")
                {
                    int node_index = Instruction.node_index;
                    int AG_index_in_total = Instruction.AG_index_in_total;
                    int replication_index = Instruction.replication_index;
                    int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
                    int instruction_group_num_ori = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].instruction_group_num;
                    int instruction_group_num_eva;
                    if (appointed_instruction_group_num == 0)
                    {
                        instruction_group_num_eva = ceil(float(instruction_group_num_ori) / float(instruction_group_reload_num));
                    }
                    else
                    {
                        instruction_group_num_eva = appointed_instruction_group_num < instruction_group_num_ori ? appointed_instruction_group_num : instruction_group_num_ori;
                    }
                    if (base_visited_record[AG_index_in_total] == 0)
                    {
                        base_visited_record[AG_index_in_total] = 1;
                        int kernel_h = PIMCOM_node_list[node_index].param.kernel_h;
                        int stride_h = PIMCOM_node_list[node_index].param.stride_h;
                        int input_width = PIMCOM_node_list[node_index].input_dim[3];
                        int input_channel_length = PIMCOM_node_list[node_index].input_dim[1];
                        int input_num = (instruction_group_num_eva * operation_cycle_before_comm);
                        int row_num = kernel_h + stride_h* ((floor(float(input_num)/float(input_width)) + ((input_num % input_width) >= 2 ? 2 : 1))-1);
                        for (int k = 0; k < PIMCOM_4_recv_info.node_list[node_index].size(); ++k)
                        {
                            if (PIMCOM_4_recv_info.node_list[node_index][k].AG_index == AG_index_in_total)
                            {
                                base_AG_memory_usage_input[AG_index_in_total] += row_num * input_width * input_channel_length;
                                base_core_memory_usage_input[i] += row_num * input_width * input_channel_length;
                                memory_usage += row_num * input_width * input_channel_length;
                                break;
                            }
                        }

//                        for (int k = 0; k < PIMCOM_4_recv_info.node_list[node_index].size(); ++k)
//                        {
//                            if (PIMCOM_4_recv_info.node_list[node_index][k].AG_index == AG_index_in_total)
//                            {
//                                base_AG_memory_usage_input[AG_index_in_total] += input_num * kernel_h * kernel_h * input_channel_length;
//                                base_core_memory_usage_input[i] += input_num * kernel_h * kernel_h * input_channel_length;
//                                memory_usage += input_num * kernel_h * kernel_h * input_channel_length;
//                                break;
//                            }
//                        }
                        base_AG_memory_usage_input[AG_index_in_total] += Instruction.input_element_num;
                        memory_usage += Instruction.input_element_num;
                        base_AG_memory_usage_output[AG_index_in_total] += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
                        base_core_memory_usage_output[i] += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
                        memory_usage += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
                    }
                }
                else
                {
                    int AG_index_in_total = Instruction.AG_index_in_total;
                    base_AG_memory_usage_input[AG_index_in_total] += Instruction.input_element_num;
                    base_core_memory_usage_input[i] += Instruction.input_element_num;
                    base_AG_memory_usage_output[AG_index_in_total] += Instruction.output_element_num;
                    base_core_memory_usage_output[i] += Instruction.output_element_num;
                    memory_usage += Instruction.input_element_num;
                    memory_usage += Instruction.output_element_num;
                }
            }
            else if (Instruction.operation == "VADD")
            {
                continue;
            }
            else if (Instruction.operation == "RECV")
            {
                int destination_AG_index = Instruction.destination;
                int node_index = PIMCOM_3_hierarchy_map.whole[destination_AG_index][0].node_index;
                int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
                int replication_index = PIMCOM_3_hierarchy_map.whole[destination_AG_index][0].replication_index;
                int instruction_group_num_ori = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].instruction_group_num;
                int instruction_group_num_eva;
                if (appointed_instruction_group_num == 0)
                    instruction_group_num_eva = ceil(float(instruction_group_num_ori) / float(instruction_group_reload_num));
                else
                    instruction_group_num_eva = appointed_instruction_group_num < instruction_group_num_ori ? appointed_instruction_group_num : instruction_group_num_ori;
                if (PIMCOM_node_list[node_index].operation == "OP_CONV")
                {
                    memory_usage += instruction_group_num_eva * Instruction.element_num;
                    base_core_memory_usage_recv[i] += instruction_group_num_eva * Instruction.element_num;
                }
                else if (PIMCOM_node_list[node_index].operation == "OP_FC")
                {
                    memory_usage += Instruction.element_num;
                    base_core_memory_usage_recv[i] += Instruction.element_num;
                }
            }
        }
        base_core_memory_usage[i] = memory_usage;
    }

    //////////////////////////////////////////// AG Memory Static ////////////////////////////////////////////
    int AG_num = PIMCOM_3_hierarchy_map.whole_index.size();
    for (int j = 0; j < AG_num; ++j)
    {
        base_AG_memory_usage[j] = base_AG_memory_usage_input[j] + base_AG_memory_usage_output[j];
    }


    //////////////////////////////////////////// Node Memory Static ////////////////////////////////////////////
    int effective_node_num = PIMCOM_2_effective_node.size();
    for (int i = 0; i < effective_node_num; ++i)
    {
        int node_index = PIMCOM_2_AG_partition[i].index;
        int replication_num = PIMCOM_2_AG_partition[i].replication_num;
        for (int j = 0; j < replication_num; ++j)
        {
            int AG_num_in_replication = PIMCOM_2_AG_partition[i].replication[j].AG_list.size();
            for (int k = 0; k < AG_num_in_replication; ++k)
            {
                int AG_index_in_total = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].AG_index;
                base_node_memory_usage[node_index] += base_AG_memory_usage_input[AG_index_in_total] + base_AG_memory_usage_output[AG_index_in_total];
            }
        }
    }
}


void ModelEvaluation::BaseMemoryUsageImproved()
{
    //////////////////////////////////////////// Core Memory Static ////////////////////////////////////////////
    // 只遍历一个instruction_group，只有output需要乘上instruction_group_num。其他的都可以复用
    for (int i = 0; i < core_num; ++i)
    {
        float memory_usage = 0;
        std::vector<struct INST> InstructionIRList = PIMCOM_4_base_instruction_ir[0].core_list[i].instruction_ir_list;
        int instruction_num = InstructionIRList.size();
        int last_recv_node;
        int last_recv_replication_index;
        int last_two_recv_node;
        int last_two_recv_replication_index;
        int comm_instruction_num = 0;
        for (int j = 0; j < instruction_num; ++j)
        {
            struct INST Instruction = InstructionIRList[j];
            if (Instruction.operation == "MVMUL")
            {
                if (Instruction.conv_or_fc == "OP_CONV")
                {
                    int node_index = Instruction.node_index;
                    int AG_index_in_total = Instruction.AG_index_in_total;
                    int replication_index = Instruction.replication_index;
                    int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
                    int instruction_group_num_ori = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].instruction_group_num;
                    int instruction_group_num_eva;
                    if (appointed_instruction_group_num == 0)
                    {
                        instruction_group_num_eva = ceil(float(instruction_group_num_ori) / float(instruction_group_reload_num));
                    }
                    else
                    {
                        instruction_group_num_eva = appointed_instruction_group_num < instruction_group_num_ori ? appointed_instruction_group_num : instruction_group_num_ori;
                    }
                    if (base_visited_record[AG_index_in_total] == 0)
                    {
                        base_visited_record[AG_index_in_total] = 1;
                        int kernel_h = PIMCOM_node_list[node_index].param.kernel_h;
                        int stride_h = PIMCOM_node_list[node_index].param.stride_h;
                        int input_width = PIMCOM_node_list[node_index].input_dim[3];
                        int input_channel_length = PIMCOM_node_list[node_index].input_dim[1];
                        int input_num = (instruction_group_num_eva * operation_cycle_before_comm);
                        //// 按行读取
//                        int row_num = kernel_h + stride_h* ((floor(float(input_num)/float(input_width)) + ((input_num % input_width) >= 2 ? 2 : 1))-1);
//                        for (int k = 0; k < PIMCOM_4_recv_info.node_list[node_index].size(); ++k)
//                        {
//                            if (PIMCOM_4_recv_info.node_list[node_index][k].AG_index == AG_index_in_total)
//                            {
//                                base_memory_usage_input[AG_index_in_total] += row_num * input_width * input_channel_length;
//                                memory_usage += row_num * input_width * input_channel_length;
//                                break;
//                            }
//                        }
                        //// 按窗口读取
                        for (int k = 0; k < PIMCOM_4_recv_info.node_list[node_index].size(); ++k)
                        {
                            if (PIMCOM_4_recv_info.node_list[node_index][k].AG_index == AG_index_in_total)
                            {
                                //// 输入(目前还未优化。应该是需要优化。因为窗口之间会有重叠，而目前没有考虑这部分)
                                base_AG_memory_usage_input[AG_index_in_total] += input_num * kernel_h * kernel_h * input_channel_length;
                                base_core_memory_usage_input[i] += input_num * kernel_h * kernel_h * input_channel_length;
                                memory_usage += input_num * kernel_h * kernel_h * input_channel_length;
//                                std::cout << input_num * kernel_h * kernel_h * input_channel_length << std::endl;
                                //// 输出(每个rep只用加一次即可，然而这与AG无关，所以base_AG_memory_usage_input其实不同管)
                                base_core_memory_usage_output[i] += input_num * Instruction.output_element_num;
                                memory_usage += input_num * Instruction.output_element_num;
                                //// 对于输出的更激进的优化（这里的1是比较理想的情况，MVMUL算完的时候VADD已经算完，否则要比1更大）
                                base_core_memory_usage_output[i] += 1 * Instruction.output_element_num;
                                memory_usage += 1 * Instruction.output_element_num;
                                break;
                            }
                        }
                        //// im2col开销
                        base_AG_memory_usage_input[AG_index_in_total] += Instruction.input_element_num;
                        memory_usage += Instruction.input_element_num;
                        //// output开销
//                        base_AG_memory_usage_output[AG_index_in_total] += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
//                        base_core_memory_usage_output[i] += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
//                        memory_usage += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
                        //// output开销（考虑空间复用）（如果采用更激进的情况，则不需要这部分，改为上面循环内部的写法）
//                        base_AG_memory_usage_output[AG_index_in_total] += Instruction.output_element_num;
//                        base_core_memory_usage_output[i] += Instruction.output_element_num;
//                        memory_usage += Instruction.output_element_num;
                    }
                }
                else
                {
                    int AG_index_in_total = Instruction.AG_index_in_total;
                    base_AG_memory_usage_input[AG_index_in_total] += Instruction.input_element_num;
                    base_core_memory_usage_input[i] += Instruction.input_element_num;
                    base_AG_memory_usage_output[AG_index_in_total] += Instruction.output_element_num;
                    base_core_memory_usage_output[i] += Instruction.output_element_num;
                    memory_usage += Instruction.input_element_num;
                    memory_usage += Instruction.output_element_num;
                }
            }
            else if (Instruction.operation == "VADD")
            {
                continue;
            }
            else if (Instruction.operation == "RECV")
            {
                int destination_AG_index = Instruction.destination;
                int node_index = PIMCOM_3_hierarchy_map.whole[destination_AG_index][0].node_index;
                int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
                int replication_index = PIMCOM_3_hierarchy_map.whole[destination_AG_index][0].replication_index;
                int instruction_group_num_ori = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].instruction_group_num;
                int instruction_group_num_eva;
                if (appointed_instruction_group_num == 0)
                    instruction_group_num_eva = ceil(float(instruction_group_num_ori) / float(instruction_group_reload_num));
                else
                    instruction_group_num_eva = appointed_instruction_group_num < instruction_group_num_ori ? appointed_instruction_group_num : instruction_group_num_ori;
                if (PIMCOM_node_list[node_index].operation == "OP_CONV")
                {
                    if (comm_instruction_num > 0 && last_recv_node != node_index && last_recv_replication_index != replication_index)
                    {
                        memory_usage += instruction_group_num_eva * Instruction.element_num;
                        base_core_memory_usage_recv[i] += instruction_group_num_eva * Instruction.element_num;
                    }
                }
                else if (PIMCOM_node_list[node_index].operation == "OP_FC")
                {
                    if (comm_instruction_num > 0 && last_recv_node != node_index)
                    {
                        memory_usage += Instruction.element_num;
                        base_core_memory_usage_recv[i] += Instruction.element_num;
                    }
                }
                //// 如果更实际一点，可能需要换成前两个节点或前三个节点，就改一改上面的判断条件即可
                if (comm_instruction_num > 1)
                {
                    last_two_recv_node = last_recv_node;
                    last_two_recv_replication_index = last_recv_replication_index;
                }
                if (comm_instruction_num > 0)
                {
                    last_recv_node = node_index;
                    last_recv_replication_index = replication_index;
                }
                comm_instruction_num++;
            }
        }
        base_core_memory_usage[i] = memory_usage;
    }

    //////////////////////////////////////////// AG Memory Static ////////////////////////////////////////////
    int AG_num = PIMCOM_3_hierarchy_map.whole_index.size();
    for (int j = 0; j < AG_num; ++j)
    {
        base_AG_memory_usage[j] = base_AG_memory_usage_input[j] + base_AG_memory_usage_output[j];
    }


    //////////////////////////////////////////// Node Memory Static ////////////////////////////////////////////
    int effective_node_num = PIMCOM_2_effective_node.size();
    for (int i = 0; i < effective_node_num; ++i)
    {
        int node_index = PIMCOM_2_AG_partition[i].index;
        int replication_num = PIMCOM_2_AG_partition[i].replication_num;
        for (int j = 0; j < replication_num; ++j)
        {
            int AG_num_in_replication = PIMCOM_2_AG_partition[i].replication[j].AG_list.size();
            for (int k = 0; k < AG_num_in_replication; ++k)
            {
                int AG_index_in_total = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].AG_index;
                base_node_memory_usage[node_index] += base_AG_memory_usage_input[AG_index_in_total] + base_AG_memory_usage_output[AG_index_in_total];
            }
        }
    }
}


void ModelEvaluation::ShowBaseMemoryUsage()
{
    std::cout << "============ core memory statistic ============" << std::endl;
    float core_memory_sum = 0.0;
    for (int i = 0; i < core_num; ++i)
    {
        std::cout << "core" << i << "  " << base_core_memory_usage[i] * 16 / 8 / 1024 << "KB" << std::endl;
        std::cout << "    input ratio:" << base_core_memory_usage_input[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        std::cout << "    output ratio:" << base_core_memory_usage_output[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        std::cout << "    recv ratio:" << base_core_memory_usage_recv[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        core_memory_sum += base_core_memory_usage[i];
    }
    std::cout << "sum:" << core_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;

//    std::cout << "============ AG memory statistic ============" << std::endl;
//    float ag_memory_sum = 0.0;
//    int AG_num = PIMCOM_3_hierarchy_map.whole_index.size();
//    for (int j = 0; j < AG_num; ++j)
//    {
//        std::cout << "AG" << j
//                  << "  input:" << std::left << std::setw(8) << base_AG_memory_usage_input[j] * 16 / 8 / 1024 << "KB"
//                  << "  output:" << std::left  << std::setw(8) << base_AG_memory_usage_output[j] * 16 / 8 / 1024 << "KB" << std::endl;
//        ag_memory_sum += base_AG_memory_usage[j];
//    }
//    std::cout << "sum:" << ag_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;


//    std::cout << "============ node memory statistic ============" << std::endl;
//    int effective_node_num = PIMCOM_2_effective_node.size();
//    float node_memory_sum = 0.0;
//    for (int i = 0; i < effective_node_num; ++i)
//    {
//        int node_index = PIMCOM_2_AG_partition[i].index;
//        node_memory_sum += base_node_memory_usage[node_index];
//        std::cout << "node" << node_index << "  "  << base_node_memory_usage[node_index] * 16 / 8 / 1024 << "KB" << std::endl;
//    }
//    std::cout << "sum:" << node_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;


//    std::cout << "============ core memory recv statistic ============" << std::endl;
//    float core_recv_sum = 0.0;
//    for (int i = 0; i < core_num; ++i)
//    {
//        std::cout << " core" << i <<  "  " << base_core_memory_usage_recv[i] * 16 / 8 / 1024 << "KB" << std::endl;
//        core_recv_sum += base_core_memory_usage_recv[i];
//    }
//    std::cout << "sum:" << core_recv_sum * 16 / 8 / 1024 << "KB" << std::endl;
}


static int MVMUL_single[MAX_CORE] = {0};
static int VADD_single[MAX_CORE] = {0};
static int COMM_single[MAX_CORE] = {0};
static int DELAY_single[MAX_CORE] = {0};
static int DELAY_single_old[MAX_CORE] = {0};
static int Visited_single[MAX_CORE] = {0};
void ModelEvaluation::EvaluateRecursionSingleInstructionGroup(int instruction_group_index, int core_index, int index_in_core)
{
    if (core_index >= core_num)
        return;
    Visited_single[core_index] = 1;
    std::vector<struct INST> InstructionIrList =  PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[core_index].instruction_ir_list;
    int instruction_ir_num = InstructionIrList.size();
    for (int k = index_in_core; k < instruction_ir_num; ++k)
    {
        struct INST tmpInstruction = InstructionIrList[k];
        if (tmpInstruction.operation == "SEND" || tmpInstruction.operation == "RECV")
        {
            int comm_index = tmpInstruction.comm_index;
            int instruction_index_in_core = tmpInstruction.instruction_index_in_core;
            if (comm_index_2_index_in_core_map.count(comm_index) == 0)
            {
                comm_index_2_index_in_core_map.insert(std::pair<int,int>(comm_index, instruction_index_in_core));
                comm_index_2_core_index.insert(std::pair<int,int>(comm_index, core_index));
                int next_core_index = core_index+1;
                while (Visited_single[next_core_index] != 0)
                {
                    next_core_index++;
                }
                EvaluateRecursionSingleInstructionGroup(instruction_group_index, next_core_index, 0);
            }
            else
            {
                int corresponding_core_index = comm_index_2_core_index[comm_index];
                int corresponding_instruction_index_in_core = comm_index_2_index_in_core_map[comm_index];
                if (DELAY_single[core_index] > DELAY_single[corresponding_core_index])
                    DELAY_single[corresponding_core_index] = DELAY_single[core_index];
                else
                    DELAY_single[core_index] = DELAY_single[corresponding_core_index];
                DELAY_single[corresponding_core_index] += COMM_delay;
                DELAY_single[core_index] += COMM_delay;
                COMM_single[corresponding_core_index]++;
                COMM_single[core_index]++;
                EvaluateRecursionSingleInstructionGroup(instruction_group_index, corresponding_core_index, corresponding_instruction_index_in_core+1);
                EvaluateRecursionSingleInstructionGroup(instruction_group_index, core_index, instruction_index_in_core+1);
            }
            return;
        }
        else if (tmpInstruction.operation == "MVMUL")
        {
            MVMUL_single[core_index]++;
            DELAY_single[core_index] += MVMUL_delay;
        }
        else if (tmpInstruction.operation == "VADD")
        {
            VADD_single[core_index]++;
            DELAY_single[core_index] += VECTOR_delay;
        }
//        if (k == instruction_ir_num-1)
//        {
//            int next_core_index = core_index+1;
//            while (next_core_index < core_num && Visited_single[next_core_index] != 0)
//            {
//                next_core_index++;
//            }
//            std::cout << next_core_index << std::endl;
//            EvaluateRecursionSingleInstructionGroup(instruction_group_index, next_core_index, 0);
//        }
    }
    //// 这么改是因为有的core的instruction_ir_num是0，但是下一个core仍然有指令。如果这部分在循环内，则后面核的指令都访问不到。
    int next_core_index = core_index+1;
    while (Visited_single[next_core_index] != 0)
    {
        next_core_index++;
    }
    EvaluateRecursionSingleInstructionGroup(instruction_group_index, next_core_index, 0);
}


static int MVMUL_whole[MAX_CORE] = {0};
static int VADD_whole[MAX_CORE] = {0};
static int COMM_whole[MAX_CORE] = {0};
void ModelEvaluation::EvaluateRecursionWholeModel()
{
//    for (auto it = PIMCOM_4_AG_instruction_group_num.begin(); it != PIMCOM_4_AG_instruction_group_num.cend(); it++)
//    {
//        std::cout << "AG:" << it->first << " " << it->second << std::endl;
//    }
//    for (auto it = PIMCOM_4_unique_instruction_group_index.begin(); it != PIMCOM_4_unique_instruction_group_index.cend(); it++)
//    {
//        std::cout << *it << std::endl;
//    }
    int evaluation_instruction_group_num = PIMCOM_4_evaluation_instruction_group_index.size();
    for (int i = 0; i < evaluation_instruction_group_num; ++i)
    {
        int instruction_group_index = PIMCOM_4_evaluation_instruction_group_index[i];
        EvaluateRecursionSingleInstructionGroup(instruction_group_index, 0, 0);
        if (PIMCOM_4_unique_instruction_group_index.count(instruction_group_index+1) == 1)
        {
            for (int j = 0; j < core_num; ++j)
            {
                MVMUL_whole[j] += MVMUL_single[j];
                VADD_whole[j] += VADD_single[j];
                COMM_whole[j] += COMM_single[j];
            }
        }
        else
        {
            int same_instruction_group_num ;
            if (i != evaluation_instruction_group_num-1)
                same_instruction_group_num = (PIMCOM_4_evaluation_instruction_group_index[i+1]-instruction_group_index);
            else
                same_instruction_group_num = instruction_group_num - instruction_group_index;
            for (int j = 0; j < core_num; ++j)
            {
                MVMUL_whole[j] += MVMUL_single[j] * same_instruction_group_num;
                VADD_whole[j] += VADD_single[j] * same_instruction_group_num;
                COMM_whole[j] += COMM_single[j] * same_instruction_group_num;
                DELAY_single[j] += (DELAY_single[j] - DELAY_single_old[j]) * (same_instruction_group_num-1);
            }
        }
        ResetSingleInstructionGroup(1);
    }
}


void ModelEvaluation::ShowEvaluationResultSingleInstructionGroup()
{
    long ideal = 0;
    long practical = 0;
    int MVMUL_num = 0;
    int VADD_num = 0;
    int COMM_num = 0;
    for (int i = 0; i < core_num; ++i)
    {
        if(MVMUL_single[i]*MVMUL_delay + VADD_single[i]*VECTOR_delay + COMM_single[i]*COMM_delay > ideal)
            ideal = MVMUL_single[i]*MVMUL_delay + VADD_single[i]*VECTOR_delay + COMM_single[i]*COMM_delay;
        if (DELAY_single[i] > practical)
            practical = DELAY_single[i];
        std::cout << i << "  MVMUL:" << MVMUL_single[i] << "  VADD:" << VADD_single[i] << " COMM:" << COMM_single[i] << " DELAY: " << DELAY_single[i] << std::endl;
        MVMUL_num += MVMUL_single[i];
        VADD_num += VADD_single[i];
        COMM_num += COMM_single[i];
    }
    std::cout << "ideal:" << ideal << std::endl;
    std::cout << "practical:" << practical << std::endl;
    std::cout << "MVMUL:" << MVMUL_num << std::endl;
    std::cout << "VADD:" << VADD_num << std::endl;
    std::cout << "COMM:" << COMM_num << std::endl;
}


void LinearFit(double abr[],double x[],double y[],int n);
void ModelEvaluation::ShowEvaluationResultWholeModel()
{
    double MVMUL_log[MAX_CORE] = {0.0};
    double DELAY_log[MAX_CORE] = {0.0};
    long ideal = 0;
    long practical = 0;
    int MVMUL_num = 0;
    int VADD_num = 0;
    int COMM_num = 0;
    std::ofstream MVMUL_log_file("../mvmul.txt", std::ios::out | std::ios::trunc);
    std::ofstream DELAY_log_file("../delay.txt", std::ios::out | std::ios::trunc);
    for (int i = 0; i < core_num; ++i)
    {
        if(MVMUL_whole[i]*MVMUL_delay + VADD_whole[i]*VECTOR_delay + COMM_whole[i]*COMM_delay > ideal)
            ideal = MVMUL_whole[i]*MVMUL_delay + VADD_whole[i]*VECTOR_delay + COMM_whole[i]*COMM_delay;
        std::cout << i << "  MVMUL:" << MVMUL_whole[i] << "  VADD:" << VADD_whole[i] << " COMM:" << COMM_whole[i] << " DELAY: " << DELAY_single[i]  << std::endl;
        if (DELAY_single[i] > practical)
            practical = DELAY_single[i];
        MVMUL_num += MVMUL_whole[i];
        VADD_num += VADD_whole[i];
        COMM_num += COMM_whole[i];
        MVMUL_log[i] = MVMUL_whole[i];
        MVMUL_log_file << std::to_string(MVMUL_log[i]) << std::endl;
        DELAY_log[i] = DELAY_single[i];
        DELAY_log_file << std::to_string(DELAY_log[i]) << std::endl;
    }
    std::cout << "ideal:" << ideal << std::endl;
    std::cout << "practical:" << practical << std::endl;
    std::cout << "MVMUL:" << MVMUL_num << std::endl;
    std::cout << "VADD:" << VADD_num << std::endl;
    std::cout << "COMM:" << COMM_num << std::endl;
    MVMUL_log_file.close();
    DELAY_log_file.close();
    double abr[3];
    LinearFit(abr, MVMUL_log, DELAY_log, core_num);
}

void LinearFit(double abr[],double x[],double y[],int n)
{//线性拟合ax+b
    double xsum, ysum, x2sum, xysum;
    xsum = 0; ysum = 0; x2sum = 0; xysum = 0;
    for (int  i = 0; i < n; i++)
    {
        xsum += x[i];
        ysum += y[i];
        x2sum += x[i] * x[i];
        xysum += x[i] * y[i];
    }
    abr[0] = (n*xysum - xsum * ysum) / (n*x2sum - xsum * xsum);//a
    abr[1] = (ysum - abr[0] * xsum) / n;//b
    double yavg = ysum / n;
    double dy2sum1 = 0, dy2sum2 = 0;
    for (int i = 0; i < n; i++)
    {
        dy2sum1 += ((abr[0] * x[i] + abr[1]) - yavg)*((abr[0] * x[i] + abr[1]) - yavg);//r^2的分子
        dy2sum2 += (y[i] - yavg)*(y[i] - yavg);//r^2的分母
    }
    abr[2] = dy2sum1 / dy2sum2;//r^2
    std::cout << "[fitting] a:" << abr[0] << std::endl;
    std::cout << "[fitting] b:" << abr[1] << std::endl;
    std::cout << "[fitting] r2:" << abr[2] << std::endl;
}


void ModelEvaluation::ResetSingleInstructionGroup(bool clear_other_info)
{
//    for (int & n : DELAY_single) {n = 0;}
    for (int & n : Visited_single) {n = 0;}
    if (clear_other_info)
    {
        for (int & n : MVMUL_single) {n = 0;}
        for (int & n : VADD_single) {n = 0;}
        for (int & n : COMM_single) {n = 0;}
        for (int i = 0; i < core_num; ++i)  DELAY_single_old[i] = DELAY_single[i];
    }
}

void ModelEvaluation::Clear()
{
    for (int & n : MVMUL_single) {n = 0;}
    for (int & n : VADD_single) {n = 0;}
    for (int & n : COMM_single) {n = 0;}
    for (int & n : DELAY_single) {n = 0;}
    for (int & n : DELAY_single_old) {n = 0;}
    for (int & n : Visited_single) {n = 0;}
    for (int & n : MVMUL_whole) {n = 0;}
    for (int & n : VADD_whole) {n = 0;}
    for (int & n : COMM_whole) {n = 0;}

    for (int & n : base_visited_record) {n = 0;}
    for (float & n : base_AG_memory_usage_input) {n = 0;}
    for (float & n : base_AG_memory_usage_output) {n = 0;}
    for (float & n : base_node_memory_usage) {n = 0;}
    for (float & n : base_core_memory_usage_recv) {n = 0;}

    for (float & n : base_core_memory_usage_input) {n = 0;}
    for (float & n : base_core_memory_usage_output) {n = 0;}
}