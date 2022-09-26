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
    Json::Value NodeList;
    void GetInstructionGroupNum(Json::Value & DNNInfo);
    void GetReloadInfo(Json::Value & DNNInfo);
    void MemoryUsageInfo(Json::Value & DNNInfo);
    void AllocateNaive(Json::Value & DNNInfo);
    int GetInputChannelFromOutputIndex(Json::Value &DNNInfo, int node_index, int output_index, bool is_last);
};


#endif //PIMCOM_MEMORYALLOCATION_H
