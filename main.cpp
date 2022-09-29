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

void ShowModelInfo(Json::Value & DNNInfo)
{
    int node_num = DNNInfo["node_list"].size();
    std::cout << "#Nodes in total: " << node_num << std::endl;
    float weight_precession = 16;
    float weights = 0.0;
    float FC_weights = 0.0;
    for (int i = 0; i < node_num; ++i)
    {
        Json::Value Node = DNNInfo["node_list"][i];
        if (strcmp(Node["operation"].asCString(), "OP_CONV") == 0)
        {
            std::cout << i <<std::endl;
            Json::Value Param = Node["param"];
            float kernel = Param["kernel_h"].asFloat();
            float input_channel = Param["input_channel"].asFloat();
            float output_channel = Param["output_channel"].asFloat();
            weights += kernel * kernel * input_channel * output_channel;
//            std::cout << "weight: " << kernel * kernel * input_channel * output_channel*weight_precession/8/1024/1024 << "MB" << std::endl;
            Json::Value Input = Node["input_dim"];
            std::cout << "input: " << Input[0].asFloat() * Input[1].asFloat() * Input[2].asFloat() * Input[3].asFloat() *weight_precession/8/1024 << "KB" << std::endl;
            Json::Value Output = Node["output_dim"];
            std::cout << "output: " << Output[0].asFloat() * Output[1].asFloat() * Output[2].asFloat() * Output[3].asFloat() *weight_precession/8/1024 << "KB" << std::endl;
        }
        else if (strcmp(Node["operation"].asCString(), "OP_FC") == 0)
        {
            Json::Value Param = Node["param"];
            float input_num = Param["num_input"].asFloat();
            float output_num = Param["num_output"].asFloat();
            weights += input_num * output_num;
            FC_weights += input_num * output_num;
//            std::cout << "weight: " << input_num*output_num*weight_precession/8/1024/1024 << "MB" << std::endl;
            Json::Value Output = Node["output_dim"];
            std::cout << "output: " << Output[0].asFloat() * Output[1].asFloat() * Output[2].asFloat() * Output[3].asFloat()*weight_precession/8/1024/1024 << "MB" << std::endl;
        }
    }
    std::cout << "FC Weight: " << FC_weights*weight_precession/8/1024/1024 << "MB" << std::endl;
    std::cout << "Sum Weight: " << weights*weight_precession/8/1024/1024 << "MB" << std::endl;
    std::cout << "FC Ratio: " << FC_weights/weights*100 << "%" << std::endl;
}

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
    std::cout << "========================= MODEL INFO =========================" << std::endl;
    ShowModelInfo(DNNInfo);
    std::cout << "========================= MAPPING =========================" << std::endl;
    clock_t timestamp = clock();
    WeightReplication replication;
    replication.ReplicateWeight(DNNInfo);
    replication.SaveJsonIR(DNNInfo, model_name);
    CrossbarPartition partition;
    partition.PartitionCrossbar(DNNInfo);
    partition.SaveJsonIR(DNNInfo,model_name);
    HierarchyMapping mapping;
    mapping.MapHierarchy(DNNInfo);
    mapping.SaveJsonIR(DNNInfo, model_name);
    ElementPlacement placement;
    placement.PlaceElement(DNNInfo);
    placement.SaveJsonIR(DNNInfo, model_name);
    clock_t timestamp2 = clock();
    std::cout << double(timestamp2 - timestamp) / CLOCKS_PER_SEC << "s" << std::endl;

    std::cout << "========================= SCHEDULING =========================" << std::endl;
    enum PipelineType PipelineUse = Inference;
    PipelineDesignAndSchedule pipeline;
    pipeline.DesignAndSchedule(DNNInfo, model_name, PipelineUse);

    DetailAppend da;
    da.AppendDetail(DNNInfo);
    da.SaveJsonIR(DNNInfo, model_name);
    da.ShowDetailedInstruction(DNNInfo);

    MemoryAllocation allocation;
    allocation.AllocateMemory(DNNInfo);
    allocation.ShowInstruction(DNNInfo);
    allocation.SaveJsonIR(DNNInfo, model_name);

    std::cout << "========================= EVALUATING =========================" << std::endl;
    ModelEvaluation evaluation;
    evaluation.EvaluateModel(DNNInfo);

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
