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
    void PlaceCoreNaive(Json::Value & DNNInfo);
    void PlaceCrossbarNaive(Json::Value & DNNInfo);
};




#endif //PIMCOM_ELEMENTPLACEMENT_H
