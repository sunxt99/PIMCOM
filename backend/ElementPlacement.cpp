//
// Created by SXT on 2022/8/21.
//

#include "ElementPlacement.h"

void ElementPlacement::PlaceElement(Json::Value &DNNInfo)
{
    PlaceCoreNaive(DNNInfo);
    PlaceCrossbarNaive(DNNInfo);
}

void ElementPlacement::PlaceCoreNaive(Json::Value &DNNInfo)
{
    for (int i = 0; i < ChipH * ChipW; ++i)
    {
        // index is virtual-index, value is physical-index
        DNNInfo["4_physical_core_placement"].append(i);
    }
}


void ElementPlacement::PlaceCrossbarNaive(Json::Value &DNNInfo)
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