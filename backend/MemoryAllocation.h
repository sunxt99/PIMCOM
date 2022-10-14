//
// Created by SXT on 2022/9/25.
//

#ifndef PIMCOM_MEMORYALLOCATION_H
#define PIMCOM_MEMORYALLOCATION_H

#include "../common.h"
#include "../configure.h"
#include "PIMCOMVariable.h"

class MemoryAllocation
{
public:
    void AllocateMemory();
    void SaveInstruction();
private:
    int core_num;
    void PostMemoryUsageInfo();
    void BaseGetReloadInfo();
    void Clear();
    void BaseAllocate();
    //// Reload By Row Helper Function 只能返回一个值，要么是input划窗中的第一个，要么是最后一个
    int GetInputChannelFromOutputIndex(int node_index, int output_index, bool is_last);
    //// Reload By Window Helper Function 把多个output的所有划窗包括的input channel index都返回
    void GetMultiInputChannelFromMultiOutputIndex(std::set<int> & all_input_channel_index, int node_index, int output_index_begin, int output_index_end);
};


#endif //PIMCOM_MEMORYALLOCATION_H
