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
    void AllocateMemory(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
    void ShowInstruction(Json::Value &DNNInfo);
private:
    int core_num;
    Json::Value BackBone;
    Json::Value PostPart;
    Json::Value NodeList;
    void BaseGetReloadInfo(Json::Value & DNNInfo);
    void BaseMemoryUsageInfo(Json::Value & DNNInfo);
    void BaseAllocateNaive(Json::Value & DNNInfo);
    void PostMemoryUsageInfo(Json::Value & DNNInfo);
    int GetInputChannelFromOutputIndex(Json::Value &DNNInfo, int node_index, int output_index, bool is_last);
};


#endif //PIMCOM_MEMORYALLOCATION_H
