//
// Created by SXT on 2022/10/17.
//

#include "common_function.h"

int GetInputChannelFromOutputIndex(int node_index, int output_index, bool is_last)
{
    struct PIMCOM_node Node = PIMCOM_node_list[node_index];
    struct param Params = Node.param;
    int input_H = Node.input_dim[2];
    int input_W = Node.input_dim[3];
    int conv_kernel_w = Params.kernel_w;
    int conv_kernel_h = Params.kernel_h;
    int conv_padding_h0 = Params.pad_h0;
    int conv_padding_h1 = Params.pad_h1;
    int conv_padding_w0 = Params.pad_w0;
    int conv_padding_w1 = Params.pad_w1;
    int conv_stride_w = Params.stride_w;
    int conv_stride_h = Params.stride_h;

    int output_W = floor(float(input_W + conv_padding_w0 + conv_padding_w1 - conv_kernel_w) / float(conv_stride_w)) + 1;
    int output_H = floor(float(input_H + conv_padding_h0 + conv_padding_h1 - conv_kernel_h) / float(conv_stride_h)) + 1;
    int info_output_W = Node.output_dim[3];
    int info_output_H = Node.output_dim[2];
    if (info_output_W != output_W || info_output_H != output_H)
    {
        std::cout << " Output Size Doesn't Match" << std::endl;
        return -1;
    }
    int normal_start_index_in_w = conv_padding_w0/conv_stride_w + (conv_padding_w0 % conv_stride_w == 0 ? 0 : 1);
    int normal_start_index_in_h = conv_padding_h0/conv_stride_h + (conv_padding_h0 % conv_stride_h == 0 ? 0 : 1);

    int i = output_index / output_W;
    int j = output_index % output_W;
    int start_address = i * conv_stride_h * input_W + j *  conv_stride_w;
    if (j < normal_start_index_in_w)
        start_address -= (j * conv_stride_w);
    else
        start_address -= conv_padding_w0;
    if (i < normal_start_index_in_h)
        start_address -= (i * conv_stride_h * input_W);
    else
        start_address -= conv_padding_h0 * input_W;

    int start_row = start_address / input_W;
    int start_col = start_address % input_W;

    int conv_w_num = conv_kernel_w;
    if (j < normal_start_index_in_w)
        conv_w_num = conv_w_num - conv_padding_w0 + j * conv_stride_w;
    if (start_col + conv_kernel_w > input_W)
        conv_w_num = conv_w_num - (start_col + conv_kernel_w - input_W);

    int conv_h_num = conv_kernel_h;
    if (i < normal_start_index_in_h)
        conv_h_num = conv_h_num - conv_padding_h0 + i * conv_stride_h;
    if (start_row + conv_kernel_h > input_H)
        conv_h_num = conv_h_num - (start_row + conv_kernel_h - input_H);

    int h = 0;
    int w = 0;
    if (is_last)
    {
        h = conv_h_num-1;
        w = conv_w_num-1;
    }
    int position = start_address + w + h * input_W; // input_index
    return position;
}



void GetMinMaxInputChannelFromInputCycle(int *min_max, int node_index, int input_cycle_first, int input_cycle_last)
{
    int min_input_channel = 1000000;
    int max_input_channel = 0;
    for (int i = input_cycle_first; i <= input_cycle_last; ++i)
    {
        int first_last[2];
        GetInputChannelFromOutputIndex(first_last, node_index, i);
        if (first_last[0] < min_input_channel)
            min_input_channel = first_last[0];
        if (first_last[1] > max_input_channel)
            max_input_channel = first_last[1];
    }
    min_max[0] = min_input_channel;
    min_max[1] = max_input_channel;
}

void GetInputChannelFromOutputIndex(int *first_last, int node_index, int output_index)
{
    struct PIMCOM_node Node = PIMCOM_node_list[node_index];
    struct param Params = Node.param;
    int input_H = Node.input_dim[2];
    int input_W = Node.input_dim[3];
    int conv_kernel_w = Params.kernel_w;
    int conv_kernel_h = Params.kernel_h;
    int conv_padding_h0 = Params.pad_h0;
    int conv_padding_h1 = Params.pad_h1;
    int conv_padding_w0 = Params.pad_w0;
    int conv_padding_w1 = Params.pad_w1;
    int conv_stride_w = Params.stride_w;
    int conv_stride_h = Params.stride_h;

    int output_W = floor(float(input_W + conv_padding_w0 + conv_padding_w1 - conv_kernel_w) / float(conv_stride_w)) + 1;
    int output_H = floor(float(input_H + conv_padding_h0 + conv_padding_h1 - conv_kernel_h) / float(conv_stride_h)) + 1;
    int info_output_W = Node.output_dim[3];
    int info_output_H = Node.output_dim[2];
    if (info_output_W != output_W || info_output_H != output_H)
    {
        std::cout << info_output_H << " " << output_W << std::endl;
        std::cout << " Output Size Doesn't Match" << std::endl;
        return ;
    }
    int normal_start_index_in_w = conv_padding_w0/conv_stride_w + (conv_padding_w0 % conv_stride_w == 0 ? 0 : 1);
    int normal_start_index_in_h = conv_padding_h0/conv_stride_h + (conv_padding_h0 % conv_stride_h == 0 ? 0 : 1);

    int i = output_index / output_W;
    int j = output_index % output_W;
    int start_address = i * conv_stride_h * input_W + j *  conv_stride_w;
    if (j < normal_start_index_in_w)
        start_address -= (j * conv_stride_w);
    else
        start_address -= conv_padding_w0;
    if (i < normal_start_index_in_h)
        start_address -= (i * conv_stride_h * input_W);
    else
        start_address -= conv_padding_h0 * input_W;

    int start_row = start_address / input_W;
    int start_col = start_address % input_W;

    int conv_w_num = conv_kernel_w;
    if (j < normal_start_index_in_w)
        conv_w_num = conv_w_num - conv_padding_w0 + j * conv_stride_w;
    if (start_col + conv_w_num > input_W)
        conv_w_num = conv_w_num - (start_col + conv_w_num - input_W);

    int conv_h_num = conv_kernel_h;
    if (i < normal_start_index_in_h)
        conv_h_num = conv_h_num - conv_padding_h0 + i * conv_stride_h;
    if (start_row + conv_h_num > input_H)
        conv_h_num = conv_h_num - (start_row + conv_h_num - input_H);

    first_last[0] = start_address;
    first_last[1] = start_address + (conv_w_num-1) + (conv_h_num-1) * input_W;
}