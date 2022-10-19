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
    ElementPipelineSchedule(std::string model_name);
    void ScheduleExecution();
    void SaveInstruction();
private:
    std::string model_name;
    int last_node_index;
    int last_node_output_channel_num;
    int core_num;
    int node_num;
    int effective_instruction_group_num;
    void SchedulePreparation();
    void SavePreparation();
    void Check();
    void Clear();
    void GetEarlyTermination();
    ////////////////////////// DataFlow (CLUSTER) //////////////////////////
    void ScheduleNaiveGatherDataFlow();
    void GetRelatedAGIndex();
    bool NewCheckNormalPrepared(int AG_index_in_total);
    bool NewCheckPoolRepPrepared(int node_index, int replication_index, int pool_key_input_channel_index_current);
    bool NewCheckVecPrepared(int node_index, int replication_index, int pool_key_input_channel_index_current);
    void DivideTheOutputChannel(std::vector<int> &start_address, std::vector<int> &end_address, int output_channel_num_total, int replication_num);
    void ScheduleNaiveGatherDataFlowMain(std::ofstream & OutFile, int instruction_group_index);
    void ScheduleNaiveGatherDataFlowPost(std::ofstream & OutFile, int instruction_group_index, int this_node_index, int next_node_index, int input_channel_index, int complete_replication_index, bool effective_post);
    ////////////////////////// Instruction //////////////////////////
    void ScheduleNaiveGatherInstruction();
    void ScheduleNaiveGatherInstructionMain(int instruction_group_index);
    void ScheduleNaiveGatherInstructionPost(int instruction_group_index, int this_node_index, int next_node_index, int input_channel_index, int complete_replication_index, bool append_instruction);
    void ScheduleNaiveGatherInstructionStage1MVMUL(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication, int input_cycle_index);
    void ScheduleNaiveGatherInstructionStage2VADD(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication);
    void ScheduleNaiveGatherInstructionStage3ACC(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication);
    void ScheduleNaiveGatherInstructionStage3ACT(int instruction_group_index, int start_AG_index_in_total);
    void ScheduleNaiveGatherInstructionStage4Pool(int instruction_group_index, int pool_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveGatherInstructionStage4Eltwise(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveGatherInstructionStage4Activate(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveGatherInstructionStage4Concat(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveGatherInstructionCOMM(int instruction_group_index, int send_AG_index, int recv_AG_index);

    ////////////////////////// DataFlow (SPLIT) //////////////////////////
    void ScheduleNaiveSplitDataFlow();
    void ScheduleNaiveSplitDataFlowMain(std::ofstream & OutFile, int instruction_group_index);
    void ScheduleNaiveSplitDataFlowPost(std::ofstream & OutFile, int instruction_group_index, int this_node_index, int next_node_index, int input_channel_index, int complete_replication_index, bool effective_post);
};


#endif //PIMCOM_ELEMENTPIPELINESCHEDULE_H
