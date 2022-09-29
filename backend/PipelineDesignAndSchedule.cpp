//
// Created by SXT on 2022/9/16.
//

#include "PipelineDesignAndSchedule.h"

void PipelineDesignAndSchedule::DesignAndSchedule(Json::Value & DNNInfo, std::string model_name, enum PipelineType PipelineUse)
{
    switch (PipelineUse) {
        case Inference:
        {
            clock_t timestamp = clock();
            InferencePipelineDesign design;
            design.DesignPipeline(DNNInfo);
//            design.ShowWaitToActInfo(DNNInfo);
//            design.ShowClassificationInfo(DNNInfo);
            design.SaveJsonIR(DNNInfo, model_name);
            clock_t timestamp_2 = clock();
            std::cout << double(timestamp_2 - timestamp) / CLOCKS_PER_SEC << "s" << std::endl;

            clock_t timestamp_3 = clock();
            InferencePipelineSchedule schedule;
            schedule.ScheduleExecution(DNNInfo);
            schedule.ScheduleShowInstruction(DNNInfo);
            schedule.SaveJsonIR(DNNInfo, model_name);
            clock_t timestamp_4 = clock();
            std::cout << double(timestamp_4 - timestamp_3) / CLOCKS_PER_SEC << "s" << std::endl;
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