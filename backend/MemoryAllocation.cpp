//
// Created by SXT on 2022/9/25.
//

#include "MemoryAllocation.h"

void MemoryAllocation::AllocateMemory()
{
    core_num = PIMCOM_3_virtual_core_crossbar_map.size();
    BaseGetReloadInfo();
    BaseAllocate();
    Clear();
}



void MemoryAllocation::GetMultiInputChannelFromMultiOutputIndex(std::set<int> & all_input_channel_index, int node_index, int output_index_begin,int output_index_end)
{
    //// 上面这个效率更高一些
    all_input_channel_index.clear();
    for (int i = output_index_begin; i <= output_index_end ; ++i)
    {
        for (auto iter = PIMCOM_conv_pool_input_output_info[node_index].output_index[i].begin(); iter != PIMCOM_conv_pool_input_output_info[node_index].output_index[i].end() ; ++iter)
        {
            all_input_channel_index.insert(*iter);
        }
    }

//    all_input_channel_index.clear();
//    int input_width = PIMCOM_node_list[node_index].input_dim[3];
//    for (int i = output_index_begin; i <= output_index_end ; ++i)
//    {
//        // 注意first和last之间并不是连续的
//        int input_channel_first = GetInputChannelFromOutputIndex(node_index, i, 0);
//        int input_channel_last = GetInputChannelFromOutputIndex(node_index, i, 1);
//        int real_window_width = (input_channel_last % input_width) - (input_channel_first % input_width) + 1;
//        int real_window_height = (input_channel_last / input_width) - (input_channel_first / input_width) + 1;
//        for (int j = 0; j < real_window_height; ++j)
//        {
//            for (int k = 0; k < real_window_width; ++k)
//            {
//                int input_channel_index = input_channel_first + k + j * input_width;
//                all_input_channel_index.insert(input_channel_index);
//            }
//        }
//    }
}


static int core_AG_num[MAX_CORE] = {0};
void MemoryAllocation::BaseGetReloadInfo()
{
    int effective_node_num = PIMCOM_2_effective_node.size();
    for (int i = 0; i < effective_node_num; ++i)
    {
        int effective_node_index = PIMCOM_2_effective_node[i];
        if (PIMCOM_node_list[effective_node_index].operation == "OP_CONV")
        {
            int reload_AG_num = PIMCOM_4_recv_info.node_list[effective_node_index].size();
            for (int j = 0; j < reload_AG_num; ++j)
            {
                int AG_index = PIMCOM_4_recv_info.node_list[effective_node_index][j].AG_index;
                int core_index = PIMCOM_4_recv_info.node_list[effective_node_index][j].core_index;
                int replication_index = PIMCOM_4_recv_info.node_list[effective_node_index][j].replication_index;
                int AG_index_in_replication = PIMCOM_4_recv_info.node_list[effective_node_index][j].AG_index_in_replication;
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
                        PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].reload = Reload;
                        PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].node_index = effective_node_index;
                        PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].replication_index = replication_index;
                        PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].core_index = core_index;
                        PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].AG_index = AG_index;
                        PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].AG_index_in_replication = AG_index_in_replication;
                        break;
                    }
                }
            }
        }
        else if (PIMCOM_node_list[effective_node_index].operation == "OP_FC")
        {
            int reload_AG_num = PIMCOM_4_recv_info.node_list[effective_node_index].size();
            for (int j = 0; j < reload_AG_num; ++j)
            {
                int AG_index = PIMCOM_4_recv_info.node_list[effective_node_index][j].AG_index;
                int core_index = PIMCOM_4_recv_info.node_list[effective_node_index][j].core_index;
                int node_index = PIMCOM_4_recv_info.node_list[effective_node_index][j].node_index;
                int recv_element = PIMCOM_4_recv_info.node_list[effective_node_index][j].recv_element;
                int start_offset_element = PIMCOM_4_recv_info.node_list[effective_node_index][j].start_offset_element;
                int AG_index_in_core = core_AG_num[core_index];
                core_AG_num[core_index]++;
                PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].AG_index = AG_index;
                PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].core_index = core_index;
                PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].node_index = node_index;
                PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].recv_element = recv_element;
                PIMCOM_5_reload_info.core_list[core_index].AG_list[AG_index_in_core].start_offset_element = start_offset_element;
            }
        }
    }
}


