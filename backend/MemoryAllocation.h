//
// Created by SXT on 2022/9/25.
//

#ifndef PIMCOM_MEMORYALLOCATION_H
#define PIMCOM_MEMORYALLOCATION_H

#include "common.h"
#include "../configure.h"

class MemoryAllocation
{
public:
    void AllocateMemorySlow(Json::Value & DNNInfo);
    void AllocateMemoryFast(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
    void ShowInstructionSlow(Json::Value &DNNInfo);
    void ShowInstructionFast(Json::Value &DNNInfo);
    void SaveInstructionSlow(Json::Value &DNNInfo);
    void SaveInstructionFast(Json::Value &DNNInfo);
private:
    int core_num;
    Json::Value BackBone;
    Json::Value PostPart;
    Json::Value NodeList;
    void PostMemoryUsageInfo(Json::Value & DNNInfo);
    void BaseGetReloadInfoSlow(Json::Value & DNNInfo);
    void BaseGetReloadInfoFast(Json::Value & DNNInfo);
    void BaseMemoryUsageInfoSlow(Json::Value & DNNInfo);
    void BaseMemoryUsageInfoFast(Json::Value & DNNInfo);
    void BaseAllocateNaiveSlow(Json::Value & DNNInfo);
    void BaseAllocateNaiveFast(Json::Value & DNNInfo);
    int GetInputChannelFromOutputIndexSlow(Json::Value &DNNInfo, int node_index, int output_index, bool is_last);
    int GetInputChannelFromOutputIndexFast(Json::Value &DNNInfo, int node_index, int output_index, bool is_last);

    void BaseGetReloadInfoFast(INST &DNNInfo);
};


#endif //PIMCOM_MEMORYALLOCATION_H
