//
// Created by SXT on 2022/9/16.
//

#ifndef PIMCOM_PIPELINEDESIGNANDSCHEDULE_H
#define PIMCOM_PIPELINEDESIGNANDSCHEDULE_H

#include "common.h"
#include "configure.h"
#include "InferencePipelineDesign.h"
#include "InferencePipelineSchedule.h"
#include "RowPipelineDesign.h"
#include "RowPipelineSchedule.h"
#include "ElementPipelineDesign.h"

class PipelineDesignAndSchedule
{
public:
    void DesignAndSchedule(Json::Value & DNNInfo, std::string model_name, enum PipelineType PipelineUse);
};

#endif //PIMCOM_PIPELINEDESIGNANDSCHEDULE_H
