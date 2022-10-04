//
// Created by SXT on 2022/8/21.
//

#include "ElementPlacement.h"

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;
extern std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
extern struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
extern std::vector<int> PIMCOM_2_effective_node;
extern struct PIMCOM_3_hierarchy_map PIMCOM_3_hierarchy_map;
extern std::map<int, std::vector<int>> PIMCOM_3_virtual_core_crossbar_map;
std::map<int,int> PIMCOM_4_physical_core_placement;
std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_4_physical_crossbar_placement;

void ElementPlacement::PlaceElement(Json::Value &DNNInfo)
{
    if (FastMode)
    {
        PlaceCoreNaiveFast(DNNInfo);
        PlaceCrossbarNaiveFast(DNNInfo);
    }
    else
    {
        PlaceCoreNaiveSlow(DNNInfo);
        PlaceCrossbarNaiveSlow(DNNInfo);
    }
}

void ElementPlacement::PlaceCoreNaiveFast(Json::Value &DNNInfo)
{
    for (int i = 0; i < ChipH * ChipW; ++i)
    {
        PIMCOM_4_physical_core_placement[i] = i;
    }
}


void ElementPlacement::PlaceCrossbarNaiveFast(Json::Value &DNNInfo)
{
    int virtual_crossbar_num = PIMCOM_2_resource_info.RRAMS;
    for (int i = 0; i < virtual_crossbar_num; ++i)
    {
        struct PIMCOM_2_virtual_crossbar pimcom2VirtualCrossbar = PIMCOM_2_virtual_crossbar[i];
        pimcom2VirtualCrossbar.physical_position_in_core = pimcom2VirtualCrossbar.index_in_vcore;
        pimcom2VirtualCrossbar.physical_core = PIMCOM_4_physical_core_placement[pimcom2VirtualCrossbar.vcore];
        PIMCOM_4_physical_crossbar_placement.push_back(pimcom2VirtualCrossbar);
    }
}


void ElementPlacement::PlaceCoreNaiveSlow(Json::Value &DNNInfo)
{
    for (int i = 0; i < ChipH * ChipW; ++i)
    {
        // index is virtual-index, value is physical-index
        DNNInfo["4_physical_core_placement"].append(i);
    }
}


void ElementPlacement::PlaceCrossbarNaiveSlow(Json::Value &DNNInfo)
{
    int virtual_crossbar_num = DNNInfo["2_resource_info"]["RRAMS"].asInt();
    for (int i = 0; i < virtual_crossbar_num; ++i)
    {
        Json::Value Crossbar = DNNInfo["2_virtual_crossbar"][i];
        // Placement for RRAM Crossbar. Very, Very Naive and Straight
        Crossbar["physical_position_in_core"] = Crossbar["index_in_vcore"].asInt();
        // Record the Core Placement Info for RRAM Crossbar
        int vcore = Crossbar["vcore"].asInt();
        Crossbar["physical_core"] = DNNInfo["4_physical_core_placement"][vcore];
        DNNInfo["4_physical_crossbar_placement"].append(Crossbar);
    }
}

void ElementPlacement::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/4_ep.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}