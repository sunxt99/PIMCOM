//
// Created by SXT on 2022/8/19.
//

#ifndef PIMCOM_CONFIGURE_H
#define PIMCOM_CONFIGURE_H

const int CellPrecision = 2;
const int CrossbarW = 256;
const int CrossbarH = 256;
const int CoreW = 1;  // #Crossbar every row in Core (Logical)
const int CoreH = 3;  // #Crossbar every column in Core (Logical)
const int ChipW = 8;  // #Core every row in Chip
const int ChipH = 8;  // #Core every column in Chip
enum PipelineType {Inference, Row, Element};

const int MAX_AG = 10000;
const int MAX_CORE = 5000;
const int MAX_NODE = 5000;

const int MVMUL_delay = 100;
const int VECTOR_delay = 50;
const int COMM_delay = 20;

const int operation_cycle_before_comm = 5;
const int instruction_group_reload_num = 500;
const int appointed_instruction_group_num = 30;

#endif //PIMCOM_CONFIGURE_H
