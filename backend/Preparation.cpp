//
// Created by SXT on 2022/10/5.
//

#include "Preparation.h"

std::map<int, struct PIMCOM_node> PIMCOM_node_list_origin;
std::map<int, struct PIMCOM_node> PIMCOM_node_list;
// std::map<int, struct PIMCOM_conv_pool_input_output_info> PIMCOM_conv_pool_input_output_info;
std::vector<struct PIMCOM_conv_pool_input_output_info> PIMCOM_conv_pool_input_output_info;
std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
std::vector<int> PIMCOM_2_effective_node;
struct PIMCOM_3_hierarchy_map PIMCOM_3_hierarchy_map;
std::map<int, std::vector<int>> PIMCOM_3_virtual_core_crossbar_map;
std::map<int, int> PIMCOM_4_AG_instruction_group_num;
struct PIMCOM_4_first_AG_info PIMCOM_4_first_AG_info;
struct PIMCOM_4_virtual_core_AG_map PIMCOM_4_virtual_core_AG_map;
struct PIMCOM_4_recv_info PIMCOM_4_recv_info;
std::vector<struct PIMCOM_4_instruction_ir> PIMCOM_4_base_instruction_ir;
std::vector<std::vector<int>> PIMCOM_4_input_cycle_record;
std::map<int, struct PIMCOM_4_instruction_ir> PIMCOM_4_post_instruction_ir;
std::map<int, struct PIMCOM_4_instruction_ir> PIMCOM_4_post_multi_core_instruction_ir;
struct PIMCOM_5_reload_info PIMCOM_5_reload_info;
std::vector<struct PIMCOM_4_instruction_ir> PIMCOM_5_base_instruction_with_reload;
struct PIMCOM_6_detailed_instruction_ir PIMCOM_6_detailed_instruction_ir;

std::set<int> PIMCOM_4_unique_instruction_group_index;
std::vector<int> PIMCOM_4_evaluation_instruction_group_index;
std::map<int, int> PIMCOM_4_core_instruction_group_num;

std::vector<std::pair<struct MapSortStruct, int>> PIMCOM_3_compute_crossbar_ratio;
std::vector<std::vector<struct AGMapStruct>> PIMCOM_3_mapping_result;

void PreProcess(Json::Value & DNNInfo)
{
    Json::Value NodeList = DNNInfo["node_list"];
    int node_num = NodeList.size();
    //// Save the "Name-Index" key-value map
    std::map<std::string, int> name2index_map;
    for (int i = 0; i < node_num; ++i)
        name2index_map.insert(std::pair<std::string, int>(NodeList[i]["name"].asCString(),i));
    //// Reorder the Index
    for (int i = 0; i < node_num; ++i)
    {
        DNNInfo["node_list"][i]["index"] = i;
    }
    //// Get the Provider_Index and Consumer_Index
    for (int i = 0; i < node_num; ++i)
    {
        Json::Value Node = NodeList[i];
        if (strcmp(Node["operation"].asCString(), "OP_INPUT") == 0)
            continue;
        int provider_index = name2index_map[Node["provider"][0].asCString()];
        int input_dim_num = NodeList[provider_index]["output_dim_num"].asInt();
        DNNInfo["node_list"][i]["input_dim_num"] = input_dim_num;
        for (int j = 0; j < input_dim_num; ++j)
        {
            DNNInfo["node_list"][i]["input_dim"][j] = NodeList[provider_index]["output_dim"][j].asInt();
        }
    }
    for (int i = 0; i < node_num; ++i)
    {
        Json::Value Node = NodeList[i];
        int consumer_num = Node["consumer_num"].asInt();
        for (int j = 0; j < consumer_num; ++j)
        {
            std::string consumer_name = Node["consumer"][j].asCString();
            int consumer_index = name2index_map[consumer_name];
            DNNInfo["node_list"][i]["consumer_index"].append(consumer_index);
        }

        int provider_num = Node["provider_num"].asInt();
        for (int j = 0; j < provider_num; ++j)
        {
            std::string provider_name = Node["provider"][j].asCString();
            int provider_index = name2index_map[provider_name];
            DNNInfo["node_list"][i]["provider_index"].append(provider_index);
        }
    }
}

