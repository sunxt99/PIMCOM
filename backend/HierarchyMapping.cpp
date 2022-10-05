//
// Created by SXT on 2022/8/19.
//

#include "HierarchyMapping.h"

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;
extern std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
extern struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
extern std::vector<int> PIMCOM_2_effective_node;

struct PIMCOM_3_hierarchy_map PIMCOM_3_hierarchy_map;
std::map<int, std::vector<int>> PIMCOM_3_virtual_core_crossbar_map;

static int ArrayGroupIndex = 0;

HierarchyMapping::HierarchyMapping()
{
    for (int i = 0; i < ChipW * ChipH; ++i)
    {
        ResourceList[i] = CoreW * CoreH;
    }
}

void HierarchyMapping::MapHierarchy()
{
    ShowOriginalInfo();
    MapNaive();
    Check();
}

void HierarchyMapping::MapNaive()
{
    int node_num_partition = PIMCOM_2_AG_partition.size();
    for (int i = 0; i < node_num_partition; ++i)
    {
        int replication_num = PIMCOM_2_AG_partition[i].replication_num;
        for (int j = 0; j < replication_num; ++j)
        {
            int ag_num_current_replication = PIMCOM_2_AG_partition[i].replication[j].AG_list.size();
            for (int k = 0; k < ag_num_current_replication; ++k)
            {
                int crossbar_need_current_ag = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_crossbar_list.size();
                bool mapped = false;
                for (int l = 0; l < ChipW * ChipH; ++l)
                {
                    if (crossbar_need_current_ag <= ResourceList[l])
                    {
                        struct std::vector<struct PIMCOM_2_virtual_crossbar> whole;
                        for (int m = 0; m < crossbar_need_current_ag; ++m)
                        {
                            int crossbar_index = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_crossbar_list[m];
                            PIMCOM_2_virtual_crossbar[crossbar_index].vcore_index = l;
                            PIMCOM_3_virtual_core_crossbar_map[l].push_back(crossbar_index);
                            PIMCOM_2_virtual_crossbar[crossbar_index].vcore = l;
                            PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_core_list[m] = l;
                            PIMCOM_2_virtual_crossbar[crossbar_index].index_in_vcore = CoreW*CoreH-ResourceList[l];
                            ResourceList[l] -= 1;
                            whole.push_back(PIMCOM_2_virtual_crossbar[crossbar_index]);
                        }
                        PIMCOM_3_hierarchy_map.whole.push_back(whole);
                        PIMCOM_3_hierarchy_map.whole_index.push_back(ArrayGroupIndex);
//                        ResourceList[l] -= crossbar_need_current_ag;
                        mapped = true;
                        break;
                    }
                }
                if (!mapped)
                {
                    struct std::vector<struct PIMCOM_2_virtual_crossbar> split;
                    int rram_current_index = 0;
                    for (int l = 0; l < ChipW * ChipH; ++l)
                    {
                        if (crossbar_need_current_ag <= ResourceList[l])
                        {
                            for (int m = 0; m < crossbar_need_current_ag; ++m)
                            {
                                int crossbar_index = PIMCOM_2_AG_partition[j].replication[j].AG_list[k].virtual_crossbar_list[rram_current_index++];
                                PIMCOM_2_virtual_crossbar[crossbar_index].vcore_index = l;
                                PIMCOM_3_virtual_core_crossbar_map[l].push_back(crossbar_index);
                                PIMCOM_2_virtual_crossbar[crossbar_index].vcore = l;
                                PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_core_list[rram_current_index] = l;
                                PIMCOM_2_virtual_crossbar[crossbar_index].index_in_vcore = CoreW*CoreH-ResourceList[l];
                                ResourceList[l] -= 1;
                                split.push_back(PIMCOM_2_virtual_crossbar[crossbar_index]);
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
                                int crossbar_index = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_crossbar_list[rram_current_index++];
                                PIMCOM_2_virtual_crossbar[crossbar_index].vcore_index = l;
                                PIMCOM_3_virtual_core_crossbar_map[l].push_back(crossbar_index);
                                PIMCOM_2_virtual_crossbar[crossbar_index].vcore = l;
                                PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_core_list[rram_current_index] = l;
                                PIMCOM_2_virtual_crossbar[crossbar_index].index_in_vcore = CoreW*CoreH-ResourceList[l];
                                ResourceList[l] -= 1;
                                split.push_back(PIMCOM_2_virtual_crossbar[crossbar_index]);
                            }
//                            ResourceList[l] = 0;
                        }
                    }
                    PIMCOM_3_hierarchy_map.split.push_back(split);
                    PIMCOM_3_hierarchy_map.split_index.push_back(ArrayGroupIndex);
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
    std::cout << "split AG num: " << PIMCOM_3_hierarchy_map.split_index.size() << std::endl;
}


void HierarchyMapping::ShowOriginalInfo()
{
    int v = 0;
    int node_num_partition = PIMCOM_2_AG_partition.size();
    for (int i = 0; i < node_num_partition; ++i)
    {
        std::cout << "Node Index: " << PIMCOM_2_AG_partition[i].index << ", Operation: " << PIMCOM_2_AG_partition[i].operation << std::endl;
        int replication_num = PIMCOM_2_AG_partition[i].replication_num;
        for (int j = 0; j < replication_num; ++j)
        {
            std::cout << "|---Replication Index: " << j << std::endl;
            int ag_num_current_replication = PIMCOM_2_AG_partition[i].replication[j].AG_list.size();
            for (int k = 0; k < ag_num_current_replication; ++k)
            {
                std::cout << "|-------Array Group Index: " << v << ", Crossbar Number: "
                          << PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_crossbar_list.size() << std::endl;
                v ++;
            }
        }
    }
}

void HierarchyMapping::Check()
{
    // 统计crossbar的数量
    int core_num = PIMCOM_3_virtual_core_crossbar_map.size();
    int FLAG[10000] = {0};
    for (int i = 0; i < core_num; ++i)
    {
        int rram_num = PIMCOM_3_virtual_core_crossbar_map[i].size();
        for (int j = 0; j < rram_num; ++j)
        {
            int index = PIMCOM_3_virtual_core_crossbar_map[i][j];
            FLAG[index] = 1;
        }
    }
    int sum = 0;
    for (int i = 0; i < 10000; ++i)
    {
        sum += FLAG[i];
    }
    if (sum == PIMCOM_2_resource_info.RRAMS)
        std::cout << "map check pass!" << std::endl;
}

//void HierarchyMapping::SaveJsonIR(Json::Value & DNNInfo, std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/3_hm.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}