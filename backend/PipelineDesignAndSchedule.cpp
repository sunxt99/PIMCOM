//
// Created by SXT on 2022/9/16.
//

#include "PipelineDesignAndSchedule.h"

/////////////////////////////////////////////// 【示例使用方式】///////////////////////////////////////////////
//// 在main函数中，替代InferencePipelineDesign和InferencePipelineSchedule
////PipelineDesignAndSchedule pipeline(RunMode);
////pipeline.DesignAndSchedule(model_name, Inference, ThisMode);

PipelineDesignAndSchedule::PipelineDesignAndSchedule(enum Mode RunMode)
{
    RunMode = RunMode;
}

void PipelineDesignAndSchedule::DesignAndSchedule(std::string model_name, enum PipelineType PipelineUse)
{
    switch (PipelineUse) {
        case Inference:
        {

            InferencePipelineDesign design;
            design.DesignPipeline();

            InferencePipelineSchedule schedule(RunMode);
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