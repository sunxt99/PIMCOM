//
// Created by SXT on 2022/9/25.
//

#include "MemoryAllocation.h"

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;
extern std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
extern struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
extern std::vector<int> PIMCOM_2_effective_node;
extern struct PIMCOM_3_hierarchy_map PIMCOM_3_hierarchy_map;
extern std::map<int, std::vector<int>> PIMCOM_3_virtual_core_crossbar_map;
extern std::map<int,int> PIMCOM_4_physical_core_placement;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_4_physical_crossbar_placement;
//extern std::map<int, struct PIMCOM_5_pool_info> PIMCOM_5_pool_info;
extern std::vector<struct PIMCOM_5_pool_info> PIMCOM_5_pool_info;
extern std::map<int, int> PIMCOM_6_AG_instruction_group_num;
extern struct PIMCOM_6_first_AG_info PIMCOM_6_first_AG_info;
extern struct PIMCOM_6_physical_core_AG_map PIMCOM_6_physical_core_AG_map;
extern struct PIMCOM_6_recv_info PIMCOM_6_recv_info;
extern std::vector<struct PIMCOM_6_instruction_ir> PIMCOM_6_base_instruction_ir;
extern std::vector<std::vector<int>> PIMCOM_6_input_cycle_record;
extern std::map<int, struct PIMCOM_6_instruction_ir> PIMCOM_6_post_instruction_ir;
extern std::map<int, struct PIMCOM_6_instruction_ir> PIMCOM_6_post_multi_core_instruction_ir;

struct PIMCOM_7_reload_info PIMCOM_7_reload_info;
std::vector<struct PIMCOM_6_instruction_ir> PIMCOM_7_base_instruction_with_reload;


void MemoryAllocation::AllocateMemoryFast(Json::Value &DNNInfo)
{
    core_num = PIMCOM_3_virtual_core_crossbar_map.size();
//    BaseMemoryUsageInfoFast(DNNInfo);
    clock_t timestamp_1 = clock();
    BaseGetReloadInfoFast(DNNInfo);
    BaseAllocateNaiveFast(DNNInfo);
    clock_t timestamp_2 = clock();
//    std::cout << double(timestamp_2 - timestamp_1) / CLOCKS_PER_SEC << "s" << std::endl;
}

int MemoryAllocation::GetInputChannelFromOutputIndexFast(Json::Value &DNNInfo, int node_index, int output_index, bool is_last)
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

