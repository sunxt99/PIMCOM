//
// Created by SXT on 2022/10/11.
//

#ifndef PIMCOM_ELEMENTPIPELINESCHEDULE_H
#define PIMCOM_ELEMENTPIPELINESCHEDULE_H
#include "../common.h"
#include "../configure.h"
#include "PIMCOMVariable.h"
#include "common_function.h"

class ElementPipelineSchedule
{
public:
    ElementPipelineSchedule();
    void ScheduleExecution();
    void SaveInstruction();
private:
    int core_num;
    int node_num;
    void SchedulePreparation();
    ////////////////////////// DataFlow //////////////////////////
    void ScheduleNaiveDataFlow();
    void GetRelatedAGIndex();
    void SavePreparation();
    bool NewCheckNormalPrepared(int AG_index_in_total);
    bool NewCheckPoolRepPrepared(int node_index, int replication_index, int pool_key_input_channel_index_current);
    bool NewCheckVecPrepared(int node_index, int replication_index, int pool_key_input_channel_index_current);
    void DivideTheOutputChannel(std::vector<int> &start_address, std::vector<int> &end_address, int output_channel_num_total, int replication_num);
    void ScheduleNaiveDataflowMain(std::ofstream & OutFile, int instruction_group_index);
    void ScheduleNaiveDataflowPost(std::ofstream & OutFile, int instruction_group_index, int this_node_index, int next_node_index, int input_channel_index, int complete_replication_index, bool effective_post);
    ////////////////////////// Instruction //////////////////////////
    void ScheduleNaiveInstruction();
    void ScheduleNaiveInstructionMain(int instruction_group_index);
    void ScheduleNaiveInstructionPost(int instruction_group_index, int this_node_index, int next_node_index, int input_channel_index, int complete_replication_index, bool append_instruction);
    void ScheduleNaiveInstructionStage1MVMUL(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication, int input_cycle_index);
    void ScheduleNaiveInstructionStage2VADD(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication);
    void ScheduleNaiveInstructionStage3ACC(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication);
    void ScheduleNaiveInstructionStage3ACT(int instruction_group_index, int start_AG_index_in_total);
    void ScheduleNaiveInstructionStage4Pool(int instruction_group_index, int pool_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveInstructionStage4Eltwise(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveInstructionStage4Activate(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveInstructionStage4Concat(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveInstructionCOMM(int instruction_group_index, int send_AG_index, int recv_AG_index);

    void Clear();
};


#endif //PIMCOM_ELEMENTPIPELINESCHEDULE_H
