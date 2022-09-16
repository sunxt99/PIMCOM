//
// Created by SXT on 2022/9/16.
//

#ifndef PIMCOM_ELEMENTPIPELINEDESIGN_H
#define PIMCOM_ELEMENTPIPELINEDESIGN_H
#include "configure.h"
#include "common.h"


class ElementPipelineDesign {
public:
    void ShowWaitToActInfo(Json::Value & DNNInfo);
private:
    int node_num;
    Json::Value NodeList;
};


#endif //PIMCOM_ELEMENTPIPELINEDESIGN_H