void MemoryAllocation::BaseAllocate()
{
    int instruction_group_num = PIMCOM_4_base_instruction_ir.size();
    PIMCOM_5_base_instruction_with_reload.resize(instruction_group_num);
    for (int i = 0; i < instruction_group_num; ++i)
    {
        int reload_index = i / appointed_instruction_group_num;
        if (i % appointed_instruction_group_num == 0)
        {
            for (int j = 0; j < core_num; ++j)
            {
                // LOAD
                int reload_AG_num = PIMCOM_5_reload_info.core_list[j].AG_list.size();
                for (int k = 0; k < reload_AG_num; ++k)
                {
                    int node_index = PIMCOM_5_reload_info.core_list[j].AG_list[k].node_index;
                    if (PIMCOM_node_list[node_index].operation == "OP_CONV")
                    {
                        int this_ag_max_reload_num = PIMCOM_5_reload_info.core_list[j].AG_list[k].reload.size();
                        if (reload_index >= this_ag_max_reload_num)
                            continue;
                        int AG_index = PIMCOM_5_reload_info.core_list[j].AG_list[k].AG_index;
                        // input_cycle is output_channel_index
                        int input_cycle_start = PIMCOM_5_reload_info.core_list[j].AG_list[k].reload[reload_index].start;
                        int input_cycle_end = PIMCOM_5_reload_info.core_list[j].AG_list[k].reload[reload_index].end;
                        int input_channel_length = PIMCOM_node_list[node_index].param.input_channel;
                        switch (ReloadMethod)
                        {
                            case ByRow:
                            {
                                int min_max_start[2];
                                GetMinMaxInputChannelFromInputCycle(min_max_start, node_index, input_cycle_start, input_cycle_end);
//                                int input_channel_start = GetInputChannelFromOutputIndex(node_index, input_cycle_start, 0);
//                                int input_channel_end = GetInputChannelFromOutputIndex(node_index, input_cycle_end, 1);
                                int input_channel_start = min_max_start[0];
                                int input_channel_end = min_max_start[1];
                                struct INST Instruction_ld;
                                Instruction_ld.type = MEM;
                                Instruction_ld.level_index = PIMCOM_node_list[node_index].level_index;
                                Instruction_ld.level_diff = 0;
                                Instruction_ld.operation = "LD";
                                Instruction_ld.node_index = node_index;
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
                                PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_ld);
                                break;
                            }
                            case ByWindow:
                            {
                                std::set<int> input_index_set;
                                GetMultiInputChannelFromMultiOutputIndex(input_index_set, node_index, input_cycle_start, input_cycle_end);
                                int rd_offset = 0;
                                for (auto iter = input_index_set.begin(); iter != input_index_set.end() ; ++iter)
                                {
                                    int input_channel_index = *iter;
                                    struct INST Instruction_ld;
                                    Instruction_ld.type = MEM;
                                    Instruction_ld.level_index = PIMCOM_node_list[node_index].level_index;
                                    Instruction_ld.level_diff = 0;
                                    Instruction_ld.operation = "LD";
                                    Instruction_ld.node_index = node_index;
                                    Instruction_ld.stage = "RELOAD";
                                    int provider_index = PIMCOM_node_list[node_index].provider_index[0];
                                    if (PIMCOM_node_list[provider_index].operation == "OP_INPUT")
                                        Instruction_ld.source = -1;
                                    else
                                        Instruction_ld.source = PIMCOM_node_list[provider_index].AG0_index_in_total;
                                    Instruction_ld.destination = AG_index;
                                    Instruction_ld.rs_offset = input_channel_length * input_channel_index;
                                    Instruction_ld.rd_offset = rd_offset;
                                    rd_offset += input_channel_length;
                                    Instruction_ld.element_num = 1 * input_channel_length;
                                    Instruction_ld.instruction_group_index = i;
                                    PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_ld);
                                }
                                break;
                            }
                        }
                    }
                    else if(PIMCOM_node_list[node_index].operation == "OP_FC")
                    {
                        if (reload_index >= 1)
                            continue;
                        int AG_index = PIMCOM_5_reload_info.core_list[j].AG_list[k].AG_index;
                        int recv_element = PIMCOM_5_reload_info.core_list[j].AG_list[k].recv_element;
                        int start_offset_element = PIMCOM_5_reload_info.core_list[j].AG_list[k].start_offset_element;

                        struct INST Instruction_ld;
                        Instruction_ld.type = MEM;
                        Instruction_ld.level_index = PIMCOM_node_list[node_index].level_index;
                        Instruction_ld.level_diff = 0;
                        Instruction_ld.operation = "LD";
                        Instruction_ld.node_index = node_index;
                        Instruction_ld.stage = "RELOAD";
                        Instruction_ld.source = PIMCOM_node_list[node_index].AG0_index_in_total;
                        Instruction_ld.destination = AG_index;
                        Instruction_ld.rs_offset = start_offset_element;
                        Instruction_ld.rd_offset = 0;
                        Instruction_ld.element_num = recv_element;
                        Instruction_ld.instruction_group_index = i;
                        PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_ld);
                    }
                }
                std::vector<struct INST> InstructionIRList = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list;
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    struct INST tmpInstruction = InstructionIRList[k];
                    PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
                }
            }
        }
        else if (i % appointed_instruction_group_num == appointed_instruction_group_num-1 || i == instruction_group_num-1)
        {
            for (int j = 0; j < core_num; ++j)
            {
                std::vector<struct INST> InstructionIRList = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list;
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    struct INST tmpInstruction = InstructionIRList[k];
                    PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
                }

                // STORE
                int reload_AG_num = PIMCOM_5_reload_info.core_list[j].AG_list.size();
                for (int k = 0; k < reload_AG_num; ++k)
                {
                    int node_index = PIMCOM_5_reload_info.core_list[j].AG_list[k].node_index;
                    if (PIMCOM_node_list[node_index].operation == "OP_CONV")
                    {
                        int this_ag_max_reload_num = PIMCOM_5_reload_info.core_list[j].AG_list[k].reload.size();
                        if (reload_index >= this_ag_max_reload_num)
                            continue;
                        int AG_index = PIMCOM_5_reload_info.core_list[j].AG_list[k].AG_index;
                        int AG_index_in_replication = PIMCOM_5_reload_info.core_list[j].AG_list[k].AG_index_in_replication;
                        if (AG_index_in_replication != 0)
                            continue;
                        int output_channel_index_start = PIMCOM_5_reload_info.core_list[j].AG_list[k].reload[reload_index].start;
                        int output_channel_index_end = PIMCOM_5_reload_info.core_list[j].AG_list[k].reload[reload_index].end;
                        int output_channel_length = PIMCOM_node_list[node_index].param.output_channel;

                        struct INST Instruction_st;
                        Instruction_st.type = MEM;
                        Instruction_st.level_index = PIMCOM_node_list[node_index].level_index;
                        Instruction_st.level_diff = 0;
                        Instruction_st.operation = "ST";
                        Instruction_st.stage = "RELOAD";
                        Instruction_st.node_index = node_index;
                        Instruction_st.source = AG_index;
                        Instruction_st.destination = PIMCOM_node_list[node_index].AG0_index_in_total;
                        Instruction_st.rs_offset = 0;
                        Instruction_st.rd_offset = output_channel_index_start;
                        Instruction_st.element_num = (output_channel_index_end - output_channel_index_start + 1) * output_channel_length;
                        Instruction_st.instruction_group_index = i;
                        PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_st);
                    }
                    else if(PIMCOM_node_list[node_index].operation == "OP_FC")
                    {
                        if (reload_index >= 1)
                            continue;
                        int AG_index = PIMCOM_5_reload_info.core_list[j].AG_list[k].AG_index;
                        int recv_element = PIMCOM_5_reload_info.core_list[j].AG_list[k].recv_element;
                        int start_offset_element = PIMCOM_5_reload_info.core_list[j].AG_list[k].start_offset_element;

                        struct INST Instruction_st;
                        Instruction_st.type = MEM;
                        Instruction_st.level_index = PIMCOM_node_list[node_index].level_index;
                        Instruction_st.level_diff = 0;
                        Instruction_st.operation = "ST";
                        Instruction_st.stage = "RELOAD";
                        Instruction_st.node_index = node_index;
                        Instruction_st.source = AG_index;
                        Instruction_st.destination = PIMCOM_node_list[node_index].AG0_index_in_total;
                        Instruction_st.rs_offset = 0;
                        Instruction_st.rd_offset = start_offset_element;
                        Instruction_st.element_num = recv_element;
                        Instruction_st.instruction_group_index = i;
                        PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(Instruction_st);
                    }
                }
            }
        }
        else
        {
            for (int j = 0; j < core_num; ++j)
            {
                std::vector<struct INST> InstructionIRList = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list;
                int instruction_ir_num = InstructionIRList.size();
                for (int k = 0; k < instruction_ir_num; ++k)
                {
                    struct INST tmpInstruction = InstructionIRList[k];
                    PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
                }
            }
        }
    }
}



