//
// Created by SXT on 2022/9/16.
//

#ifndef PIMCOM_INFERENCEPIPELINESCHEDULE_H
#define PIMCOM_INFERENCEPIPELINESCHEDULE_H

#include "../common.h"
#include "../configure.h"
#include "PIMCOMVariable.h"

class InferencePipelineSchedule
{
public:
    InferencePipelineSchedule(enum Mode RunMode);
    void ScheduleExecution();
    void ShowInstruction();
    void SaveInstruction();
//    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);

private:
    enum Mode ScheduleMode;
    int core_num;
    int node_num;
    int AG_num_total;
    std::vector<int> post_start_address;
    std::vector<int> post_end_address;
    void ResetPostStartAndEndAddress(int origin_length, int assumed_core_num);
    int GetInputChannelFromOutputIndex( int node_index, int output_index, bool is_last);
    void SchedulePreparation();
    void ScheduleNaive();
    void ScheduleNaiveStage1(int instruction_group_index, bool append_instruction);
    void ScheduleNaiveStage2(int instruction_group_index, bool append_instruction);
    void ScheduleNaiveStage3(int instruction_group_index, bool append_instruction);
    void ScheduleNaiveStage4(int instruction_group_index);
    void ScheduleNaiveStage4WithoutWB(int instruction_group_index);
    void ScheduleNaiveStageAct(int instruction_group_index, bool append_instruction);
    void ScheduleNaiveStage5(int node_index, int level_index, int instruction_group_index);
    void ScheduleNaiveStage6(int node_index, int level_index, int mode, int instruction_group_index);
    void ScheduleNaivePickOnePostOperation();
    void ScheduleNaiveScheduleOnePostOperation(int instruction_group_index, int post_node_index);
    void AddSeparateLine(int instruction_group_index);
    void FillTheWholeInstructionGroup();
    int GetEffectiveInstructionGroupNum();
    void Clear();

    // For Fast Evaluation
    void ScheduleNaiveForEvaluation();
    void ScheduleNaiveStage1ForEvaluation(int instruction_group_index, int operation_cycle_index, bool append_instruction);

};

#endif //PIMCOM_INFERENCEPIPELINESCHEDULE_H
