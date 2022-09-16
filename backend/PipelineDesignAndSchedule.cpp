//
// Created by SXT on 2022/9/16.
//

#include "PipelineDesignAndSchedule.h"

void PipelineDesignAndSchedule::DesignAndSchedule(Json::Value & DNNInfo, std::string model_name, enum PipelineType PipelineUse)
{
    switch (PipelineUse) {
        case Inference:
        {
            InferencePipelineDesign design;
            design.DesignPipeline(DNNInfo);
//            design.ShowWaitToActInfo(DNNInfo);
            design.SaveJsonIR(DNNInfo, model_name);
            design.ShowClassificationInfo(DNNInfo);
            InferencePipelineSchedule schedule;
            schedule.ScheduleExecution(DNNInfo);
            schedule.SaveJsonIR(DNNInfo, model_name);
//            schedule.ScheduleShowInstruction(DNNInfo);
            break;
        }
        case Row:
        {
            RowPipelineDesign design;
            design.DesignPipeline(DNNInfo);
            design.SaveJsonIR(DNNInfo, model_name);
            break;
        }
        case Element:
        {
            ElementPipelineDesign design;
            design.ShowWaitToActInfo(DNNInfo);
            break;
        }
    }
}