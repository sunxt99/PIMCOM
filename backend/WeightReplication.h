//
// Created by SXT on 2022/8/19.
//

#ifndef PIMCOM_WEIGHTREPLICATION_H
#define PIMCOM_WEIGHTREPLICATION_H

#include "common.h"
#include <random>
#include "../configure.h"

class WeightReplication
{
public:
    void ReplicateWeight(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    void ReplicateRandomly(Json::Value & DNNInfo);
};


#endif //PIMCOM_WEIGHTREPLICATION_H
