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
    void ScheduleExecution(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
    void ScheduleShowInstruction(Json::Value & DNNInfo);
private:
    int core_num;
    int node_num;
    Json::Value CoreList;
    Json::Value NodeList;
    std::vector<int> post_start_address;
    std::vector<int> post_end_address;
    void ResetPostStartAndEndAddress(int origin_length, int assumed_core_num);
    int GetInputChannelFromOutputIndex(Json::Value & DNNInfo, int node_index, int output_index, bool is_last);
    void SchedulePreparation(Json::Value & DNNInfo);
    void ScheduleNaive(Json::Value & DNNInfo);
    void ScheduleNaiveStage1(Json::Value & DNNInfo, int instruction_group_index, bool append_instruction);
    void ScheduleNaiveStage2(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage3(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage4(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStageAct(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage5(Json::Value & DNNInfo, int node_index, int level_index, int instruction_group_index);
    void ScheduleNaiveStage6(Json::Value & DNNInfo, int node_index, int level_index, int mode, int instruction_group_index);
    void ScheduleNaivePickOnePostOperation(Json::Value & DNNInfo);
    void ScheduleNaiveScheduleOnePostOperation(Json::Value & DNNInfo, int instruction_group_index, int post_node_index);
    void AddSeparateLine(Json::Value & DNNInfo, int instruction_group_index);
    void FillTheWholeInstructionGroup(Json::Value & DNNInfo);
    int GetEffectiveInstructionGroupNum(Json::Value & DNNInfo);
};


#endif //PIMCOM_INFERENCEPIPELINESCHEDULE_H
