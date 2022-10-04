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
    void MapHierarchy(Json::Value & DNNInfo);
    void ShowOriginalInfo(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    int ResourceList[ChipW*ChipH]{};
    void MapNaiveFast(Json::Value &DNNInfo);
    void MapNaiveSlow(Json::Value &DNNInfo);
    void Check(Json::Value &DNNInfo);
};


#endif //PIMCOM_HIERARCHYMAPPING_H
