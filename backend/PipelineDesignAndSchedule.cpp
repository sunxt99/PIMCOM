//
// Created by SXT on 2022/9/16.
//

#include "PipelineDesignAndSchedule.h"

void PipelineDesignAndSchedule::DesignAndSchedule(std::string model_name, enum PipelineType PipelineUse)
{
    switch (PipelineUse) {
        case Inference:
        {
            InferencePipelineDesign design;
            design.DesignPipeline();

            InferencePipelineSchedule schedule;
            schedule.ScheduleExecution();
//            schedule.SaveInstruction();
//            schedule.ShowInstruction();
            break;
        }
        case Row:
        {
//            RowPipelineDesign design;
//            design.DesignPipeline(DNNInfo);
//            design.SaveJsonIR(DNNInfo, model_name);
            break;
        }
        case Element:
        {
//            ElementPipelineDesign design;
//            design.ShowWaitToActInfo(DNNInfo);
            break;
        }
    }
}