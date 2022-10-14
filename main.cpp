//
// Created by SXT on 2022/8/18.
//

#include "common.h"
#include "configure.h"
#include "backend/WeightReplication.h"
#include "backend/CrossbarPartition.h"
#include "backend/HierarchyMapping.h"
#include "backend/ElementPlacement.h"
#include "backend/PipelineDesignAndSchedule.h"
#include "backend/DetailAppend.h"
#include "backend/MemoryAllocation.h"
#include "evaluation/ModelEvaluation.h"
#include "backend/Preparation.h"


void PIMCOM(const std::string model_name)
{
    Json::Reader jsonReader;
    Json::Value DNNInfo;
    std::ifstream jsonFile("../models/JSON/"+model_name+".json");
    if(!jsonReader.parse(jsonFile, DNNInfo, true))
    {
        std::cout << "error" << std::endl;
        return ;
    }

    std::cout << "========================= PreProcessing =========================" << std::endl;
    PreProcess(DNNInfo);
    GetStructNodeListFromJson(DNNInfo);
    GetConvPoolInputOutputInfo();

//    std::cout << "========================= MODEL INFO =========================" << std::endl;
//    ShowModelInfo();

    clock_t timestamp_1;
    clock_t timestamp_2;
    timestamp_1 = clock();
    enum Mode RunMode = Generation;
    for (int i = 0; i < 1; ++i)
    {
        CopyFromOriginNodeList();
        clock_t timestamp_a = clock();
        std::cout << "copy: " << double(timestamp_a - timestamp_1) / CLOCKS_PER_SEC << "s" << std::endl;
//
        std::cout << "========================= MAPPING =========================" << std::endl;
//
        WeightReplication replication;
        replication.ReplicateWeight();
        clock_t timestamp_b = clock();
        std::cout << "replication: " << double(timestamp_b - timestamp_a) / CLOCKS_PER_SEC << "s" << std::endl;
//
        CrossbarPartition partition(RunMode);
        partition.PartitionCrossbar();
        clock_t timestamp_c = clock();
        std::cout << "partition: " << double(timestamp_c - timestamp_b) / CLOCKS_PER_SEC << "s" << std::endl;
//
        HierarchyMapping mapping(RunMode);
        mapping.MapHierarchy();
        clock_t timestamp_d = clock();
        std::cout << "mapping: " << double(timestamp_d - timestamp_c) / CLOCKS_PER_SEC << "s" << std::endl;
//        mapping.ShowMappingInfo();
//        mapping.ShowMVMULInfo();
//
        std::cout << "========================= SCHEDULING =========================" << std::endl;
//
        ElementPipelineDesign design;
        design.DesignPipeline();

        ElementPipelineSchedule schedule;
        schedule.ScheduleExecution();
        schedule.SaveInstruction();
//        schedule.SaveInstruction();

//        InferencePipelineDesign design;
//        design.DesignPipeline();

//        InferencePipelineSchedule schedule(Generation);
//        schedule.ScheduleExecution();
//        schedule.SaveInstruction();
//        clock_t timestamp_e = clock();
//        std::cout << "schedule: " << double(timestamp_e - timestamp_d) / CLOCKS_PER_SEC << "s" << std::endl;

//        MemoryAllocation allocation;
//        allocation.AllocateMemory();
//        allocation.SaveInstruction();

//        DetailAppend da;
//        da.AppendDetail();
//        da.SaveInstruction();

//        std::cout << "========================= EVALUATING =========================" << std::endl;
//        ModelEvaluation evaluation(Generation);
//        evaluation.EvaluateCompute();
//        evaluation.EvaluateMemory();
//        evaluation.EvaluationInstruction();
//        evaluation.EvaluationMVMUL();

        PIMCOM_node_list.clear();
        PIMCOM_2_AG_partition.clear();
        PIMCOM_2_virtual_crossbar.clear();
        PIMCOM_2_effective_node.clear();
        PIMCOM_3_hierarchy_map.whole.clear();
        PIMCOM_3_hierarchy_map.whole_index.clear();
        PIMCOM_3_virtual_core_crossbar_map.clear();
        PIMCOM_3_compute_crossbar_ratio.clear();
        PIMCOM_4_AG_instruction_group_num.clear();
        PIMCOM_4_first_AG_info.node_list.clear();
        PIMCOM_4_virtual_core_AG_map.core_list.clear();
        PIMCOM_4_recv_info.node_list.clear();
        PIMCOM_4_base_instruction_ir.clear();
        PIMCOM_4_input_cycle_record.clear();
        PIMCOM_4_post_instruction_ir.clear();
        PIMCOM_4_post_multi_core_instruction_ir.clear();
        PIMCOM_5_reload_info.core_list.clear();
        PIMCOM_5_base_instruction_with_reload.clear();
        PIMCOM_6_detailed_instruction_ir.detailed_1_prepare_for_input.instruction_group.clear();
        PIMCOM_4_unique_instruction_group_index.clear();
        PIMCOM_4_evaluation_instruction_group_index.clear();
        PIMCOM_4_core_instruction_group_num.clear();
        PIMCOM_3_mapping_result.clear();
        PIMCOM_4_effective_provider_consumer_relation.clear();
        PIMCOM_4_effective_provider_consumer_relation.clear();
        PIMCOM_4_provider_consumer_relation_with_pool.clear();
        PIMCOM_4_consumer_provider_relation_with_pool.clear();
    }
//    std::cout << "========================= TIME STATISTIC =========================" << std::endl;
    timestamp_2 = clock();
    std::cout << double(timestamp_2 - timestamp_1) / CLOCKS_PER_SEC << "s" << std::endl;
}


int main()
{
    std::string Models[20] = {"vgg16",
                        "inception-v1",
                        "alexnet",
                        "resnet18",
                        "CR",
                        "CRP",
                        "CRPF",
                        "vggsim",
                        "schedule",
                        "schedule2",
                        "agp_example",
                        "resnetsim",
                        "inceptionsim",
                        "row",
                        "row2"};

//    for (int i = 0; i < 8; ++i)
//    {
//        std::string model_name = Models[i];
//        PIMCOM(model_name);
//        std::cout << "************************" << std::endl;
//    }

    std::string model_name = Models[0];
    PIMCOM(model_name);
}
