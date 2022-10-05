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
    void AllocateMemory();
    void ShowInstruction();
    void SaveInstruction();
    void SaveJsonIR(Json::Value &DNNInfo, std::string ModelName);

private:
    int core_num;
    void PostMemoryUsageInfo();
    void BaseGetReloadInfo();
    void BaseMemoryUsageInfo();
    void BaseAllocateNaive();
    int GetInputChannelFromOutputIndex(int node_index, int output_index, bool is_last);
};


#endif //PIMCOM_MEMORYALLOCATION_H
