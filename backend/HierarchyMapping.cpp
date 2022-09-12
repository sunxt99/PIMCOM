//
// Created by SXT on 2022/8/19.
//

#include "HierarchyMapping.h"

static int ArrayGroupIndex = 0;

HierarchyMapping::HierarchyMapping()
{
    for (int i = 0; i < ChipW * ChipH; ++i)
    {
        ResourceList[i] = CoreW * CoreH;
    }
}


void HierarchyMapping::MapHierarchy(Json::Value &DNNInfo)
{
//    ShowOriginalInfo(DNNInfo);
    MapNaive(DNNInfo);
    Check(DNNInfo);
}


void HierarchyMapping::MapNaive(Json::Value &DNNInfo)
{
    Json::Value NodeList = DNNInfo["node_list"];
    Json::Value AGPartition = DNNInfo["2_AG_partition"];
    int node_num_partition = AGPartition.size();
    for (int i = 0; i < node_num_partition; ++i)
    {
        int replication_num = AGPartition[i]["replication_num"].asInt();
        for (int j = 0; j < replication_num; ++j)
        {
            int ag_num_current_replication = AGPartition[i]["replication"][j]["AG_list"].size();
            for (int k = 0; k < ag_num_current_replication; ++k)
            {
                int crossbar_need_current_ag = AGPartition[i]["replication"][j]["AG_list"][k]["virtual_crossbar_list"].size();
                bool mapped = false;
                for (int l = 0; l < ChipW * ChipH; ++l)
                {
                    if (crossbar_need_current_ag <= ResourceList[l])
                    {
                        Json::Value CrossbarMap;
                        for (int m = 0; m < crossbar_need_current_ag; ++m)
                        {
                            int crossbar_index = AGPartition[i]["replication"][j]["AG_list"][k]["virtual_crossbar_list"][m].asInt();
                            Json::Value Crossbar = DNNInfo["2_virtual_crossbar"][crossbar_index];
                            Crossbar["vcore_index"] = l;
                            //
                            DNNInfo["3_virtual_core_crossbar_map"][l].append(crossbar_index);
                            DNNInfo["2_virtual_crossbar"][crossbar_index]["vcore"] = l;
                            DNNInfo["2_AG_partition"][i]["replication"][j]["AG_list"][k]["virtual_core_list"][m] = l;
                            DNNInfo["2_virtual_crossbar"][crossbar_index]["index_in_vcore"] = CoreW*CoreH-ResourceList[l];
                            Crossbar["index_in_vcore"] = CoreW*CoreH-ResourceList[l];
                            ResourceList[l] -= 1;
                            CrossbarMap.append(Crossbar);
                        }
                        DNNInfo["3_hierarchy_map"]["whole"].append(CrossbarMap);
                        DNNInfo["3_hierarchy_map"]["whole_index"].append(ArrayGroupIndex);
//                        ResourceList[l] -= crossbar_need_current_ag;
                        mapped = true;
                        break;
                    }
                }
                if (!mapped)
                {
                    Json::Value CrossbarMap;
                    int rram_current_index = 0;
                    for (int l = 0; l < ChipW * ChipH; ++l)
                    {
                        if (crossbar_need_current_ag <= ResourceList[l])
                        {
                            for (int m = 0; m < crossbar_need_current_ag; ++m)
                            {
                                int crossbar_index = AGPartition[i]["replication"][j]["AG_list"][k]["virtual_crossbar_list"][rram_current_index++].asInt();
                                Json::Value Crossbar = DNNInfo["2_virtual_crossbar"][crossbar_index];
                                Crossbar["vcore_index"] = l;
                                //
                                DNNInfo["3_virtual_core_crossbar_map"][l].append(crossbar_index);
                                DNNInfo["2_virtual_crossbar"][crossbar_index]["vcore"] = l;
                                DNNInfo["2_AG_partition"][i]["replication"][j]["AG_list"][k]["virtual_core_list"][rram_current_index] = l;
                                DNNInfo["2_virtual_crossbar"][crossbar_index]["index_in_vcore"] = CoreW*CoreH-ResourceList[l];
                                Crossbar["index_in_vcore"] = CoreW*CoreH-ResourceList[l];
                                ResourceList[l] -= 1;
                                CrossbarMap.append(Crossbar);
                            }
//                            ResourceList[l] -= crossbar_need_current_ag;
                            break;
                        }
                        else
                        {
                            crossbar_need_current_ag -= ResourceList[l];
                            int resource_rest = ResourceList[l];
                            for (int m = 0; m < resource_rest; ++m)
                            {
                                int crossbar_index = AGPartition[i]["replication"][j]["AG_list"][k]["virtual_crossbar_list"][rram_current_index++].asInt();
                                Json::Value Crossbar = DNNInfo["2_virtual_crossbar"][crossbar_index];
                                Crossbar["vcore_index"] = l;
                                //
                                DNNInfo["3_virtual_core_crossbar_map"][l].append(crossbar_index);
                                DNNInfo["2_virtual_crossbar"][crossbar_index]["vcore"] = l;
                                DNNInfo["2_AG_partition"][i]["replication"][j]["AG_list"][k]["virtual_core_list"][rram_current_index] = l;
                                DNNInfo["2_virtual_crossbar"][crossbar_index]["index_in_vcore"] = CoreW*CoreH-ResourceList[l];
                                Crossbar["index_in_vcore"] = CoreW*CoreH-ResourceList[l];
                                ResourceList[l] -= 1;
                                CrossbarMap.append(Crossbar);
                            }
//                            ResourceList[l] = 0;
                        }
                    }
                    DNNInfo["3_hierarchy_map"]["split"].append(CrossbarMap);
                    DNNInfo["3_hierarchy_map"]["split_index"].append(ArrayGroupIndex);
                }
                ArrayGroupIndex += 1;
            }
        }
    }
    for (int i = 0; i < ChipW * ChipH; ++i)
    {
        std::cout << ResourceList[i] << " ";
    }
    ArrayGroupIndex = 0;
    std::cout << std::endl;
    std::cout << "split AG num: " << DNNInfo["3_hierarchy_map"]["split_index"].size() << std::endl;
}


