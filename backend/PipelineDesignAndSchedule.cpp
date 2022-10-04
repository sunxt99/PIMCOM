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
            if (FastMode)
                design.DesignPipelineFast(DNNInfo);
            else
                design.DesignPipelineSlow(DNNInfo);
            design.SaveJsonIR(DNNInfo, model_name);


            InferencePipelineSchedule schedule;
            if (FastMode)
                schedule.ScheduleExecutionFast(DNNInfo);
            else
                schedule.ScheduleExecutionSlow(DNNInfo);
            if (FastMode)
                schedule.ScheduleSaveInstructionFast(DNNInfo);
            else
                schedule.ScheduleSaveInstructionSlow(DNNInfo);

//            if (FastMode)
//                schedule.ScheduleShowInstructionFast(DNNInfo);
//            else
//                schedule.ScheduleShowInstructionSlow(DNNInfo);
            schedule.SaveJsonIR(DNNInfo, model_name);

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