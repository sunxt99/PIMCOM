//
// Created by SXT on 2022/9/16.
//

#ifndef PIMCOM_PIPELINEDESIGNANDSCHEDULE_H
#define PIMCOM_PIPELINEDESIGNANDSCHEDULE_H

#include "../common.h"
#include "../configure.h"
#include "InferencePipelineDesign.h"
#include "InferencePipelineSchedule.h"
#include "RowPipelineDesign.h"
#include "RowPipelineSchedule.h"
#include "ElementPipelineDesign.h"
#include "ElementPipelineSchedule.h"
#include "PIMCOMVariable.h"

class PipelineDesignAndSchedule
{
public:
    PipelineDesignAndSchedule(enum Mode RunMode);
    void DesignAndSchedule(std::string model_name, enum PipelineType PipelineUse);
private:
    enum Mode RunMode;
    void GetInputAndOutputMappingInfo();
};

#endif //PIMCOM_PIPELINEDESIGNANDSCHEDULE_H
