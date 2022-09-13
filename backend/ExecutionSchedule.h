//
// Created by SXT on 2022/8/24.
//

#ifndef PIMCOM_EXECUTIONSCHEDULE_H
#define PIMCOM_EXECUTIONSCHEDULE_H
#include "configure.h"
#include "common.h"

class ExecutionSchedule
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


#endif //PIMCOM_EXECUTIONSCHEDULE_H