static int base_visited_record[MAX_AG] = {0};
static float base_memory_usage_input[MAX_AG] = {0};
static float base_memory_usage_output[MAX_AG] = {0};
static float base_memory_usage_node[MAX_NODE] = {0};
static float base_memory_usage_recv[MAX_CORE] = {0};
void MemoryAllocation::BaseMemoryUsageInfoFast(Json::Value &DNNInfo)
{
    std::cout << "============ core memory statistic ============" << std::endl;
    float core_memory_sum = 0.0;
    // 只遍历一个instruction_group，只有output需要乘上instruction_group_num。其他的都可以复用
    for (int i = 0; i < core_num; ++i)
    {
        float memory_usage = 0;
//        Json::Value InstructionIRList = BackBone[0].core_list[i].instruction_ir_list;
        std::vector<struct INST> InstructionIRList = PIMCOM_6_base_instruction_ir[0].core_list[i].instruction_ir_list;
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
                    int instruction_group_num;
                    if (appointed_instruction_group_num == 0)
                    {
                        instruction_group_num = ceil(float(instruction_group_num_ori) / float(instruction_group_reload_num));
                    }
                    else
                    {
                        instruction_group_num = appointed_instruction_group_num < instruction_group_num_ori ? appointed_instruction_group_num : instruction_group_num_ori;
                    }
                    if (base_visited_record[AG_index_in_total] == 0)
                    {
                        base_visited_record[AG_index_in_total] = 1;
                        int kernel_h = PIMCOM_node_list[node_index].param.kernel_h;
                        int stride_h = PIMCOM_node_list[node_index].param.stride_h;
                        int input_width = PIMCOM_node_list[node_index].input_dim[3];
                        int input_channel_length = PIMCOM_node_list[node_index].input_dim[1];
                        int input_num = (instruction_group_num * operation_cycle_before_comm);
                        int row_num = kernel_h + stride_h* ((floor(float(input_num)/float(input_width)) + ((input_num % input_width) >= 2 ? 2 : 1))-1);
                        for (int k = 0; k < PIMCOM_6_recv_info.node_list[node_index].size(); ++k)
                        {
                            if (PIMCOM_6_recv_info.node_list[node_index][k].AG_index == AG_index_in_total)
                            {
                                base_memory_usage_input[AG_index_in_total] += row_num * input_width * input_channel_length;
                                memory_usage += row_num * input_width * input_channel_length;
                                break;
                            }
                        }

//                        for (int k = 0; k < PIMCOM_6_recv_info.node_list[node_index].size(); ++k)
//                        {
//                            if (PIMCOM_6_recv_info.node_list[node_index][k].AG_index == AG_index_in_total)
//                            {
//                                base_memory_usage_input[AG_index_in_total] += input_num * kernel_h * kernel_h * input_channel;
//                                memory_usage += input_num * kernel_h * kernel_h * input_channel;
//                                break;
//                            }
//                        }
                        base_memory_usage_input[AG_index_in_total] += Instruction.input_element_num;
                        memory_usage += Instruction.input_element_num;
                        base_memory_usage_output[AG_index_in_total] += operation_cycle_before_comm * instruction_group_num * Instruction.output_element_num;
                        memory_usage += operation_cycle_before_comm * instruction_group_num * Instruction.output_element_num;
                    }
                }
                else
                {
                    int AG_index_in_total = Instruction.AG_index_in_total;
                    base_memory_usage_input[AG_index_in_total] += Instruction.input_element_num;
                    base_memory_usage_output[AG_index_in_total] += Instruction.output_element_num;
                    memory_usage += Instruction.input_element_num;
                    memory_usage += Instruction.output_element_num;
                }
            }
            else if (Instruction.operation == "VADD")
            {

            }
            else if (Instruction.operation == "RECV")
            {
                int destination_AG_index = Instruction.destination;
                int node_index = PIMCOM_3_hierarchy_map.whole[destination_AG_index][0].node_index;
                int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
                int replication_index = PIMCOM_3_hierarchy_map.whole[destination_AG_index][0].replication_index;
                int instruction_group_num_ori = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].instruction_group_num;
                int instruction_group_num;
                if (appointed_instruction_group_num == 0)
                    instruction_group_num = ceil(float(instruction_group_num_ori) / float(instruction_group_reload_num));
                else
                    instruction_group_num = appointed_instruction_group_num < instruction_group_num_ori ? appointed_instruction_group_num : instruction_group_num_ori;
                if (PIMCOM_node_list[node_index].operation == "OP_CONV")
                {
                    memory_usage += instruction_group_num * Instruction.element_num;
                    base_memory_usage_recv[i] += instruction_group_num * Instruction.element_num;
                }
                else if (PIMCOM_node_list[node_index].operation == "OP_FC")
                {
                    memory_usage += Instruction.element_num;
                    base_memory_usage_recv[i] += Instruction.element_num;
                }
            }
        }
        std::cout << "core" << i << "  " << memory_usage * 16 / 8 / 1024 << "KB" << std::endl;
        core_memory_sum += memory_usage;
    }
    std::cout << "sum:" << core_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;


    int AG_num = PIMCOM_3_hierarchy_map.whole_index.size();
    float ag_memory_sum = 0.0;
    std::cout << "============ AG memory statistic ============" << std::endl;
    for (int j = 0; j < AG_num; ++j)
    {
//        std::cout << "AG" << j
//                  << "  input:" << std::left << std::setw(8) << base_memory_usage_input[j] * 16 / 8 / 1024 << "KB"
//                  << "  output:" << std::left  << std::setw(8) << base_memory_usage_output[j] * 16 / 8 / 1024 << "KB" << std::endl;
        ag_memory_sum += base_memory_usage_input[j] + base_memory_usage_output[j];
    }
    std::cout << "sum:" << ag_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;


    int effective_node_num = PIMCOM_2_effective_node.size();
    std::cout << "============ node memory statistic ============" << std::endl;
    float node_memory_sum = 0.0;
    for (int i = 0; i < effective_node_num; ++i)
    {
        //TODO !!!!!!!!! 这样必然很低效 !!!!!!!!! 没有必要这样复制。次数也不多
//        Json::Value Node = PIMCOM_2_AG_partition[i];
//        struct PIMCOM_2_AG_partition Node = PIMCOM_2_AG_partition[i];
        int node_index = PIMCOM_2_AG_partition[i].index;
        int replication_num = PIMCOM_2_AG_partition[i].replication_num;
        for (int j = 0; j < replication_num; ++j)
        {
            int AG_num_in_replication = PIMCOM_2_AG_partition[i].replication[j].AG_list.size();
            for (int k = 0; k < AG_num_in_replication; ++k)
            {
                int AG_index_in_total = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].AG_index;
                base_memory_usage_node[node_index] += base_memory_usage_input[AG_index_in_total] + base_memory_usage_output[AG_index_in_total];
            }
        }
        node_memory_sum += base_memory_usage_node[node_index];
        std::cout << "node" << node_index << "  "  << base_memory_usage_node[node_index] * 16 / 8 / 1024 << "KB" << std::endl;
    }
    std::cout << "sum:" << node_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;


    std::cout << "============ core memory recv statistic ============" << std::endl;
    float core_recv_sum = 0.0;
    for (int i = 0; i < core_num; ++i)
    {
        std::cout << " core" << i <<  "  " << base_memory_usage_recv[i] * 16 / 8 / 1024 << "KB" << std::endl;
        core_recv_sum += base_memory_usage_recv[i];
    }
    std::cout << "sum:" << core_recv_sum * 16 / 8 / 1024 << "KB" << std::endl;
}

