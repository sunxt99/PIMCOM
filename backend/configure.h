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
const int ChipW = 4;  // #Core every row in Chip
const int ChipH = 4;  // #Core every column in Chip
enum PipelineType {Inference, Row, Element};

const int MAX_AG = 10000;
const int MAX_CORE = 5000;
const int MAX_NODE = 5000;

#endif //PIMCOM_CONFIGURE_H