void HierarchyMapping::ShowOriginalInfo(Json::Value &DNNInfo)
{
    int v = 0;
    Json::Value NodeList = DNNInfo["node_list"];
    Json::Value AGPartition = DNNInfo["2_AG_partition"];
    int node_num_partition = AGPartition.size();
    for (int i = 0; i < node_num_partition; ++i)
    {
        std::cout << "Node Index: " << AGPartition[i]["index"].asInt() << ", Operation: " <<AGPartition[i]["operation"].asCString() << std::endl;
        int replication_num = AGPartition[i]["replication_num"].asInt();
        for (int j = 0; j < replication_num; ++j)
        {
            std::cout << "|---Replication Index: " << j << std::endl;
            int ag_num_current_replication = AGPartition[i]["replication"][j]["AG_list"].size();
            for (int k = 0; k < ag_num_current_replication; ++k)
            {
                std::cout << "|-------Array Group Index: " << v << ", Crossbar Number: "
                          << AGPartition[i]["replication"][j]["AG_list"][k]["virtual_crossbar_list"].size() << std::endl;
                v ++;
            }
        }
    }
}

void HierarchyMapping::Check(Json::Value &DNNInfo)
{
    // 统计crossbar的数量。若等于
    int core_num = DNNInfo["3_virtual_core_crossbar_map"].size();
    int FLAG[10000] = {0};
    for (int i = 0; i < core_num; ++i)
    {
        int rram_num = DNNInfo["3_virtual_core_crossbar_map"][i].size();
        for (int j = 0; j < rram_num; ++j)
        {
            int index = DNNInfo["3_virtual_core_crossbar_map"][i][j].asInt();
            FLAG[index] = 1;
        }
    }
    int sum = 0;
    for (int i = 0; i < 10000; ++i)
    {
        sum += FLAG[i];
    }
    if (sum == DNNInfo["2_resource_info"]["RRAMS"].asInt())
        std::cout << "map check pass!" << std::endl;
}

void HierarchyMapping::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/3_hm.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}