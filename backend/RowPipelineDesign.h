//
// Created by SXT on 2022/9/16.
//

#ifndef PIMCOM_ROWPIPELINEDESIGN_H
#define PIMCOM_ROWPIPELINEDESIGN_H
#include "../common.h"
#include "../configure.h"

class RowPipelineDesign {
public:
    void DesignPipeline(Json::Value & DNNInfo);
    void TryRowPipeline1();
    void TryRowPipeline2(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    int node_num;
    int effective_node_num;
    Json::Value NodeList;
    Json::Value EffectiveNodeList;
    void GetStartPosition(Json::Value & DNNInfo);
    void GetRecvStartInfo(Json::Value & DNNInfo);
    void ShowRecvStartInfo(Json::Value & DNNInfo);
};

#endif //PIMCOM_ROWPIPELINEDESIGN_H