void GetStructNodeListFromJson(Json::Value & DNNInfo)
{
    Json::Value NodeList = DNNInfo["node_list"];
    int node_num = NodeList.size();
    for (int i = 0; i < node_num; ++i)
    {
        PIMCOM_node_list_origin[i].bitwidth = NodeList[i]["bitwidth"].asInt();
        PIMCOM_node_list_origin[i].consumer_num = NodeList[i]["consumer_num"].asInt();
        PIMCOM_node_list_origin[i].index = NodeList[i]["index"].asInt();
        PIMCOM_node_list_origin[i].input_dim_num = NodeList[i]["input_dim_num"].asInt();
        PIMCOM_node_list_origin[i].name = NodeList[i]["name"].asCString();
        PIMCOM_node_list_origin[i].operation = NodeList[i]["operation"].asCString();
        PIMCOM_node_list_origin[i].output_dim_num = NodeList[i]["output_dim_num"].asInt();
        PIMCOM_node_list_origin[i].provider_num = NodeList[i]["provider_num"].asInt();

        for (int j = 0; j < PIMCOM_node_list_origin[i].provider_num; ++j)
            PIMCOM_node_list_origin[i].provider_index.push_back(NodeList[i]["provider_index"][j].asInt());
        for (int j = 0; j < PIMCOM_node_list_origin[i].consumer_num; ++j)
            PIMCOM_node_list_origin[i].consumer_index.push_back(NodeList[i]["consumer_index"][j].asInt());
        for (int j = 0; j < PIMCOM_node_list_origin[i].input_dim_num; ++j)
            PIMCOM_node_list_origin[i].input_dim.push_back(NodeList[i]["input_dim"][j].asInt());
        for (int j = 0; j < PIMCOM_node_list_origin[i].output_dim_num; ++j)
            PIMCOM_node_list_origin[i].output_dim.push_back(NodeList[i]["output_dim"][j].asInt());

        if (strcmp(NodeList[i]["operation"].asCString(), "OP_FC") == 0)
        {
            PIMCOM_node_list_origin[i].param.num_input = NodeList[i]["param"]["num_input"].asInt();
            PIMCOM_node_list_origin[i].param.num_output = NodeList[i]["param"]["num_output"].asInt();
        }
        else if (strcmp(NodeList[i]["operation"].asCString(), "OP_CONV")==0 || strcmp(NodeList[i]["operation"].asCString(), "OP_POOL")==0)
        {
            PIMCOM_node_list_origin[i].param.kernel_h = NodeList[i]["param"]["kernel_h"].asInt();
            PIMCOM_node_list_origin[i].param.kernel_w = NodeList[i]["param"]["kernel_w"].asInt();
            PIMCOM_node_list_origin[i].param.stride_h = NodeList[i]["param"]["stride_h"].asInt();
            PIMCOM_node_list_origin[i].param.stride_w = NodeList[i]["param"]["stride_w"].asInt();
            PIMCOM_node_list_origin[i].param.pad_h0 = NodeList[i]["param"]["pad_h0"].asInt();
            PIMCOM_node_list_origin[i].param.pad_h1 = NodeList[i]["param"]["pad_h1"].asInt();
            PIMCOM_node_list_origin[i].param.pad_w0 = NodeList[i]["param"]["pad_w0"].asInt();
            PIMCOM_node_list_origin[i].param.pad_w1 = NodeList[i]["param"]["pad_w1"].asInt();

            if (strcmp(NodeList[i]["operation"].asCString(), "OP_CONV")==0)
            {
                PIMCOM_node_list_origin[i].param.dilation_h = NodeList[i]["param"]["dilation_h"].asInt();
                PIMCOM_node_list_origin[i].param.dilation_w = NodeList[i]["param"]["dilation_w"].asInt();
                PIMCOM_node_list_origin[i].param.input_channel = NodeList[i]["param"]["input_channel"].asInt();
                PIMCOM_node_list_origin[i].param.output_channel = NodeList[i]["param"]["output_channel"].asInt();
                PIMCOM_node_list_origin[i].param.group = NodeList[i]["param"]["group"].asInt();
                PIMCOM_node_list_origin[i].param.activation = NodeList[i]["param"]["activation"].asInt();
                PIMCOM_node_list_origin[i].param.wino_off = NodeList[i]["param"]["wino_off"].asInt();
            }
            else
            {
                PIMCOM_node_list_origin[i].param.pool_method = NodeList[i]["param"]["pool_method"].asInt();
            }
        }
        else if (strcmp(NodeList[i]["operation"].asCString(), "OP_FLATTEN")==0)
        {
            PIMCOM_node_list_origin[i].param.axis = NodeList[i]["param"]["axis"].asInt();
            PIMCOM_node_list_origin[i].param.end_axis = NodeList[i]["param"]["end_axis"].asInt();
        }
        else if (strcmp(NodeList[i]["operation"].asCString(), "OP_ELTWISE")==0)
        {
            PIMCOM_node_list_origin[i].param.eletype = NodeList[i]["param"]["eletype"].asInt();
            PIMCOM_node_list_origin[i].param.caffe_flavor = NodeList[i]["param"]["caffe_flavor"].asInt();
            PIMCOM_node_list_origin[i].param.shift = NodeList[i]["param"]["shift"].asFloat();
            PIMCOM_node_list_origin[i].param.power = NodeList[i]["param"]["power"].asFloat();
            PIMCOM_node_list_origin[i].param.scale = NodeList[i]["param"]["scale"].asFloat();
        }
    }
}