static int core_AG_num[MAX_CORE] = {0};
void MemoryAllocation::BaseGetReloadInfoFast(Json::Value &DNNInfo)
{
    int effective_node_num = PIMCOM_2_effective_node.size();
    for (int i = 0; i < effective_node_num; ++i)
    {
        int effective_node_index = PIMCOM_2_effective_node[i];
        if (PIMCOM_node_list[effective_node_index].operation == "OP_CONV")
        {
            int reload_AG_num = PIMCOM_6_recv_info.node_list[effective_node_index].size();
            for (int j = 0; j < reload_AG_num; ++j)
            {
                int AG_index = PIMCOM_6_recv_info.node_list[effective_node_index][j].AG_index;
                int core_index = PIMCOM_6_recv_info.node_list[effective_node_index][j].core_index;
                int replication_index = PIMCOM_6_recv_info.node_list[effective_node_index][j].replication_index;
                int AG_index_in_replication = PIMCOM_6_recv_info.node_list[effective_node_index][j].AG_index_in_replication;
                int input_cycle_this_replication_start = PIMCOM_2_AG_partition[i].replication[replication_index].input_cycle_this_start;
                int input_cycle_this_replication_end = PIMCOM_2_AG_partition[i].replication[replication_index].input_cycle_this_end;
                int reload_num = ceil(float(PIMCOM_2_AG_partition[i].replication[replication_index].instruction_group_num) / float(appointed_instruction_group_num));

                std::vector<struct reload_start_and_end> Reload;
                Reload.resize(reload_num);
                int input_index = input_cycle_this_replication_start;
                bool k_break = false;
                for (int k = 0; k < reload_num; ++k)
                {
                    for (int l = 0; l < appointed_instruction_group_num * operation_cycle_before_comm; ++l)
                    {
                        if (l == 0)
                            Reload[k].start = input_index;
                        if (l == appointed_instruction_group_num * operation_cycle_before_comm - 1)
                            Reload[k].end = input_index;
                        input_index++;
                        if (input_index > input_cycle_this_replication_end)
                        {
                            Reload[k].end = input_index - 1;
                            k_break = true;
                            break;
                        }
                    }
                    if (k_break)
                    {
                        int AG_index_in_core = core_AG_num[core_index];
                        core_AG_num[core_index]++;
                        PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].reload = Reload;
                        PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].node_index = effective_node_index;
                        PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].replication_index = replication_index;
                        PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].core_index = core_index;
                        PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].AG_index = AG_index;
                        PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].AG_index_in_replication = AG_index_in_replication;
                        break;
                    }
                }
            }
        }
        else if (PIMCOM_node_list[effective_node_index].operation == "OP_FC")
        {
            int reload_AG_num = PIMCOM_6_recv_info.node_list[effective_node_index].size();
            for (int j = 0; j < reload_AG_num; ++j)
            {
                int AG_index = PIMCOM_6_recv_info.node_list[effective_node_index][j].AG_index;
                int core_index = PIMCOM_6_recv_info.node_list[effective_node_index][j].core_index;
                int node_index = PIMCOM_6_recv_info.node_list[effective_node_index][j].node_index;
                int recv_element = PIMCOM_6_recv_info.node_list[effective_node_index][j].recv_element;
                int start_offset_element = PIMCOM_6_recv_info.node_list[effective_node_index][j].start_offset_element;
                int AG_index_in_core = core_AG_num[core_index];
                core_AG_num[core_index]++;
                PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].AG_index = AG_index;
                PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].core_index = core_index;
                PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].node_index = node_index;
                PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].recv_element = recv_element;
                PIMCOM_7_reload_info.core_list[core_index].AG_list[AG_index_in_core].start_offset_element = start_offset_element;
            }
        }
    }
}

