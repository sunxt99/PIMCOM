//
// Created by SXT on 2022/10/17.
//

#ifndef PIMCOM_COMMON_FUNCTION_H
#define PIMCOM_COMMON_FUNCTION_H

#include "../common.h"
#include "../configure.h"
#include "PIMCOMVariable.h"

int GetInputChannelFromOutputIndex( int node_index, int output_index, bool is_last);

void GetInputChannelFromOutputIndex(int first_last[2], int node_index, int output_index);
void GetMinMaxInputChannelFromInputCycle(int min_max[2], int node_index, int input_cycle_first, int input_cycle_last);
#endif //PIMCOM_COMMON_FUNCTION_H
