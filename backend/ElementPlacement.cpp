//
// Created by SXT on 2022/8/21.
//

#include "ElementPlacement.h"

//// 不应该是第4个步骤。应该在第8甚至更靠后。
//std::map<int,int> PIMCOM_4_physical_core_placement;
//std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_4_physical_crossbar_placement;

//void ElementPlacement::PlaceElement()
//{
//    PlaceCoreNaive();
//    PlaceCrossbarNaive();
//}

//void ElementPlacement::PlaceCoreNaive()
//{
//    for (int i = 0; i < ChipH * ChipW; ++i)
//    {
//        // index is virtual-index, value is physical-index
//        PIMCOM_4_physical_core_placement[i] = i;
//    }
//}

//void ElementPlacement::PlaceCrossbarNaive()
//{
//    int virtual_crossbar_num = PIMCOM_2_resource_info.RRAMS;
//    for (int i = 0; i < virtual_crossbar_num; ++i)
//    {
//        // Placement for RRAM Crossbar. Very, Very Naive and Straight
//        struct PIMCOM_2_virtual_crossbar pimcom2VirtualCrossbar = PIMCOM_2_virtual_crossbar[i];
//        pimcom2VirtualCrossbar.physical_position_in_core = pimcom2VirtualCrossbar.index_in_vcore;
//        // Record the Core Placement Info for RRAM Crossbar
//        pimcom2VirtualCrossbar.physical_core = PIMCOM_4_physical_core_placement[pimcom2VirtualCrossbar.vcore];
//        PIMCOM_4_physical_crossbar_placement.push_back(pimcom2VirtualCrossbar);
//    }
//}

//void ElementPlacement::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/4_ep.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}