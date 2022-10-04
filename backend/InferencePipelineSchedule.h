//
// Created by SXT on 2022/9/16.
//

#ifndef PIMCOM_INFERENCEPIPELINESCHEDULE_H
#define PIMCOM_INFERENCEPIPELINESCHEDULE_H

#include "common.h"
#include "../configure.h"

class InferencePipelineSchedule
{
public:
    void ScheduleExecutionSlow(Json::Value & DNNInfo);
    void ScheduleExecutionFast(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
    void ScheduleShowInstructionSlow(Json::Value & DNNInfo);
    void ScheduleShowInstructionFast(Json::Value & DNNInfo);
    void ScheduleSaveInstructionSlow(Json::Value & DNNInfo);
    void ScheduleSaveInstructionFast(Json::Value & DNNInfo);
private:
    int core_num;
    int node_num;
    Json::Value CoreList;
    Json::Value NodeList;
    std::vector<int> post_start_address;
    std::vector<int> post_end_address;
    void ResetPostStartAndEndAddress(int origin_length, int assumed_core_num);
    int GetInputChannelFromOutputIndexSlow(Json::Value & DNNInfo, int node_index, int output_index, bool is_last);
    int GetInputChannelFromOutputIndexFast(Json::Value & DNNInfo, int node_index, int output_index, bool is_last);
    void SchedulePreparationFast(Json::Value & DNNInfo);
    void SchedulePreparationSlow(Json::Value & DNNInfo);
    void ScheduleNaiveFast(Json::Value & DNNInfo);
    void ScheduleNaiveSlow(Json::Value & DNNInfo);
    void ScheduleNaiveStage1Fast(Json::Value & DNNInfo, int instruction_group_index, bool append_instruction);
    void ScheduleNaiveStage1Slow(Json::Value & DNNInfo, int instruction_group_index, bool append_instruction);
    void ScheduleNaiveStage2Fast(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage2Slow(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage3Fast(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage3Slow(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage4Fast(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage4Slow(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStageActFast(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStageActSlow(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage5Fast(Json::Value & DNNInfo, int node_index, int level_index, int instruction_group_index);
    void ScheduleNaiveStage5Slow(Json::Value & DNNInfo, int node_index, int level_index, int instruction_group_index);
    void ScheduleNaiveStage6Fast(Json::Value & DNNInfo, int node_index, int level_index, int mode, int instruction_group_index);
    void ScheduleNaiveStage6Slow(Json::Value & DNNInfo, int node_index, int level_index, int mode, int instruction_group_index);
    void ScheduleNaivePickOnePostOperationFast(Json::Value & DNNInfo);
    void ScheduleNaivePickOnePostOperationSlow(Json::Value & DNNInfo);
    void ScheduleNaiveScheduleOnePostOperationFast(Json::Value & DNNInfo, int instruction_group_index, int post_node_index);
    void ScheduleNaiveScheduleOnePostOperationSlow(Json::Value & DNNInfo, int instruction_group_index, int post_node_index);
    void AddSeparateLineFast(Json::Value & DNNInfo, int instruction_group_index);
    void AddSeparateLineSlow(Json::Value & DNNInfo, int instruction_group_index);
    void FillTheWholeInstructionGroupFast(Json::Value & DNNInfo);
    void FillTheWholeInstructionGroupSlow(Json::Value & DNNInfo);
    int GetEffectiveInstructionGroupNumFast(Json::Value & DNNInfo);
    int GetEffectiveInstructionGroupNumSlow(Json::Value & DNNInfo);
};

#endif //PIMCOM_INFERENCEPIPELINESCHEDULE_H