void ShowModelInfo()
{
    int node_num = PIMCOM_node_list_origin.size();
    std::cout << "#Nodes in total: " << node_num << std::endl;
    float weight_precession = 16;
    float weights = 0.0;
    float FC_weights = 0.0;
    for (int i = 0; i < node_num; ++i)
    {
        if(PIMCOM_node_list_origin[i].operation == "OP_CONV")
        {
            std::cout << i <<std::endl;
            float kernel = PIMCOM_node_list_origin[i].param.kernel_h;
            float input_channel = PIMCOM_node_list_origin[i].param.input_channel;
            float output_channel = PIMCOM_node_list_origin[i].param.output_channel;
            weights += kernel * kernel * input_channel * output_channel;
//            std::cout << "weight: " << kernel * kernel * input_channel * output_channel*weight_precession/8/1024/1024 << "MB" << std::endl;
            std::vector<int> Input = PIMCOM_node_list_origin[i].input_dim;
            std::cout << "input: " << float(Input[0]) * float(Input[1]) * float(Input[2]) * float(Input[3]) *weight_precession/8/1024 << "KB" << std::endl;
            std::vector<int> Output = PIMCOM_node_list_origin[i].output_dim;
            std::cout << "output: " << float(Output[0]) * float(Output[1]) * float(Output[2]) * float(Output[3]) *weight_precession/8/1024 << "KB" << std::endl;
        }
        else if (PIMCOM_node_list_origin[i].operation == "OP_FC")
        {
            float input_num = PIMCOM_node_list_origin[i].param.num_input;
            float output_num = PIMCOM_node_list_origin[i].param.num_output;
            weights += input_num * output_num;
            FC_weights += input_num * output_num;
//            std::cout << "weight: " << input_num*output_num*weight_precession/8/1024/1024 << "MB" << std::endl;
            std::vector<int> Output = PIMCOM_node_list_origin[i].output_dim;
            std::cout << "output: " << float(Output[0]) * float(Output[1]) * float(Output[2]) * float(Output[3]) *weight_precession/8/1024/1024 << "MB" << std::endl;

        }
    }
    std::cout << "FC Weight: " << FC_weights*weight_precession/8/1024/1024 << "MB" << std::endl;
    std::cout << "Sum Weight: " << weights*weight_precession/8/1024/1024 << "MB" << std::endl;
    std::cout << "FC Ratio: " << FC_weights/weights*100 << "%" << std::endl;
}


