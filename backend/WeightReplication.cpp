//
// Created by SXT on 2022/8/19.
//

#include "WeightReplication.h"

static int pre_replication_num[20] = {2,3,1};

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;
void WeightReplication::ReplicateWeight(Json::Value& DNNInfo)
{
    if (FastMode)
        ReplicateRandomlyFast(DNNInfo);
    else
        ReplicateRandomlySlow(DNNInfo);
}


void WeightReplication::ReplicateRandomlyFast(Json::Value& DNNInfo)
{
    int node_num = PIMCOM_node_list.size();
    std::random_device rd;
    auto gen = std::default_random_engine(rd());
    std::uniform_int_distribution<int> dis(1,3);
    int I = 0;
    for (int i = 0; i < node_num; ++i)
    {
        if(PIMCOM_node_list[i].operation == "OP_CONV")
        {
            // 随机指定
            int replication_num = dis(gen);
//            PIMCOM_node_list[i].replication_num = replication_num;
            // 固定倍数
            PIMCOM_node_list[i].replication_num = 2;
            // 指定倍数 [2, 3, 1]
//            PIMCOM_node_list[i].replication_num = pre_replication_num[I++];
        }
        else if (PIMCOM_node_list[i].operation == "OP_FC")
            PIMCOM_node_list[i].replication_num = 1;
    }
}

void WeightReplication::ReplicateRandomlySlow(Json::Value& DNNInfo)
{
    Json::Value NodeList = DNNInfo["node_list"];
    int node_num = NodeList.size();
    std::random_device rd;
    auto gen = std::default_random_engine(rd());
    std::uniform_int_distribution<int> dis(1,3);
    int I = 0;
    for (int i = 0; i < node_num; ++i)
    {
        Json::Value Node = NodeList[i];
        if (strcmp(Node["operation"].asCString(),"OP_CONV") == 0)
        {
            // 随机指定
            int replication_num = dis(gen);
//            DNNInfo["node_list"][i]["replication_num"] = replication_num;
            // 固定倍数
            DNNInfo["node_list"][i]["replication_num"] = 2;
            // 指定倍数 [2, 3, 1]
//            DNNInfo["node_list"][i]["replication_num"] = pre_replication_num[I++];
        }
        else if (strcmp(Node["operation"].asCString(),"OP_FC") == 0)
            DNNInfo["node_list"][i]["replication_num"] = 1;
    }
}

void WeightReplication::SaveJsonIR(Json::Value & DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/1_wr.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}