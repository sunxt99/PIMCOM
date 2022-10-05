//
// Created by SXT on 2022/8/18.
//

#include "backend/common.h"
#include "configure.h"
#include "backend/WeightReplication.h"
#include "backend/CrossbarPartition.h"
#include "backend/HierarchyMapping.h"
#include "backend/ElementPlacement.h"
#include "backend/PipelineDesignAndSchedule.h"
#include "backend/DetailAppend.h"
#include "backend/MemoryAllocation.h"
#include "evaluation/ModelEvaluation.h"

std::map<int, struct PIMCOM_node> PIMCOM_node_list;

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
        PIMCOM_node_list[i].bitwidth = NodeList[i]["bitwidth"].asInt();
        PIMCOM_node_list[i].consumer_num = NodeList[i]["consumer_num"].asInt();
        PIMCOM_node_list[i].index = NodeList[i]["index"].asInt();
        PIMCOM_node_list[i].input_dim_num = NodeList[i]["input_dim_num"].asInt();
        PIMCOM_node_list[i].name = NodeList[i]["name"].asCString();
        PIMCOM_node_list[i].operation = NodeList[i]["operation"].asCString();
        PIMCOM_node_list[i].output_dim_num = NodeList[i]["output_dim_num"].asInt();
        PIMCOM_node_list[i].provider_num = NodeList[i]["provider_num"].asInt();

        for (int j = 0; j < PIMCOM_node_list[i].provider_num; ++j)
            PIMCOM_node_list[i].provider_index.push_back(NodeList[i]["provider_index"][j].asInt());
        for (int j = 0; j < PIMCOM_node_list[i].consumer_num; ++j)
            PIMCOM_node_list[i].consumer_index.push_back(NodeList[i]["consumer_index"][j].asInt());
        for (int j = 0; j < PIMCOM_node_list[i].input_dim_num; ++j)
            PIMCOM_node_list[i].input_dim.push_back(NodeList[i]["input_dim"][j].asInt());
        for (int j = 0; j < PIMCOM_node_list[i].output_dim_num; ++j)
            PIMCOM_node_list[i].output_dim.push_back(NodeList[i]["output_dim"][j].asInt());

        if (strcmp(NodeList[i]["operation"].asCString(), "OP_FC") == 0)
        {
            PIMCOM_node_list[i].param.num_input = NodeList[i]["param"]["num_input"].asInt();
            PIMCOM_node_list[i].param.num_output = NodeList[i]["param"]["num_output"].asInt();
        }
        else if (strcmp(NodeList[i]["operation"].asCString(), "OP_CONV")==0 || strcmp(NodeList[i]["operation"].asCString(), "OP_POOL")==0)
        {
            PIMCOM_node_list[i].param.kernel_h = NodeList[i]["param"]["kernel_h"].asInt();
            PIMCOM_node_list[i].param.kernel_w = NodeList[i]["param"]["kernel_w"].asInt();
            PIMCOM_node_list[i].param.stride_h = NodeList[i]["param"]["stride_h"].asInt();
            PIMCOM_node_list[i].param.stride_w = NodeList[i]["param"]["stride_w"].asInt();
            PIMCOM_node_list[i].param.pad_h0 = NodeList[i]["param"]["pad_h0"].asInt();
            PIMCOM_node_list[i].param.pad_h1 = NodeList[i]["param"]["pad_h1"].asInt();
            PIMCOM_node_list[i].param.pad_w0 = NodeList[i]["param"]["pad_w0"].asInt();
            PIMCOM_node_list[i].param.pad_w1 = NodeList[i]["param"]["pad_w1"].asInt();

            if (strcmp(NodeList[i]["operation"].asCString(), "OP_CONV")==0)
            {
                PIMCOM_node_list[i].param.dilation_h = NodeList[i]["param"]["dilation_h"].asInt();
                PIMCOM_node_list[i].param.dilation_w = NodeList[i]["param"]["dilation_w"].asInt();
                PIMCOM_node_list[i].param.input_channel = NodeList[i]["param"]["input_channel"].asInt();
                PIMCOM_node_list[i].param.output_channel = NodeList[i]["param"]["output_channel"].asInt();
                PIMCOM_node_list[i].param.group = NodeList[i]["param"]["group"].asInt();
                PIMCOM_node_list[i].param.activation = NodeList[i]["param"]["activation"].asInt();
                PIMCOM_node_list[i].param.wino_off = NodeList[i]["param"]["wino_off"].asInt();
            }
            else
            {
                PIMCOM_node_list[i].param.pool_method = NodeList[i]["param"]["pool_method"].asInt();
            }
        }
        else if (strcmp(NodeList[i]["operation"].asCString(), "OP_FLATTEN")==0)
        {
            PIMCOM_node_list[i].param.axis = NodeList[i]["param"]["axis"].asInt();
            PIMCOM_node_list[i].param.end_axis = NodeList[i]["param"]["end_axis"].asInt();
        }
        else if (strcmp(NodeList[i]["operation"].asCString(), "OP_ELTWISE")==0)
        {
            PIMCOM_node_list[i].param.eletype = NodeList[i]["param"]["eletype"].asInt();
            PIMCOM_node_list[i].param.caffe_flavor = NodeList[i]["param"]["caffe_flavor"].asInt();
            PIMCOM_node_list[i].param.shift = NodeList[i]["param"]["shift"].asFloat();
            PIMCOM_node_list[i].param.power = NodeList[i]["param"]["power"].asFloat();
            PIMCOM_node_list[i].param.scale = NodeList[i]["param"]["scale"].asFloat();
        }
    }
}

