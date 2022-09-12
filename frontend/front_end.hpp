//
// Created by SXT on 2022/8/15.
//
#include <iostream>
#include <cstring>
#include <fstream>
#include <string>
#include "stdio.h"
#include "json/json.h"


extern "C" {
#include "api/c_api.h"
#include "graph/graph.h"
#include "graph/subgraph.h"
#include "graph/node.h"
#include "graph/tensor.h"
#include "executer/executer.h"
#include "module/module.h"
#include "utility/log.h"
#include "utility/sys_port.h"
#include "utility/vector.h"
}

std::string OP_TYPE[] =
        {
    "OP_GENERIC ",
    "OP_ABSVAL",
    "OP_ADD_N",
    "OP_ARGMAX",
    "OP_ARGMIN",
    "OP_BATCHNORM",
    "OP_BATCHTOSPACEND",
    "OP_BIAS",
    "OP_BROADMUL",
    "OP_CAST",
    "OP_CEIL",
    "OP_CLIP",
    "OP_COMPARISON",
    "OP_CONCAT",
    "OP_CONST",
    "OP_CONV",
    "OP_CROP",
    "OP_DECONV",
    "OP_DEPTHTOSPACE",
    "OP_DETECTION_OUTPUT",
    "OP_DETECTION_POSTPROCESS",
    "OP_DROPOUT",
    "OP_ELTWISE",
    "OP_ELU",
    "OP_EMBEDDING",
    "OP_EXPANDDIMS",
    "OP_FC",
    "OP_FLATTEN",
    "OP_GATHER",
    "OP_GEMM",
    "OP_GRU",
    "OP_HARDSIGMOID",
    "OP_HARDSWISH",
    "OP_INPUT",
    "OP_INSTANCENORM",
    "OP_INTERP",
    "OP_LOGICAL",
    "OP_LOGISTIC",
    "OP_LRN",
    "OP_LSTM",
    "OP_MATMUL",
    "OP_MAXIMUM",
    "OP_MEAN",
    "OP_MINIMUM",
    "OP_MVN",
    "OP_NOOP",
    "OP_NORMALIZE",
    "OP_PAD",
    "OP_PERMUTE",
    "OP_POOL",
    "OP_PRELU",
    "OP_PRIORBOX",
    "OP_PSROIPOOLING",
    "OP_REDUCEL2",
    "OP_REDUCTION",
    "OP_REGION",
    "OP_RELU",
    "OP_RELU6",
    "OP_REORG",
    "OP_RESHAPE",
    "OP_RESIZE",
    "OP_REVERSE",
    "OP_RNN",
    "OP_ROIALIGN",
    "OP_ROIPOOLING",
    "OP_ROUND",
    "OP_RPN",
    "OP_SCALE",
    "OP_SELU",
    "OP_SHUFFLECHANNEL",
    "OP_SIGMOID",
    "OP_SLICE",
    "OP_SOFTMAX",
    "OP_SPACETOBATCHND",
    "OP_SPACETODEPTH",
    "OP_SPARSETODENSE",
    "OP_SPLIT",
    "OP_SQUAREDDIFFERENCE",
    "OP_SQUEEZE",
    "OP_STRIDED_SLICE",
    "OP_SWAP_AXIS",
    "OP_TANH",
    "OP_THRESHOLD",
    "OP_TOPKV2",
    "OP_TRANSPOSE",
    "OP_UNARY",
    "OP_UNSQUEEZE",
    "OP_UPSAMPLE",
    "OP_ZEROSLIKE",
    "OP_MISH",
    "OP_LOGSOFTMAX",
    "OP_RELU1",
    "OP_L2NORMALIZATION",
    "OP_L2POOL",
    "OP_TILE",
    "OP_SHAPE",
    "OP_SCATTER",
    "OP_WHERE",
    "OP_SOFTPLUS",
    "OP_RECIPROCAL",
    "OP_SPATIALTRANSFORMER",
    "OP_EXPAND",
    "OP_GELU",
    "OP_BUILTIN_LAST"
        };