void CopyFromOriginNodeList()
{
    // std::map<int, PIMCOM_node> PIMCOM_node_list_origin;
    PIMCOM_node_list.clear();
    for (auto iter = PIMCOM_node_list_origin.begin(); iter != PIMCOM_node_list_origin.end() ; ++iter)
    {
        PIMCOM_node_list[iter->first] = iter->second;
    }
}


void GetConvPoolInputOutputInfo()
{
    int node_num = PIMCOM_node_list_origin.size();
    PIMCOM_conv_pool_input_output_info.resize(node_num);
    for (int n = 0; n < node_num; ++n)
    {
        struct PIMCOM_node Node = PIMCOM_node_list_origin[n];
        if (Node.operation != "OP_POOL" && Node.operation != "OP_CONV")
            continue;

        struct param Params = Node.param;
        int input_H = Node.input_dim[2];
        int input_W = Node.input_dim[3];
        int pool_kernel_w = Params.kernel_w;
        int pool_kernel_h = Params.kernel_h;
        int pool_padding_h0 = Params.pad_h0;
        int pool_padding_h1 = Params.pad_h1;
        int pool_padding_w0 = Params.pad_w0;
        int pool_padding_w1 = Params.pad_w1;
        int pool_stride_w = Params.stride_w;
        int pool_stride_h = Params.stride_h;

        int output_W = floor(float(input_W + pool_padding_w0 + pool_padding_w1 - pool_kernel_w) / float(pool_stride_w)) + 1;
        int output_H = floor(float(input_H + pool_padding_h0 + pool_padding_h1 - pool_kernel_h) / float(pool_stride_h)) + 1;
        int info_output_W = Node.output_dim[3];
        int info_output_H = Node.output_dim[2];
        if (info_output_W != output_W || info_output_H != output_H)
        {
            std::cout << " Output Size Doesn't Match" << std::endl;
            return;
        }

        PIMCOM_conv_pool_input_output_info[n].input_index.resize(input_H * input_W);
        PIMCOM_conv_pool_input_output_info[n].output_index.resize(info_output_W * info_output_H);

        int output_index = 0;
        int normal_start_index_in_w = pool_padding_w0/pool_stride_w + (pool_padding_w0 % pool_stride_w == 0 ? 0 : 1);
        int normal_start_index_in_h = pool_padding_h0/pool_stride_h + (pool_padding_h0 % pool_stride_h == 0 ? 0 : 1);
        for (int i = 0; i < output_H; ++i)
        {
            for (int j = 0; j < output_W; ++j)
            {
                int start_address = i * pool_stride_h * input_W + j *  pool_stride_w;

                if (j < normal_start_index_in_w)
                    start_address -= (j * pool_stride_w);
                else
                    start_address -= pool_padding_w0;

                if (i < normal_start_index_in_h)
                    start_address -= (i * pool_stride_h * input_W);
                else
                    start_address -= pool_padding_h0 * input_W;

                int start_row = start_address / input_W;
                int start_col = start_address % input_W;

                int pool_w_num = pool_kernel_w;
                if (j < normal_start_index_in_w)
                    pool_w_num = pool_w_num - pool_padding_w0 + j * pool_stride_w;
                if (start_col + pool_w_num > input_W)
                    pool_w_num = pool_w_num - (start_col + pool_w_num - input_W);

                int pool_h_num = pool_kernel_h;
                if (i < normal_start_index_in_h)
                    pool_h_num = pool_h_num - pool_padding_h0 + i * pool_stride_h;
                if (start_row + pool_h_num > input_H)
                    pool_h_num = pool_h_num - (start_row + pool_h_num - input_H);


                for (int h = 0; h < pool_h_num ; ++h)
                {
                    for (int w = 0; w < pool_w_num; ++w)
                    {
                        int position = start_address + w + h * input_W;
                        PIMCOM_conv_pool_input_output_info[n].input_index[position].push_back(output_index);
                        PIMCOM_conv_pool_input_output_info[n].output_index[output_index].push_back(position);
                    }
                }
                output_index += 1;
            }
        }
    }
}

