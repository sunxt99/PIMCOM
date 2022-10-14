//
// Created by SXT on 2022/8/19.
//

#ifndef PIMCOM_HIERARCHYMAPPING_H
#define PIMCOM_HIERARCHYMAPPING_H

#include "../common.h"
#include "../configure.h"
#include "PIMCOMVariable.h"

class HierarchyMapping
{
public:
    HierarchyMapping(enum Mode RunMode);
    void MapHierarchy();
    void ShowOriginalInfo();
    void ShowMappingInfo();
    void ShowMappingInfoDistributed();
    void ShowMVMULInfo();
    void ShowMemoryInfo(); // Only For Evaluation Mode
    void SaveMemoryInfo();
//    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    enum Mode MapMode;
    int node_num;
    int core_num;
    int ResourceList[ChipW*ChipH]{0};
    void MapNaive();
    void MapDistributed();
    int MapDistributedTry();
    void AllocateMapInfo();
    void GatherForAccelerate();
    int GetInputElementNumFromAG(int node_index, int index_in_replication);
    void Check();
    void Clear();
};


#endif //PIMCOM_HIERARCHYMAPPING_H
