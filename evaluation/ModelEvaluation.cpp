//
// Created by SXT on 2022/9/20.
//

#include "ModelEvaluation.h"

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;
extern std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
extern struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
extern std::vector<int> PIMCOM_2_effective_node;
extern struct PIMCOM_3_hierarchy_map PIMCOM_3_hierarchy_map;
extern std::map<int, std::vector<int>> PIMCOM_3_virtual_core_crossbar_map;
extern std::map<int,int> PIMCOM_4_physical_core_placement;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_4_physical_crossbar_placement;
//extern std::map<int, struct PIMCOM_5_pool_info> PIMCOM_5_pool_info;
extern std::vector<struct PIMCOM_5_pool_info> PIMCOM_5_pool_info;
extern std::map<int, int> PIMCOM_6_AG_instruction_group_num;
extern struct PIMCOM_6_first_AG_info PIMCOM_6_first_AG_info;
extern struct PIMCOM_6_physical_core_AG_map PIMCOM_6_physical_core_AG_map;
extern struct PIMCOM_6_recv_info PIMCOM_6_recv_info;
extern std::vector<struct PIMCOM_6_instruction_ir> PIMCOM_6_base_instruction_ir;
extern std::vector<std::vector<int>> PIMCOM_6_input_cycle_record;
extern std::map<int, struct PIMCOM_6_instruction_ir> PIMCOM_6_post_instruction_ir;
extern std::map<int, struct PIMCOM_6_instruction_ir> PIMCOM_6_post_multi_core_instruction_ir;
extern struct PIMCOM_8_reload_info PIMCOM_8_reload_info;
extern std::vector<struct PIMCOM_6_instruction_ir> PIMCOM_8_base_instruction_with_reload;

void ModelEvaluation::EvaluateModel(Json::Value & DNNInfo)
{
    clock_t timestamp_1 = clock();
    if (FastMode)
    {
        instruction_group_num = PIMCOM_6_base_instruction_ir.size();
        core_num = PIMCOM_6_physical_core_AG_map.core_list.size();
        EvaluateRecursionFast(DNNInfo, 0, 0);
        ShowEvaluationResult();
    }
    else
    {
        instruction_group_num = static_cast<int>(DNNInfo["6_base_instruction_ir"].size());
        core_num = static_cast<int>(DNNInfo["6_physical_core_AG_map"]["core_list"].size());
        InstructionGroup = DNNInfo["6_base_instruction_ir"][0]; // the first instruction_group
        EvaluateRecursionSlow(DNNInfo, 0, 0);
        ShowEvaluationResult();
    }
    clock_t timestamp_2 = clock();
    std::cout << double(timestamp_2 - timestamp_1) / CLOCKS_PER_SEC << "s" << std::endl;
}


static int MVMUL_num[MAX_CORE] = {0};
static int VADD_num[MAX_CORE] = {0};
static int COMM_num[MAX_CORE] = {0};
static int DELAY[MAX_CORE] = {0};
static int Visited[MAX_CORE] = {0};

void ModelEvaluation::EvaluateRecursionFast(Json::Value & DNNInfo, int core_index, int index_in_core)
{
    if (core_index >= core_num)
        return;
    Visited[core_index] = 1;
    std::vector<struct INST> InstructionIrList =  PIMCOM_6_base_instruction_ir[0].core_list[core_index].instruction_ir_list;
    int instruction_ir_num = InstructionIrList.size();
    for (int k = index_in_core; k < instruction_ir_num; ++k)
    {
        struct INST tmpInstruction = InstructionIrList[k];
        if (tmpInstruction.operation == "SEND" || tmpInstruction.operation == "RECV")
        {
            int comm_index = tmpInstruction.comm_index;
            int instruction_index_in_core = tmpInstruction.instruction_index_in_core;
            if (comm_index_2_index_in_core_map.count(comm_index) == 0)
            {
                comm_index_2_index_in_core_map.insert(std::pair<int,int>(comm_index, instruction_index_in_core));
                comm_index_2_core_index.insert(std::pair<int,int>(comm_index, core_index));
                int next_core_index = core_index+1;
                while (next_core_index < core_num && Visited[next_core_index] != 0)
                {
                    next_core_index++;
                }
                EvaluateRecursionFast(DNNInfo, next_core_index, 0);
            }
            else
            {
                int corresponding_core_index = comm_index_2_core_index[comm_index];
                int corresponding_instruction_index_in_core = comm_index_2_index_in_core_map[comm_index];
                if (DELAY[core_index] > DELAY[corresponding_core_index])
                    DELAY[corresponding_core_index] = DELAY[core_index];
                else
                    DELAY[core_index] = DELAY[corresponding_core_index];
                DELAY[corresponding_core_index] += COMM_delay;
                DELAY[core_index] += COMM_delay;
                COMM_num[corresponding_core_index]++;
                COMM_num[core_index]++;
                EvaluateRecursionFast(DNNInfo, corresponding_core_index, corresponding_instruction_index_in_core+1);
                EvaluateRecursionFast(DNNInfo, core_index, instruction_index_in_core+1);
            }
            return;
        }
        else if (tmpInstruction.operation == "MVMUL")
        {
            MVMUL_num[core_index]++;
            DELAY[core_index] += MVMUL_delay;
        }
        else if (tmpInstruction.operation == "VADD")
        {
            VADD_num[core_index]++;
            DELAY[core_index] += VECTOR_delay;
        }

        if (k == instruction_ir_num-1)
        {
            int next_core_index = core_index+1;
            while (next_core_index < core_num && Visited[next_core_index] != 0)
            {
                next_core_index++;
            }
            EvaluateRecursionFast(DNNInfo, next_core_index, 0);
        }
    }
}


