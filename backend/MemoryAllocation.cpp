//
// Created by SXT on 2022/9/25.
//

#include "MemoryAllocation.h"

void MemoryAllocation::AllocateMemory(Json::Value &DNNInfo)
{
    core_num = static_cast<int>(DNNInfo["3_virtual_core_crossbar_map"].size());
    BackBone = DNNInfo["6_base_instruction_ir"];
    PostPart = DNNInfo["6_post_instruction_ir"];
    NodeList = DNNInfo["5_node_list_augmented"];
    BaseMemoryUsageInfo(DNNInfo);
//    BaseGetReloadInfo(DNNInfo);
//    BaseAllocateNaive(DNNInfo);
//    PostMemoryUsageInfo(DNNInfo);
}

int MemoryAllocation::GetInputChannelFromOutputIndex(Json::Value &DNNInfo, int node_index, int output_index, bool is_last)
{
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

int base_visited_record[MAX_AG] = {0};
float base_memory_usage_input[MAX_AG] = {0};
float base_memory_usage_output[MAX_AG] = {0};
float base_memory_usage_node[MAX_NODE] = {0};
float base_memory_usage_recv[MAX_CORE] = {0};
void MemoryAllocation::BaseMemoryUsageInfo(Json::Value &DNNInfo)
{
    std::cout << "============ core memory statistic ============" << std::endl;
    float core_memory_sum = 0.0;
    // 只遍历一个instruction_group，只有output需要乘上instruction_group_num。其他的都可以复用
    for (int i = 0; i < core_num; ++i)
    {
        float memory_usage = 0;
        Json::Value InstructionIRList = BackBone[0]["core_list"][i]["instruction_ir_list"];
        int instruction_num = InstructionIRList.size();
        for (int j = 0; j < instruction_num; ++j)
        {
            Json::Value Instruction = InstructionIRList[j];
            if (strcmp(Instruction["operation"].asCString(), "MVMUL") == 0)
            {
                if (strcmp(Instruction["conv_or_fc"].asCString(), "OP_CONV") == 0)
                {
                    int node_index = Instruction["node_index"].asInt();
                    int AG_index_in_total = Instruction["AG_index_in_total"].asInt();
                    int replication_index = Instruction["replication_index"].asInt();
                    int effective_node_index = NodeList[node_index]["effective_node_index"].asInt();
                    int instruction_group_num_ori = DNNInfo["2_AG_partition"][effective_node_index]["replication"][replication_index]["instruction_group_num"].asInt();
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
                        int kernel_h = NodeList[node_index]["param"]["kernel_h"].asInt();
                        int stride_h = NodeList[node_index]["param"]["stride_h"].asInt();
                        int input_width = NodeList[node_index]["input_dim"][3].asInt();
                        int input_channel = NodeList[node_index]["input_dim"][1].asInt();
                        int input_num = (instruction_group_num * operation_cycle_before_comm);
                        int row_num = kernel_h + stride_h* ((floor(float(input_num)/float(input_width)) + ((input_num % input_width) >= 2 ? 2 : 1))-1);
                        for (int k = 0; k < DNNInfo["6_recv_info"]["node_list"][node_index].size(); ++k)
                        {
                            if (DNNInfo["6_recv_info"]["node_list"][node_index][k]["AG_index"].asInt() == AG_index_in_total)
                            {
                                base_memory_usage_input[AG_index_in_total] += row_num * input_width * input_channel;
                                memory_usage += row_num * input_width * input_channel;
                                break;
                            }
                        }

//                        for (int k = 0; k < DNNInfo["6_recv_info"]["node_list"][node_index].size(); ++k)
//                        {
//                            if (DNNInfo["6_recv_info"]["node_list"][node_index][k]["AG_index"].asInt() == AG_index_in_total)
//                            {
//                                base_memory_usage_input[AG_index_in_total] += input_num * kernel_h * kernel_h * input_channel;
//                                memory_usage += input_num * kernel_h * kernel_h * input_channel;
//                                break;
//                            }
//                        }
                        base_memory_usage_input[AG_index_in_total] += Instruction["input_element_num"].asInt();
                        memory_usage += Instruction["input_element_num"].asInt();
                        base_memory_usage_output[AG_index_in_total] += operation_cycle_before_comm * instruction_group_num * Instruction["output_element_num"].asInt();
                        memory_usage += operation_cycle_before_comm * instruction_group_num * Instruction["output_element_num"].asInt();
                    }
                }
                else
                {
                    int AG_index_in_total = Instruction["AG_index_in_total"].asInt();
                    base_memory_usage_input[AG_index_in_total] += Instruction["input_element_num"].asInt();
                    base_memory_usage_output[AG_index_in_total] += Instruction["output_element_num"].asInt();
                    memory_usage += Instruction["input_element_num"].asInt();
                    memory_usage += Instruction["output_element_num"].asInt();
                }
            }
            else if (strcmp(Instruction["operation"].asCString(), "VADD") == 0)
            {

            }
            else if (strcmp(Instruction["operation"].asCString(), "RECV") == 0)
            {
                int destination_AG_index = Instruction["destination"].asInt();
                int node_index = DNNInfo["3_hierarchy_map"]["whole"][destination_AG_index][0]["node_index"].asInt();
                int effective_node_index = NodeList[node_index]["effective_node_index"].asInt();
                int replication_index = NodeList[node_index]["replication_index"].asInt();
                int instruction_group_num_ori = DNNInfo["2_AG_partition"][effective_node_index]["replication"][replication_index]["instruction_group_num"].asInt();
                int instruction_group_num;
                if (appointed_instruction_group_num == 0)
                    instruction_group_num = ceil(float(instruction_group_num_ori) / float(instruction_group_reload_num));
                else
                    instruction_group_num = appointed_instruction_group_num < instruction_group_num_ori ? appointed_instruction_group_num : instruction_group_num_ori;
                if (strcmp(NodeList[node_index]["operation"].asCString(), "OP_CONV") == 0)
                {
                    memory_usage += instruction_group_num * Instruction["element_num"].asInt();
                    base_memory_usage_recv[i] += instruction_group_num * Instruction["element_num"].asInt();
                }
                else if (strcmp(NodeList[node_index]["operation"].asCString(), "OP_FC") == 0)
                {
                    memory_usage += Instruction["element_num"].asInt();
                    base_memory_usage_recv[i] += Instruction["element_num"].asInt();
                }
            }
        }
        std::cout << "core" << i << "  " << memory_usage * 16 / 8 / 1024 << "KB" << std::endl;
        core_memory_sum += memory_usage;
    }
    std::cout << "sum:" << core_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;


    int AG_num = DNNInfo["3_hierarchy_map"]["whole_index"].size();
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


    int effective_node_num = DNNInfo["2_effective_node"].size();
    std::cout << "============ node memory statistic ============" << std::endl;
    float node_memory_sum = 0.0;
    for (int i = 0; i < effective_node_num; ++i)
    {
        Json::Value Node = DNNInfo["2_AG_partition"][i];
        int node_index = Node["index"].asInt();
        int replication_num = Node["replication_num"].asInt();
        for (int j = 0; j < replication_num; ++j)
        {
            int AG_num_in_replication = Node["replication"][j]["AG_list"].size();
            for (int k = 0; k < AG_num_in_replication; ++k)
            {
                int AG_index_in_total = Node["replication"][j]["AG_list"][k]["AG_index"].asInt();
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
void MemoryAllocation::BaseGetReloadInfo(Json::Value &DNNInfo)
{
    int effective_node_num = DNNInfo["2_effective_node"].size();
    for (int i = 0; i < effective_node_num; ++i)
    {
        int effective_node_index = DNNInfo["2_effective_node"][i].asInt();
        if (strcmp(NodeList[effective_node_index]["operation"].asCString(), "OP_CONV") == 0)
        {
            int reload_AG_num = DNNInfo["6_recv_info"]["node_list"][effective_node_index].size();
            for (int j = 0; j < reload_AG_num; ++j)
            {
                int AG_index = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["AG_index"].asInt();
                int core_index = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["core_index"].asInt();
                int replication_index = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["replication_index"].asInt();
                int AG_index_in_replication = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["AG_index_in_replication"].asInt();
                int input_cycle_this_replication_start = DNNInfo["2_AG_partition"][i]["replication"][replication_index]["input_cycle_this_start"].asInt();
                int input_cycle_this_replication_end = DNNInfo["2_AG_partition"][i]["replication"][replication_index]["input_cycle_this_end"].asInt();
                int reload_num = ceil(DNNInfo["2_AG_partition"][i]["replication"][replication_index]["instruction_group_num"].asFloat() / float(appointed_instruction_group_num));
                Json::Value Reload;
                Reload["reload"].resize(reload_num);
                int input_index = input_cycle_this_replication_start;
                bool k_break = false;
                for (int k = 0; k < reload_num; ++k)
                {
                    for (int l = 0; l < appointed_instruction_group_num * operation_cycle_before_comm; ++l)
                    {
                        if (l == 0)
                            Reload["reload"][k]["start"] = input_index;
                        if (l == appointed_instruction_group_num * operation_cycle_before_comm - 1)
                            Reload["reload"][k]["end"] = input_index;
                        input_index++;
                        if (input_index > input_cycle_this_replication_end)
                        {
                            Reload["reload"][k]["end"] = input_index - 1;
                            k_break = true;
                            break;
                        }
                    }
                    if (k_break)
                    {
                        int AG_index_in_core = core_AG_num[core_index];
                        core_AG_num[core_index]++;
                        DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core] = Reload;
                        DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["node_index"] = effective_node_index;
                        DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["replication_index"] = replication_index;
                        DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["core_index"] = core_index;
                        DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["AG_index"] = AG_index;
                        DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["AG_index_in_replication"] = AG_index_in_replication;
                        break;
                    }
                }
            }
        }
        else if (strcmp(NodeList[effective_node_index]["operation"].asCString(), "OP_FC") == 0)
        {
            int reload_AG_num = DNNInfo["6_recv_info"]["node_list"][effective_node_index].size();
            for (int j = 0; j < reload_AG_num; ++j)
            {
                int AG_index = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["AG_index"].asInt();
                int core_index = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["core_index"].asInt();
                int node_index = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["node_index"].asInt();
                int recv_element = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["recv_element"].asInt();
                int start_offset_element = DNNInfo["6_recv_info"]["node_list"][effective_node_index][j]["start_offset_element"].asInt();
                int AG_index_in_core = core_AG_num[core_index];
                core_AG_num[core_index]++;
                DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["AG_index"] = AG_index;
                DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["core_index"] = core_index;
                DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["node_index"] = node_index;
                DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["recv_element"] = recv_element;
                DNNInfo["8_reload_info"]["core_list"][core_index]["AG_list"][AG_index_in_core]["start_offset_element"] = start_offset_element;
            }
        }
    }
}

void MemoryAllocation::BaseAllocateNaive(Json::Value &DNNInfo)
{
    int instruction_group_num = BackBone.size();
    DNNInfo["8_base_instruction_with_reload"].resize(instruction_group_num);
    for (int i = 0; i < instruction_group_num; ++i)
    {
        int reload_index = i / appointed_instruction_group_num;
        if (i % appointed_instruction_group_num == 0)
        {
            for (int j = 0; j < core_num; ++j)
            {
                // LOAD
                int reload_AG_num = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"].size();
                for (int k = 0; k < reload_AG_num; ++k)
                {
                    int node_index = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["node_index"].asInt();
                    if (strcmp(NodeList[node_index]["operation"].asCString(), "OP_CONV") == 0)
                    {
                        int this_ag_max_reload_num = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["reload"].size();
                        if (reload_index >= this_ag_max_reload_num)
                            continue;
                        int AG_index = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["AG_index"].asInt();
                        // input_cycle is output_channel_index
                        int input_cycle_start = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["reload"][reload_index]["start"].asInt();
                        int input_cycle_end = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["reload"][reload_index]["end"].asInt();
                        int input_channel_start = GetInputChannelFromOutputIndex(DNNInfo, node_index, input_cycle_start, 0);
                        int input_channel_end = GetInputChannelFromOutputIndex(DNNInfo, node_index, input_cycle_end, 1);
                        int input_channel_length = NodeList[node_index]["param"]["input_channel"].asInt();

                        Json::Value Instruction_ld;
                        Instruction_ld["level_index"] = NodeList[node_index]["level_index"];
                        Instruction_ld["level_diff"] = 0;
                        Instruction_ld["operation"] = "RELOAD-LD";
//                        Instruction_ld["source"] = NodeList[node_index]["AG0_index_in_total"];
                        int provider_index = NodeList[node_index]["provider_index"][0].asInt();
                        if (strcmp(NodeList[provider_index]["operation"].asCString(), "OP_INPUT") == 0)
                            Instruction_ld["source"] = -1;
                        else
                            Instruction_ld["source"] = NodeList[provider_index]["AG0_index_in_total"];
                        Instruction_ld["destination"] = AG_index;
                        Json::Value offset_ld;
                        offset_ld["rs_offset"] = input_channel_length * input_channel_start;;
                        offset_ld["rd_offset"] = 0;
                        Instruction_ld["offset"] = offset_ld;
                        Instruction_ld["element_num"] = (input_channel_end-input_channel_start+1) * input_channel_length;
                        Instruction_ld["instruction_group_index"] = i;
                        DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j].append(Instruction_ld);
                    }
                    else if(strcmp(NodeList[node_index]["operation"].asCString(), "OP_FC") == 0)
                    {
                        if (reload_index >= 1)
                            continue;
                        int AG_index = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["AG_index"].asInt();
                        int recv_element = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["recv_element"].asInt();
                        int start_offset_element = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["start_offset_element"].asInt();

                        Json::Value Instruction_ld;
                        Instruction_ld["level_index"] = NodeList[node_index]["level_index"];
                        Instruction_ld["level_diff"] = 0;
                        Instruction_ld["operation"] = "RELOAD-LD";
                        Instruction_ld["source"] = NodeList[node_index]["AG0_index_in_total"];
                        Instruction_ld["destination"] = AG_index;
                        Json::Value offset_ld;
                        offset_ld["rs_offset"] = start_offset_element;
                        offset_ld["rd_offset"] = 0;
                        Instruction_ld["offset"] = offset_ld;
                        Instruction_ld["element_num"] = recv_element;
                        Instruction_ld["instruction_group_index"] = i;
                        DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j].append(Instruction_ld);
                    }
                }
                Json::Value InstructionIRList = BackBone[i]["core_list"][j]["instruction_ir_list"];
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    Json::Value tmpInstruction = InstructionIRList[k];
                    DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j].append(tmpInstruction);
                }
            }
        }
        else if (i % appointed_instruction_group_num == appointed_instruction_group_num-1 || i == instruction_group_num-1)
        {
            for (int j = 0; j < core_num; ++j)
            {
                Json::Value InstructionIRList = BackBone[i]["core_list"][j]["instruction_ir_list"];
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    Json::Value tmpInstruction = InstructionIRList[k];
                    DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j].append(tmpInstruction);
                }

                // STORE
                int reload_AG_num = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"].size();
                for (int k = 0; k < reload_AG_num; ++k)
                {
                    int node_index = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["node_index"].asInt();
                    if (strcmp(NodeList[node_index]["operation"].asCString(), "OP_CONV") == 0)
                    {
                        int this_ag_max_reload_num = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["reload"].size();
                        if (reload_index >= this_ag_max_reload_num)
                            continue;
                        int AG_index = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["AG_index"].asInt();
                        int AG_index_in_replication = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["AG_index_in_replication"].asInt();
                        if (AG_index_in_replication != 0)
                            continue;
                        int output_channel_index_start = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["reload"][reload_index]["start"].asInt();
                        int output_channel_index_end = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["reload"][reload_index]["end"].asInt();
                        int output_channel_length = NodeList[node_index]["param"]["output_channel"].asInt();

                        Json::Value Instruction_st;
                        Instruction_st["level_index"] = NodeList[node_index]["level_index"];
                        Instruction_st["level_diff"] = 0;
                        Instruction_st["operation"] = "RELOAD-ST";
                        Instruction_st["source"] = AG_index;
                        Instruction_st["destination"] = NodeList[node_index]["AG0_index_in_total"];
                        Json::Value offset_ld;
                        offset_ld["rs_offset"] = 0;
                        offset_ld["rd_offset"] = output_channel_index_start;
                        Instruction_st["offset"] = offset_ld;
                        Instruction_st["element_num"] = (output_channel_index_end - output_channel_index_start + 1) * output_channel_length;
                        Instruction_st["instruction_group_index"] = i;
                        DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j].append(Instruction_st);
                    }
                    else if(strcmp(NodeList[node_index]["operation"].asCString(), "OP_FC") == 0)
                    {
                        if (reload_index >= 1)
                            continue;
                        int AG_index = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["AG_index"].asInt();
                        int recv_element = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["recv_element"].asInt();
                        int start_offset_element = DNNInfo["8_reload_info"]["core_list"][j]["AG_list"][k]["start_offset_element"].asInt();

                        Json::Value Instruction_st;
                        Instruction_st["level_index"] = NodeList[node_index]["level_index"];
                        Instruction_st["level_diff"] = 0;
                        Instruction_st["operation"] = "RELOAD-ST";
                        Instruction_st["source"] = AG_index;
                        Instruction_st["destination"] = NodeList[node_index]["AG0_index_in_total"];
                        Json::Value offset_ld;
                        offset_ld["rs_offset"] = 0;
                        offset_ld["rd_offset"] = start_offset_element;
                        Instruction_st["offset"] = offset_ld;
                        Instruction_st["element_num"] = recv_element;
                        Instruction_st["instruction_group_index"] = i;
                        DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j].append(Instruction_st);
                    }
                }
            }
        }
        else
        {
            for (int j = 0; j < core_num; ++j)
            {
                Json::Value InstructionIRList = BackBone[i]["core_list"][j]["instruction_ir_list"];
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    Json::Value tmpInstruction = InstructionIRList[k];
                    DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j].append(tmpInstruction);
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
//        Json::Value InstructionIRList = PostPart[0]["core_list"][i]["instruction_ir_list"];
//        int instruction_num = InstructionIRList.size();
//        for (int j = 0; j < instruction_num; ++j) {
//            Json::Value Instruction = InstructionIRList[j];
//            if (strcmp(Instruction["operation"].asCString(), "RECV") == 0)
//            {
//                int stage = Instruction["stage"].asInt();
//                int element_num = Instruction["element_num"].asInt();
//                if (stage == 7)
//                {
//                    memory_usage += element_num;
//                }
//            }
//        }
//        std::cout << "core:" << i << "  "<< memory_usage * 16 / 8 / 1024 << "KB" << std::endl;
//    }
//}










int inference_start = 100;
int inference_end = 100;
void MemoryAllocation::ShowInstruction(Json::Value &DNNInfo)
{
    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        std::cout << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;
        int instruction_group_num = static_cast<int>(DNNInfo["8_base_instruction_with_reload"].size());
        for (int i = 0; i < instruction_group_num; ++i)
        {
            std::cout << std::endl;
            std::cout << "=========================================================== base instruction_group " << i << " ===========================================================" << std::endl;
            for (int j = 0; j < core_num; ++j)
            {
                std::cout << "core " << j << std::endl;
                int instruction_num = DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j].size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    Json::Value Instruction = DNNInfo["8_base_instruction_with_reload"][i]["core_list"][j][k];
                    int instruction_level_index = Instruction["level_index"].asInt();
                    if (instruction_level_index > inf)
                    {
                        continue;
                    }
                    std::string Operation = Instruction["operation"].asCString();
                    ShowSingleInstruction(Instruction, inf);
                }
            }
        }
    }
}

void MemoryAllocation::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/8_ma.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}