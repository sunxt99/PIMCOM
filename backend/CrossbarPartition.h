//
// Created by SXT on 2022/8/19.
//

#ifndef PIMCOM_CROSSBARPARTITION_H
#define PIMCOM_CROSSBARPARTITION_H

#include "common.h"
#include "../configure.h"

class CrossbarPartition {
public:
    void PartitionCrossbar(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    int Check();
    void PartitionNaiveSlow(Json::Value & DNNInfo);
    void PartitionNaiveFast(Json::Value & DNNInfo);
};


#endif //PIMCOM_CROSSBARPARTITION_H