void MemoryAllocation::BaseAllocateNaiveFast(Json::Value &DNNInfo)
{
    int instruction_group_num = PIMCOM_6_base_instruction_ir.size();
    PIMCOM_7_base_instruction_with_reload.resize(instruction_group_num);
    for (int i = 0; i < instruction_group_num; ++i)
    {
        int reload_index = i / appointed_instruction_group_num;
        if (i % appointed_instruction_group_num == 0)
        {
            for (int j = 0; j < core_num; ++j)
            {
                // LOAD
                int reload_AG_num = PIMCOM_7_reload_info.core_list[j].AG_list.size();
                for (int k = 0; k < reload_AG_num; ++k)
                {
                    int node_index = PIMCOM_7_reload_info.core_list[j].AG_list[k].node_index;
                    if (PIMCOM_node_list[node_index].operation == "OP_CONV")
                    {
                        int this_ag_max_reload_num = PIMCOM_7_reload_info.core_list[j].AG_list[k].reload.size();
                        if (reload_index >= this_ag_max_reload_num)
                            continue;
                        int AG_index = PIMCOM_7_reload_info.core_list[j].AG_list[k].AG_index;
                        // input_cycle is output_channel_index
                        int input_cycle_start = PIMCOM_7_reload_info.core_list[j].AG_list[k].reload[reload_index].start;
                        int input_cycle_end = PIMCOM_7_reload_info.core_list[j].AG_list[k].reload[reload_index].end;
                        int input_channel_start = GetInputChannelFromOutputIndexFast(DNNInfo, node_index, input_cycle_start, 0);
                        int input_channel_end = GetInputChannelFromOutputIndexFast(DNNInfo, node_index, input_cycle_end, 1);
                        int input_channel_length = PIMCOM_node_list[node_index].param.input_channel;

                        struct INST Instruction_ld;
                        Instruction_ld.type = MEM;
                        Instruction_ld.level_index = PIMCOM_node_list[node_index].level_index;
                        Instruction_ld.level_diff = 0;
                        Instruction_ld.operation = "LD";
                        Instruction_ld.stage = "RELOAD";
                        int provider_index = PIMCOM_node_list[node_index].provider_index[0];
                        if (PIMCOM_node_list[provider_index].operation == "OP_INPUT")
                            Instruction_ld.source = -1;
                        else
                            Instruction_ld.source = PIMCOM_node_list[provider_index].AG0_index_in_total;
                        Instruction_ld.destination = AG_index;
                        Instruction_ld.rs_offset = input_channel_length * input_channel_start;;
                        Instruction_ld.rd_offset = 0;
                        Instruction_ld.element_num = (input_channel_end-input_channel_start+1) * input_channel_length;
                        Instruction_ld.instruction_group_index = i;
                        PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_ld);
                    }
                    else if(PIMCOM_node_list[node_index].operation == "OP_FC")
                    {
                        if (reload_index >= 1)
                            continue;
                        int AG_index = PIMCOM_7_reload_info.core_list[j].AG_list[k].AG_index;
                        int recv_element = PIMCOM_7_reload_info.core_list[j].AG_list[k].recv_element;
                        int start_offset_element = PIMCOM_7_reload_info.core_list[j].AG_list[k].start_offset_element;

                        struct INST Instruction_ld;
                        Instruction_ld.type = MEM;
                        Instruction_ld.level_index = PIMCOM_node_list[node_index].level_index;
                        Instruction_ld.level_diff = 0;
                        Instruction_ld.operation = "LD";
                        Instruction_ld.stage = "RELOAD";
                        Instruction_ld.source = PIMCOM_node_list[node_index].AG0_index_in_total;
                        Instruction_ld.destination = AG_index;
                        Instruction_ld.rs_offset = start_offset_element;
                        Instruction_ld.rd_offset = 0;
                        Instruction_ld.element_num = recv_element;
                        Instruction_ld.instruction_group_index = i;
                        PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_ld);
                    }
                }
                std::vector<struct INST> InstructionIRList = PIMCOM_6_base_instruction_ir[i].core_list[j].instruction_ir_list;
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    struct INST tmpInstruction = InstructionIRList[k];
                    PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
                }
            }
        }
        else if (i % appointed_instruction_group_num == appointed_instruction_group_num-1 || i == instruction_group_num-1)
        {
            for (int j = 0; j < core_num; ++j)
            {
                std::vector<struct INST> InstructionIRList = PIMCOM_6_base_instruction_ir[i].core_list[j].instruction_ir_list;
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    struct INST tmpInstruction = InstructionIRList[k];
                    PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
                }

                // STORE
                int reload_AG_num = PIMCOM_7_reload_info.core_list[j].AG_list.size();
                for (int k = 0; k < reload_AG_num; ++k)
                {
                    int node_index = PIMCOM_7_reload_info.core_list[j].AG_list[k].node_index;
                    if (PIMCOM_node_list[node_index].operation == "OP_CONV")
                    {
                        int this_ag_max_reload_num = PIMCOM_7_reload_info.core_list[j].AG_list[k].reload.size();
                        if (reload_index >= this_ag_max_reload_num)
                            continue;
                        int AG_index = PIMCOM_7_reload_info.core_list[j].AG_list[k].AG_index;
                        int AG_index_in_replication = PIMCOM_7_reload_info.core_list[j].AG_list[k].AG_index_in_replication;
                        if (AG_index_in_replication != 0)
                            continue;
                        int output_channel_index_start = PIMCOM_7_reload_info.core_list[j].AG_list[k].reload[reload_index].start;
                        int output_channel_index_end = PIMCOM_7_reload_info.core_list[j].AG_list[k].reload[reload_index].end;
                        int output_channel_length = PIMCOM_node_list[node_index].param.output_channel;

                        struct INST Instruction_st;
                        Instruction_st.type = MEM;
                        Instruction_st.level_index = PIMCOM_node_list[node_index].level_index;
                        Instruction_st.level_diff = 0;
                        Instruction_st.operation = "ST";
                        Instruction_st.stage = "RELOAD";
                        Instruction_st.source = AG_index;
                        Instruction_st.destination = PIMCOM_node_list[node_index].AG0_index_in_total;
                        Instruction_st.rs_offset = 0;
                        Instruction_st.rd_offset = output_channel_index_start;
                        Instruction_st.element_num = (output_channel_index_end - output_channel_index_start + 1) * output_channel_length;
                        Instruction_st.instruction_group_index = i;
                        PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_st);
                    }
                    else if(PIMCOM_node_list[node_index].operation == "OP_FC")
                    {
                        if (reload_index >= 1)
                            continue;
                        int AG_index = PIMCOM_7_reload_info.core_list[j].AG_list[k].AG_index;
                        int recv_element = PIMCOM_7_reload_info.core_list[j].AG_list[k].recv_element;
                        int start_offset_element = PIMCOM_7_reload_info.core_list[j].AG_list[k].start_offset_element;

                        struct INST Instruction_st;
                        Instruction_st.type = MEM;
                        Instruction_st.level_index = PIMCOM_node_list[node_index].level_index;
                        Instruction_st.level_diff = 0;
                        Instruction_st.operation = "ST";
                        Instruction_st.stage = "RELOAD";
                        Instruction_st.source = AG_index;
                        Instruction_st.destination = PIMCOM_node_list[node_index].AG0_index_in_total;
                        Instruction_st.rs_offset = 0;
                        Instruction_st.rd_offset = start_offset_element;
                        Instruction_st.element_num = recv_element;
                        Instruction_st.instruction_group_index = i;
                        PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_st);
                    }
                }
            }
        }
        else
        {
            for (int j = 0; j < core_num; ++j)
            {
                std::vector<struct INST> InstructionIRList = PIMCOM_6_base_instruction_ir[i].core_list[j].instruction_ir_list;
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    struct INST tmpInstruction = InstructionIRList[k];
                    PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
                }
            }
        }
    }
}