void ShowModelInfo()
{
    int node_num = PIMCOM_node_list.size();
    std::cout << "#Nodes in total: " << node_num << std::endl;
    float weight_precession = 16;
    float weights = 0.0;
    float FC_weights = 0.0;
    for (int i = 0; i < node_num; ++i)
    {
        if(PIMCOM_node_list[i].operation == "OP_CONV")
        {
            std::cout << i <<std::endl;
            float kernel = PIMCOM_node_list[i].param.kernel_h;
            float input_channel = PIMCOM_node_list[i].param.input_channel;
            float output_channel = PIMCOM_node_list[i].param.output_channel;
            weights += kernel * kernel * input_channel * output_channel;
//            std::cout << "weight: " << kernel * kernel * input_channel * output_channel*weight_precession/8/1024/1024 << "MB" << std::endl;
            std::vector<int> Input = PIMCOM_node_list[i].input_dim;
            std::cout << "input: " << float(Input[0]) * float(Input[1]) * float(Input[2]) * float(Input[3]) *weight_precession/8/1024 << "KB" << std::endl;
            std::vector<int> Output = PIMCOM_node_list[i].output_dim;
            std::cout << "output: " << float(Output[0]) * float(Output[1]) * float(Output[2]) * float(Output[3]) *weight_precession/8/1024 << "KB" << std::endl;
        }
        else if (PIMCOM_node_list[i].operation == "OP_FC")
        {
            float input_num = PIMCOM_node_list[i].param.num_input;
            float output_num = PIMCOM_node_list[i].param.num_output;
            weights += input_num * output_num;
            FC_weights += input_num * output_num;
//            std::cout << "weight: " << input_num*output_num*weight_precession/8/1024/1024 << "MB" << std::endl;
            std::vector<int> Output = PIMCOM_node_list[i].output_dim;
            std::cout << "output: " << float(Output[0]) * float(Output[1]) * float(Output[2]) * float(Output[3]) *weight_precession/8/1024/1024 << "MB" << std::endl;

        }
    }
    std::cout << "FC Weight: " << FC_weights*weight_precession/8/1024/1024 << "MB" << std::endl;
    std::cout << "Sum Weight: " << weights*weight_precession/8/1024/1024 << "MB" << std::endl;
    std::cout << "FC Ratio: " << FC_weights/weights*100 << "%" << std::endl;
}

void PIMCOM(const std::string model_name)
{
    Json::Reader jsonReader;
    Json::Value DNNInfo;
    std::ifstream jsonFile("../models/JSON/"+model_name+".json");
    if(!jsonReader.parse(jsonFile, DNNInfo, true))
    {
        std::cout << "error" << std::endl;
        return ;
    }

    PreProcess(DNNInfo);
    GetStructNodeListFromJson(DNNInfo);

    std::cout << "========================= MODEL INFO =========================" << std::endl;
    ShowModelInfo();

    clock_t timestamp_1 = clock();
    std::cout << "========================= MAPPING =========================" << std::endl;
    WeightReplication replication;
    replication.ReplicateWeight();

    CrossbarPartition partition;
    partition.PartitionCrossbar();

    HierarchyMapping mapping;
    mapping.MapHierarchy();

    ElementPlacement placement;
    placement.PlaceElement();

    std::cout << "========================= SCHEDULING =========================" << std::endl;
    enum PipelineType PipelineUse = Inference;
    PipelineDesignAndSchedule pipeline;
    pipeline.DesignAndSchedule(model_name, PipelineUse);

    MemoryAllocation allocation;
    allocation.AllocateMemory();
    allocation.SaveInstruction();

    DetailAppend da;
    da.AppendDetail();
    da.SaveInstruction();

    std::cout << "========================= EVALUATING =========================" << std::endl;
    ModelEvaluation evaluation;
    evaluation.EvaluateModel();
    clock_t timestamp_2 = clock();
    std::cout << double(timestamp_2 - timestamp_1) / CLOCKS_PER_SEC << "s" << std::endl;
}

int main()
{
    std::string Models[20] = {"vgg16",
                        "inception-v1",
                        "alexnet",
                        "resnet18",
                        "CR",
                        "CRP",
                        "CRPF",
                        "vggsim",
                        "schedule",
                        "schedule2",
                        "agp_example",
                        "resnetsim",
                        "inceptionsim",
                        "row",
                        "row2"};

//    for (int i = 0; i < 8; ++i)
//    {
//        std::string model_name = Models[i];
//        PIMCOM(model_name);
//        std::cout << "************************" << std::endl;
//    }

    std::string model_name = Models[0];
    PIMCOM(model_name);
}
