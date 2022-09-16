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
    void SchedulePreparation(Json::Value & DNNInfo);
    void ScheduleNaive(Json::Value & DNNInfo);
    void ScheduleNaiveStage1(Json::Value & DNNInfo, int instruction_group_index, bool append_instruction);
    void ScheduleNaiveStage2(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage3(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage4(Json::Value & DNNInfo, int operation_cycle_before_comm);
    void ScheduleNaiveStageAct(Json::Value & DNNInfo, int instruction_group_index);
    void ScheduleNaiveStage5(Json::Value & DNNInfo, int operation_cycle_before_comm, int node_index, int level_index);
    void ScheduleNaiveStage6(Json::Value & DNNInfo, int operation_cycle_before_comm, int node_index, int level_index, int mode);
    void AddSeparateLine(Json::Value & DNNInfo, int instruction_group_index);
};


#endif //PIMCOM_INFERENCEPIPELINESCHEDULE_H
