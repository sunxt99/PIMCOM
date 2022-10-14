//
// Created by SXT on 2022/9/1.
//

#include "DetailAppend.h"

static int EndInputChannelIndex[MAX_AG] = {0}; // 最终该AG结束在的input_channel_index
static int EndIndexInInputChannel[MAX_AG] = {0};
static int OffsetForFC[MAX_AG] = {0};
static int SourceIndex[MAX_AG] = {0};


void DetailAppend::AppendDetail()
{
    instruction_group_num = PIMCOM_5_base_instruction_with_reload.size();
    core_num = PIMCOM_4_virtual_core_AG_map.core_list.size();
    PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group.resize(instruction_group_num);
    PreProcess();
    PrepareForInput();
    Clear();
}

void DetailAppend::PreProcess()
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

void DetailAppend::PrepareForInput()
{
    for (int i = 0; i < instruction_group_num; ++i)
    {
        struct PIMCOM_4_instruction_ir InstructionGroup = PIMCOM_5_base_instruction_with_reload[i];
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
                    PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
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

                        PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(Instruction);
                        if (IsBreak)
                        {
                            break;
                        }
                    }
                    PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);
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
                    PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(Instruction);
                    PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.push_back(tmpInstruction);

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

void DetailAppend::Clear()
{
    for (int i = 0; i < MAX_AG; ++i) EndInputChannelIndex[i] = 0;
    for (int i = 0; i < MAX_AG; ++i) EndIndexInInputChannel[i] = 0;
    for (int i = 0; i < MAX_AG; ++i) OffsetForFC[i] = 0;
    for (int i = 0; i < MAX_AG; ++i) SourceIndex[i] = 0;
}


void DetailAppend::SaveInstruction()
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
                int instruction_num = PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list.size();
                for (int k = 0; k < instruction_num; ++k)
                {
                    struct INST Instruction = PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group[i].core_list[j].instruction_ir_list[k];
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


//void DetailAppend::SaveJsonIR(, std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/8_da.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}