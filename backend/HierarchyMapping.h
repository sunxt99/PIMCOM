//
// Created by SXT on 2022/8/19.
//

#ifndef PIMCOM_HIERARCHYMAPPING_H
#define PIMCOM_HIERARCHYMAPPING_H
#include "configure.h"
#include "common.h"

class HierarchyMapping
{
public:
    HierarchyMapping();
    void MapHierarchy(Json::Value & DNNInfo);
    void ShowOriginalInfo(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    int ResourceList[ChipW*ChipH]{};
    void MapNaive(Json::Value &DNNInfo);
    void Check(Json::Value &DNNInfo);
};


#endif //PIMCOM_HIERARCHYMAPPING_H
