//
// Created by SXT on 2022/8/18.
//

#include "common.h"
#include "configure.h"
#include "WeightReplication.h"
#include "CrossbarPartition.h"
#include "HierarchyMapping.h"
#include "ElementPlacement.h"
#include "ExecutionSchedule.h"
#include "DetailAppend.h"
#include "PipelineDesign.h"

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
    Json::Value NodeList = DNNInfo["node_list"];
    int node_num = NodeList.size();
    std::cout << "#Nodes in total: " << node_num << std::endl;

    // Save the "Name-Index" key-value map
    std::map<std::string, int> name2index_map;
    std::map<std::string, int>::iterator  m_iter;

    for (int i = 0; i < node_num; ++i)
        name2index_map.insert(std::pair<std::string, int>(NodeList[i]["name"].asCString(),i));

    for (int i = 0; i < node_num; ++i)
    {
        DNNInfo["node_list"][i]["index"] = i;
    }

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

    WeightReplication wr;
    wr.ReplicateWeight(DNNInfo);
    wr.SaveJsonIR(DNNInfo, model_name);
    CrossbarPartition cp;
    cp.PartitionCrossbar(DNNInfo);
    cp.SaveJsonIR(DNNInfo,model_name);
    HierarchyMapping hm;
    hm.MapHierarchy(DNNInfo);
    hm.SaveJsonIR(DNNInfo, model_name);
    ElementPlacement ep;
    ep.PlaceElement(DNNInfo);
    ep.SaveJsonIR(DNNInfo, model_name);
    PipelineDesign pd;
    pd.DesignPipeline(DNNInfo);
    pd.SaveJsonIR(DNNInfo, model_name);
//    pd.ShowClassificationInfo(DNNInfo);
    ExecutionSchedule es;
    es.ScheduleExecution(DNNInfo);
    es.SaveJsonIR(DNNInfo, model_name);
    es.ScheduleShowInstruction(DNNInfo);
//    DetailAppend da;
//    da.AppendDetail(DNNInfo);
//    da.SaveJsonIR(DNNInfo, model_name);
//    da.ShowDetailedInstruction(DNNInfo);
}


int main()
{
    std::string Models[13] = {"vgg16",
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
                        "inceptionsim"};

//    for (int i = 0; i < 8; ++i)
//    {
//        std::string model_name = Models[i];
//        PIMCOM(model_name);
//        std::cout << "************************" << std::endl;
//    }

    std::string model_name = Models[12];

    PIMCOM(model_name);

//    Json::Reader jsonReader;
//    Json::Value DNNInfo;
//    std::ifstream jsonFile("../ir/"+model_name+"/1_wr.json");
//    if(!jsonReader.parse(jsonFile, DNNInfo, true))
//    {
//        std::cout << "error" << std::endl;
//        return -1;
//    }

//    CrossbarPartition cp;
//    cp.PartitionCrossbar(DNNInfo);
//    cp.SaveJsonIR(DNNInfo,model_name);
//    HierarchyMapping hm;
//    hm.MapHierarchy(DNNInfo);
//    hm.SaveJsonIR(DNNInfo, model_name);
//    ElementPlacement ep;
//    ep.PlaceElement(DNNInfo);
//    ep.SaveJsonIR(DNNInfo, model_name);
//
//    ExecutionSchedule es;
//    es.ScheduleExecution(DNNInfo);
//    es.SaveJsonIR(DNNInfo, model_name);
//
//    DetailAppend da;
//    da.AppendDetail(DNNInfo);
//    da.SaveJsonIR(DNNInfo, model_name);

}