//void GetConvInfo()
//{
//    int node_num = PIMCOM_node_list_origin.size();
//    PIMCOM_conv_pool_input_output_info.resize(node_num);
//    for (int n = 0; n < node_num; ++n)
//    {
//        struct PIMCOM_node Node = PIMCOM_node_list_origin[n];
//        if (Node.operation != "OP_CONV")
//            continue;
//        struct param Params = Node.param;
//        int input_H = Node.input_dim[2];
//        int input_W = Node.input_dim[3];
//        int conv_kernel_w = Params.kernel_w;
//        int conv_kernel_h = Params.kernel_h;
//        int conv_padding_h0 = Params.pad_h0;
//        int conv_padding_h1 = Params.pad_h1;
//        int conv_padding_w0 = Params.pad_w0;
//        int conv_padding_w1 = Params.pad_w1;
//        int conv_stride_w = Params.stride_w;
//        int conv_stride_h = Params.stride_h;
//
//        int output_W = floor(float(input_W + conv_padding_w0 + conv_padding_w1 - conv_kernel_w) / float(conv_stride_w)) + 1;
//        int output_H = floor(float(input_H + conv_padding_h0 + conv_padding_h1 - conv_kernel_h) / float(conv_stride_h)) + 1;
//        int info_output_W = Node.output_dim[3];
//        int info_output_H = Node.output_dim[2];
//        if (info_output_W != output_W || info_output_H != output_H)
//        {
//            std::cout << " Output Size Doesn't Match" << std::endl;
//            return;
//        }
//        PIMCOM_conv_pool_input_output_info[n].input_index.resize(input_H * input_W);
//        PIMCOM_conv_pool_input_output_info[n].output_index.resize(info_output_W * info_output_H);
//
//        int output_index = 0;
//        for (int i = 0; i < output_H; ++i)
//        {
//            for (int j = 0; j < output_W; ++j)
//            {
//                int start_address = i * conv_stride_h * input_W + j *  conv_stride_w;
//
//                if (i != 0)
//                    start_address -= conv_padding_h0 * input_W;
//                if (j != 0)
//                    start_address -= conv_padding_w0;
//
//                int start_row = start_address / input_W;
//                int start_col = start_address % input_W;
//
//                int conv_h_num = conv_kernel_h;
//                if (i == 0)
//                    conv_h_num -= conv_padding_h0;
//                else if (i == output_H-1)
//                    if (start_row + conv_kernel_h > input_H)
//                        conv_h_num -= conv_padding_h1;
//
//                int conv_w_num = conv_kernel_w;
//                if (j == 0)
//                    conv_w_num -= conv_padding_w0;
//                else if (j == output_W-1)
//                    if (start_col + conv_kernel_w > input_W)
//                        conv_w_num -= conv_padding_w1;
//
//                for (int h = 0; h < conv_h_num ; ++h)
//                {
//                    for (int w = 0; w < conv_w_num; ++w)
//                    {
//                        int position = start_address + w + h * input_W;
//                        PIMCOM_conv_pool_input_output_info[n].input_index[position].push_back(output_index);
//                        PIMCOM_conv_pool_input_output_info[n].output_index[output_index].push_back(position);
//                    }
//                }
//                output_index += 1;
//            }
//        }
//    }
//}
