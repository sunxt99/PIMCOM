//
// Created by SXT on 2022/9/16.
//

#ifndef PIMCOM_ELEMENTPIPELINEDESIGN_H
#define PIMCOM_ELEMENTPIPELINEDESIGN_H

#include "../common.h"
#include "../configure.h"
#include "PIMCOMVariable.h"

class ElementPipelineDesign {
public:
    void DesignPipeline();
    void ShowClassificationInfo();
    void ShowWaitToActInfo(Json::Value & DNNInfo);
private:
    int node_num;
    void ClassifyTheNode(int node_index, int level_index, int index_in_level);
    void GetAugmentedNodeList();
    void RefineAugmentedNodeList(int node_index, int level_index, int AG0_core_index, int AG0_index_in_total, int AG0_node_index);
    void GetConcatMaxLevelForInception();
    void Clear();
};


#endif //PIMCOM_ELEMENTPIPELINEDESIGN_H
