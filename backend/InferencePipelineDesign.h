//
// Created by SXT on 2022/9/16.
//

#ifndef PIMCOM_INFERENCEPIPELINEDESIGN_H
#define PIMCOM_INFERENCEPIPELINEDESIGN_H

#include "common.h"
#include "../configure.h"

class InferencePipelineDesign
{
public:
    void DesignPipelineFast(Json::Value & DNNInfo);
    void DesignPipelineSlow(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
    void ShowClassificationInfoSlow(Json::Value & DNNInfo);
    void ShowClassificationInfoFast(Json::Value & DNNInfo);

private:
    Json::Value NodeList;
    int node_num;
    void ClassifyTheNodeFast(int node_index, int level_index, int index_in_level);
    void ClassifyTheNodeSlow(int node_index, int level_index, int index_in_level);
    void GetAugmentedNodeListFast(Json::Value & DNNInfo);
    void GetAugmentedNodeListSlow(Json::Value & DNNInfo);
    void RefineAugmentedNodeListFast(Json::Value & DNNInfo, int node_index, int level_index, int AG0_core_index, int AG0_index_in_total, int AG0_node_index);
    void RefineAugmentedNodeListSlow(Json::Value & DNNInfo, int node_index, int level_index, int AG0_core_index, int AG0_index_in_total, int AG0_node_index);
    void GetConcatMaxLevelForInceptionFast();
    void GetConcatMaxLevelForInceptionSlow();
    void GetPoolInfoFast(Json::Value & DNInfo);
    void GetPoolInfoSlow(Json::Value & DNInfo);
};


#endif //PIMCOM_INFERENCEPIPELINEDESIGN_H
