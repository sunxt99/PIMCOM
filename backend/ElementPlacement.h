//
// Created by SXT on 2022/8/21.
//

#ifndef PIMCOM_ELEMENTPLACEMENT_H
#define PIMCOM_ELEMENTPLACEMENT_H

#include "common.h"
#include "../configure.h"

class ElementPlacement
{
public:
    void PlaceElement(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    void PlaceCoreNaiveFast(Json::Value & DNNInfo);
    void PlaceCrossbarNaiveFast(Json::Value & DNNInfo);
    void PlaceCoreNaiveSlow(Json::Value & DNNInfo);
    void PlaceCrossbarNaiveSlow(Json::Value & DNNInfo);
};




#endif //PIMCOM_ELEMENTPLACEMENT_H
