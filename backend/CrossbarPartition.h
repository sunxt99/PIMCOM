//
// Created by SXT on 2022/8/19.
//

#ifndef PIMCOM_CROSSBARPARTITION_H
#define PIMCOM_CROSSBARPARTITION_H

#include "../common.h"
#include "../configure.h"
#include "PIMCOMVariable.h"

class CrossbarPartition {
public:
    CrossbarPartition(enum Mode RunMode);
    void PartitionCrossbar();
//    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    enum Mode PartMode;
    void Clear();
    int Check();
    void PartitionNaive();
    void PartitionExploration();
    void DivideInputCycle(std::vector<int> &start_address_vector, std::vector<int> &end_address_vector, int input_cycle_num_total, int replication_num);
};


#endif //PIMCOM_CROSSBARPARTITION_H
