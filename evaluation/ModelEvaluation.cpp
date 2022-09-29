//
// Created by SXT on 2022/9/20.
//

#include "ModelEvaluation.h"

void ModelEvaluation::EvaluateModel(Json::Value & DNNInfo)
{
    instruction_group_num = static_cast<int>(DNNInfo["6_base_instruction_ir"].size());
    core_num = static_cast<int>(DNNInfo["6_physical_core_AG_map"]["core_list"].size());
    InstructionGroup = DNNInfo["6_base_instruction_ir"][0]; // the first instruction_group
//    Json::Value tmp = InstructionGroup["core_list"][2]["instruction_ir_list"][4];
//    InstructionGroup["core_list"][2]["instruction_ir_list"][4] = InstructionGroup["core_list"][2]["instruction_ir_list"][5];
//    InstructionGroup["core_list"][2]["instruction_ir_list"][4]["instruction_index_in_core"] = 4;
//    InstructionGroup["core_list"][2]["instruction_ir_list"][5] = tmp;
//    InstructionGroup["core_list"][2]["instruction_ir_list"][5]["instruction_index_in_core"] = 5;
//    for (int i = 0; i < core_num; ++i)
//    {
//        std::cout << i << std::endl;
//        Json::Value InstructionIrList = InstructionGroup["core_list"][i]["instruction_ir_list"];
//        int instruction_ir_num = static_cast<int>(InstructionIrList.size());
//        for (int j = 0; j < instruction_ir_num; ++j)
//        {
//            Json::Value tmpInstruction = InstructionIrList[j];
//            std::cout << "  " << tmpInstruction["operation"]<<std::endl;
//        }
//    }
    EvaluateRecursion(DNNInfo, 0, 0);
    ShowEvaluationResult();
}


static int MVMUL_num[MAX_CORE] = {0};
static int VADD_num[MAX_CORE] = {0};
static int COMM_num[MAX_CORE] = {0};
static int DELAY[MAX_CORE] = {0};
static int Visited[MAX_CORE] = {0};
void ModelEvaluation::EvaluateRecursion(Json::Value & DNNInfo, int core_index, int index_in_core)
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
                EvaluateRecursion(DNNInfo, next_core_index, 0);
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
                EvaluateRecursion(DNNInfo, corresponding_core_index, corresponding_instruction_index_in_core+1);
                EvaluateRecursion(DNNInfo, core_index, instruction_index_in_core+1);
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
            EvaluateRecursion(DNNInfo, next_core_index, 0);
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