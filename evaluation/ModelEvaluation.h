//
// Created by SXT on 2022/9/20.
//

#ifndef PIMCOM_MODELEVALUATION_H
#define PIMCOM_MODELEVALUATION_H

#include "common.h"
#include "../configure.h"
#include "thread"
#include "mutex"
#include "atomic"
#include "condition_variable"
#include "chrono"
#include "vector"

class ModelEvaluation
{
public:
    void EvaluateModel(Json::Value & DNNInfo);
private:
    int instruction_group_num;
    int core_num;
    Json::Value InstructionGroup;
    std::map<int, int> comm_index_2_index_in_core_map;
    std::map<int, int> comm_index_2_core_index;
    void EvaluateRecursion(Json::Value & DNNInfo, int core_index, int index_in_core);
    void ShowEvaluationResult();
};


#endif //PIMCOM_MODELEVALUATION_H
