//
// Created by SXT on 2022/9/6.
//

#ifndef PIMCOM_PIPELINEDESIGN_H
#define PIMCOM_PIPELINEDESIGN_H

#include "configure.h"
#include "common.h"

class PipelineDesign
{
public:
    void DesignPipeline(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
    void ShowClassificationInfo(Json::Value & DNNInfo);
    void ShowWaitToActInfo(Json::Value & DNNInfo);
private:
    Json::Value NodeList;
    int node_num;
    void ClassifyTheNode(int node_index, int level_index, int index_in_level);
    void GetAugmentedNodeList(Json::Value & DNNInfo);
    void RefineAugmentedNodeList(Json::Value & DNNInfo, int node_index, int level_index, int AG0_core_index, int AG0_index_in_total, int AG0_node_index);
    void GetConcatMaxLevelForInception();
    void GetPoolInfo(Json::Value & DNInfo);
};


#endif //PIMCOM_PIPELINEDESIGN_H
