//
// Created by SXT on 2022/8/19.
//

#ifndef PIMCOM_HIERARCHYMAPPING_H
#define PIMCOM_HIERARCHYMAPPING_H

#include "common.h"
#include "../configure.h"

class HierarchyMapping
{
public:
    HierarchyMapping();
    void MapHierarchy();
    void ShowOriginalInfo();
//    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    int ResourceList[ChipW*ChipH]{};
    void MapNaive();
    void Check();
};


#endif //PIMCOM_HIERARCHYMAPPING_H
