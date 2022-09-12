//
// Created by SXT on 2022/8/16.
//

#ifndef CIMFUSION_STRUCTURE_H
#define CIMFUSION_STRUCTURE_H

#endif //CIMFUSION_STRUCTURE_H


typedef struct conv_param
{
    int kernel_h;
    int kernel_w;
    int stride_h;
    int stride_w;
    int pad_h0;
    int pad_h1;
    int pad_w0;
    int pad_w1;
    int dilation_h;
    int dilation_w;
    int input_channel;
    int output_channel;
    int group;
    int activation;
    int wino_off;
} ConvParam;

typedef struct pool_param
{
    int pool_method; // 0:max    1:avg
    int kernel_h;
    int kernel_w;
    int stride_h;
    int stride_w;
    int pad_h0;
    int pad_h1;
    int pad_w0;
    int pad_w1;
    int global; // 0:general    1:global
    int caffe_flavor;
    void* funct;

    /* to support dynamic shape, need to save the original pad values*/
    int pad_h0_org;
    int pad_h1_org;
    int pad_w0_org;
    int pad_w1_org;
    void* input_pad;
} PoolParam;


typedef struct flatten_param
{
    int axis;
    int end_axis;
} FlattenParam;

typedef struct fc_param
{
    int num_output;
} FcParam;

typedef struct relu_param
{
    float negative_slope;
} ReLUParam;

typedef struct concat_param
{
    int axis;
} ConcatParam;

typedef struct softmax_param
{
    int axis;
} SoftmaxParam;


typedef struct reshape_param
{
    /*
    int dim_0;
    int dim_1;
    int dim_2;
    int dim_3;
    int dim_size;
    int axis;
    */
    int* re_shape;
    int reverse;
    int is_mxnet;
    int is_onnx;
    int dim_size;
} ReshapeParam;

enum EltType
{
    ELT_PROD,
    ELT_PROD_SCALAR,
    ELT_SUM,
    ELT_SUM_SCALAR,
    ELT_SUB,
    ELT_SUB_SCALAR,
    ELT_MAX,
    ELT_RSQRT,
    ELT_MIN_SCALAR,
    ELT_LAST,
    ELT_DIV,
    ELT_LOG,
    ELT_EXP,
    ELT_SQRT,
    ELT_FLOOR,
    ELT_SQUARE,
    ELT_POW,
    ELT_POWER,
};

typedef struct eltwise_param
{
    int type;
    int caffe_flavor;
    float shift;
    float power;
    float scale;
} ElewiseParam;