//void MemoryAllocation::PostMemoryUsageInfo()
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


void MemoryAllocation::Clear()
{
    for (int i = 0; i < MAX_CORE; ++i) core_AG_num[i] = 0;
}


//void MemoryAllocation::ShowInstruction()
//{
//    for (int inf = inference_start; inf <= inference_end ; ++inf)
//    {
//        std::cout << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;
//        int instruction_group_num = static_cast<int>(PIMCOM_5_base_instruction_with_reload.size());
//        for (int i = 0; i < instruction_group_num; ++i)
//        {
//            std::cout << std::endl;
//            std::cout << "=========================================================== base instruction_group " << i << " ===========================================================" << std::endl;
//            for (int j = 0; j < core_num; ++j)
//            {
//                std::cout << "core " << j << std::endl;
//                int instruction_num = PIMCOM_5_base_instruction_with_reload[i].core_list[j].size();
//                for (int k = 0; k < instruction_num; ++k)
//                {
//                    Json::Value Instruction = PIMCOM_5_base_instruction_with_reload[i].core_list[j][k];
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

void MemoryAllocation::SaveInstruction()
{
    std::ofstream OutFile("../output/fast2.txt", std::ios::out | std::ios::trunc);
    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        OutFile << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;

        int instruction_group_num = PIMCOM_5_base_instruction_with_reload.size();
        for (int i = 0; i < instruction_group_num; ++i)
        {
            OutFile << "========================================= base instruction_group " << i << " =========================================" << std::endl;
            for (int j = 0; j < core_num; ++j)
            {
                OutFile << "core " << j << std::endl;
                int instruction_num = PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list.size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    struct INST Instruction = PIMCOM_5_base_instruction_with_reload[i].core_list[j].instruction_ir_list[k];
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


//void MemoryAllocation::SaveJsonIR(std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/7_ma.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}