void ModelEvaluation::EvaluateRecursionSlow(Json::Value & DNNInfo, int core_index, int index_in_core)
{
    if (core_index >= core_num)
        return;
    Visited[core_index] = 1;
    Json::Value InstructionIrList = InstructionGroup["core_list"][core_index]["instruction_ir_list"];
    int instruction_ir_num = static_cast<int>(InstructionIrList.size());

    for (int k = index_in_core; k < instruction_ir_num; ++k)
    {
        Json::Value tmpInstruction = InstructionIrList[k];
        if (strcmp(tmpInstruction["operation"].asCString(),"SEND") == 0 || strcmp(tmpInstruction["operation"].asCString(),"RECV") == 0)
        {
            int comm_index = tmpInstruction["comm_index"].asInt();
            int instruction_index_in_core = tmpInstruction["instruction_index_in_core"].asInt();
            if (comm_index_2_index_in_core_map.count(comm_index) == 0)
            {
                comm_index_2_index_in_core_map.insert(std::pair<int,int>(comm_index, instruction_index_in_core));
                comm_index_2_core_index.insert(std::pair<int,int>(comm_index, core_index));
                int next_core_index = core_index+1;
                while (next_core_index < core_num && Visited[next_core_index] != 0)
                {
                    next_core_index++;
                }
                EvaluateRecursionSlow(DNNInfo, next_core_index, 0);
            }
            else
            {
                int corresponding_core_index = comm_index_2_core_index[comm_index];
                int corresponding_instruction_index_in_core = comm_index_2_index_in_core_map[comm_index];
                if (DELAY[core_index] > DELAY[corresponding_core_index])
                    DELAY[corresponding_core_index] = DELAY[core_index];
                else
                    DELAY[core_index] = DELAY[corresponding_core_index];
                DELAY[corresponding_core_index] += COMM_delay;
                DELAY[core_index] += COMM_delay;
                COMM_num[corresponding_core_index]++;
                COMM_num[core_index]++;
                EvaluateRecursionSlow(DNNInfo, corresponding_core_index, corresponding_instruction_index_in_core+1);
                EvaluateRecursionSlow(DNNInfo, core_index, instruction_index_in_core+1);
            }
            return;
        }
        else if (strcmp(tmpInstruction["operation"].asCString(),"MVMUL") == 0)
        {
            MVMUL_num[core_index]++;
            DELAY[core_index] += MVMUL_delay;
        }
        else if (strcmp(tmpInstruction["operation"].asCString(),"VADD") == 0)
        {
            VADD_num[core_index]++;
            DELAY[core_index] += VECTOR_delay;
        }

        if (k == instruction_ir_num-1)
        {
            int next_core_index = core_index+1;
            while (next_core_index < core_num && Visited[next_core_index] != 0)
            {
                next_core_index++;
            }
            EvaluateRecursionSlow(DNNInfo, next_core_index, 0);
        }
    }
}

void ModelEvaluation::ShowEvaluationResult()
{
    int ideal = 0;
    for (int i = 0; i < core_num; ++i)
    {
        if(MVMUL_num[i]*MVMUL_delay + VADD_num[i]*VECTOR_delay + COMM_num[i]*COMM_delay > ideal)
            ideal = MVMUL_num[i]*MVMUL_delay + VADD_num[i]*VECTOR_delay + COMM_num[i]*COMM_delay;
        std::cout << i << "  MVMUL:" << MVMUL_num[i] << "  VADD:" << VADD_num[i] << " COMM:" << COMM_num[i] << " DELAY: " << DELAY[i] << std::endl;
    }
    std::cout << "ideal:" << ideal << std::endl;
}