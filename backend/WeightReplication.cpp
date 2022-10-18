//
// Created by SXT on 2022/8/19.
//

#include "WeightReplication.h"

//static int pre_replication_num[20] = {11, 10, 4, 2, 1, 4, 2, 11, 9, 4, 9, 2, 11};
static int pre_replication_num[20] = {2,3,1};

void WeightReplication::ReplicateWeight()
{
    ReplicateRandomly();
}


void WeightReplication::ReplicateRandomly()
{
    int node_num = PIMCOM_node_list.size();
    srand(unsigned(time(0)));
    int RandomUpper = 12;
    int RandomLower = 1;
    int appointed_index = 0;
    for (int i = 0; i < node_num; ++i)
    {
        if(PIMCOM_node_list[i].operation == "OP_CONV")
        {
            //// 随机指定
            int replication_num = rand() % (RandomUpper - RandomLower + 1) + RandomLower;
            std::cout << replication_num << ", ";
            PIMCOM_node_list[i].replication_num = replication_num;
            //// 固定倍数
//            PIMCOM_node_list[i].replication_num = 4;
            //// 指定倍数
//            PIMCOM_node_list[i].replication_num = pre_replication_num[appointed_index++];
        }
        else if (PIMCOM_node_list[i].operation == "OP_FC")
            PIMCOM_node_list[i].replication_num = 1;
    }
    std::cout << std::endl;
}


//void WeightReplication::SaveJsonIR(Json::Value & DNNInfo, std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/1_wr.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}