//void MemoryAllocation::PostMemoryUsageInfo(Json::Value &DNNInfo)
//{
//    for (int i = 0; i < core_num; ++i)
//    {
//        float memory_usage = 0;
//        Json::Value InstructionIRList = PostPart[0].core_list[i].instruction_ir_list;
//        int instruction_num = InstructionIRList.size();
//        for (int j = 0; j < instruction_num; ++j) {
//            Json::Value Instruction = InstructionIRList[j];
//            if (Instruction.operation == "RECV")
//            {
//                int stage = Instruction.stage;
//                int element_num = Instruction.element_num;
//                if (stage == 7)
//                {
//                    memory_usage += element_num;
//                }
//            }
//        }
//        std::cout << "core:" << i << "  "<< memory_usage * 16 / 8 / 1024 << "KB" << std::endl;
//    }
//}

static int inference_start = 100;
static int inference_end = 100;
//void MemoryAllocation::ShowInstructionFast(Json::Value &DNNInfo)
//{
//    for (int inf = inference_start; inf <= inference_end ; ++inf)
//    {
//        std::cout << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;
//        int instruction_group_num = static_cast<int>(PIMCOM_7_base_instruction_with_reload.size());
//        for (int i = 0; i < instruction_group_num; ++i)
//        {
//            std::cout << std::endl;
//            std::cout << "=========================================================== base instruction_group " << i << " ===========================================================" << std::endl;
//            for (int j = 0; j < core_num; ++j)
//            {
//                std::cout << "core " << j << std::endl;
//                int instruction_num = PIMCOM_7_base_instruction_with_reload[i].core_list[j].size();
//                for (int k = 0; k < instruction_num; ++k)
//                {
//                    Json::Value Instruction = PIMCOM_7_base_instruction_with_reload[i].core_list[j][k];
//                    int instruction_level_index = Instruction.level_index;
//                    if (instruction_level_index > inf)
//                    {
//                        continue;
//                    }
//                    std::string Operation = Instruction.operation.asCString();
//                    ShowSingleInstructionSlow(Instruction, inf);
//                }
//            }
//        }
//    }
//}

void MemoryAllocation::SaveInstructionFast(Json::Value &DNNInfo)
{
    std::ofstream OutFile("../fast2.txt", std::ios::out | std::ios::trunc);
    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        OutFile << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;

        int instruction_group_num = PIMCOM_7_base_instruction_with_reload.size();
        for (int i = 0; i < instruction_group_num; ++i)
        {
            OutFile << "========================================= base instruction_group " << i << " =========================================" << std::endl;
            for (int j = 0; j < core_num; ++j)
            {
                OutFile << "core " << j << std::endl;
                int instruction_num = PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list.size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    struct INST Instruction = PIMCOM_7_base_instruction_with_reload[i].core_list[j].instruction_ir_list[k];
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


//void MemoryAllocation::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
//{
//    std::string strJson = PIMCOM_toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/7_ma.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}