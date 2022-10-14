//
// Created by SXT on 2022/10/11.
//

#ifndef PIMCOM_ELEMENTPIPELINESCHEDULE_H
#define PIMCOM_ELEMENTPIPELINESCHEDULE_H
#include "../common.h"
#include "../configure.h"
#include "PIMCOMVariable.h"

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
    void ScheduleNaive();
    void ScheduleNaiveAbstract(std::ofstream & OutFile, int instruction_group_index);
    void ScheduleNaiveMain(int instruction_group_index);
    void ScheduleNaiveStage1(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication, int input_cycle_index);
    void ScheduleNaiveStage2(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication);
    void ScheduleNaiveStage3(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication);
    void ScheduleNaiveStageACT(int instruction_group_index, int start_AG_index_in_total);
    void ScheduleNaiveStage4Pool(int instruction_group_index, int pool_node_index, int execution_AG_index, int input_cycle_index);
    void ScheduleNaiveCOMM(int instruction_group_index, int send_AG_index, int recv_AG_index);
    void GetRelatedAGIndex();
    bool CheckNormalPrepared(int AG_index_in_total);
    bool NewCheckNormalPrepared(int AG_index_in_total);
    bool CheckPoolPrepared(int node_index, int pool_key_input_channel_index_current);
    bool CheckPoolRepPrepared(int node_index, int replication_index, int pool_key_input_channel_index_current);
    bool NewCheckPoolRepPrepared(int node_index, int replication_index, int pool_key_input_channel_index_current);
    int GetInputChannelFromOutputIndex(int node_index, int output_index, bool is_last);
    void DivideTheOutputChannel(std::vector<int> &start_address, std::vector<int> &end_address, int output_channel_num_total, int replication_num);
    void Clear();
};


#endif //PIMCOM_ELEMENTPIPELINESCHEDULE_H
