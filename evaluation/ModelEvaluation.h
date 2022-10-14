//
// Created by SXT on 2022/9/20.
//

#ifndef PIMCOM_MODELEVALUATION_H
#define PIMCOM_MODELEVALUATION_H

#include "../common.h"
#include "../configure.h"
#include "thread"
#include "mutex"
#include "atomic"
#include "condition_variable"
#include "chrono"
#include "vector"
#include "../backend/PIMCOMVariable.h"

class ModelEvaluation
{
public:
    ModelEvaluation(enum Mode RunMode);
    void EvaluationInstruction();
    void EvaluationMVMUL();
    void EvaluateCompute();
    void EvaluateMemory();
private:
    enum Mode EvalMode;
    int instruction_group_num;
    int core_num;
    void Clear();
    ////////////////////////////////////////////// Evaluation Compute //////////////////////////////////////////////
    std::map<int, int> comm_index_2_index_in_core_map;
    std::map<int, int> comm_index_2_core_index;
    void EvaluateRecursionWholeModel();
    void EvaluateRecursionSingleInstructionGroup(int instruction_group_index, int core_index, int index_in_core);
    void ShowEvaluationResultSingleInstructionGroup();
    void ShowEvaluationResultWholeModel();
    void ResetSingleInstructionGroup(bool clear_other_info);

    ////////////////////////////////////////////// Evaluation Memory //////////////////////////////////////////////
    void BaseMemoryUsage();
    void BaseMemoryUsageImproved();
    void ShowBaseMemoryUsage();
};


#endif //PIMCOM_MODELEVALUATION_H
