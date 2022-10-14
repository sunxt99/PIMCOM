//
// Created by SXT on 2022/8/19.
//

#include "HierarchyMapping.h"

static int ArrayGroupIndex = 0;

HierarchyMapping::HierarchyMapping(enum Mode RunMode)
{
    MapMode = RunMode;
    node_num = PIMCOM_node_list.size();
    for (int i = 0; i < ChipW * ChipH; ++i)
    {
        ResourceList[i] = CoreW * CoreH;
    }
}


void HierarchyMapping::MapHierarchy()
{
//    ShowOriginalInfo();
    switch (MapMode)
    {
        case Generation:
        {
            MapNaive();
//            MapDistributed();
            core_num = PIMCOM_3_virtual_core_crossbar_map.size();
            AllocateMapInfo(); // 以前这部分在InferencePipelineSchedule中，现在放到该部分。
            Check();
            break;
        }
        case Exploration:
        {
//            MapNaive();
            MapDistributed();
//            ShowMappingInfoDistributed();
            ShowMemoryInfo(); // 只有Exploration阶段能用
//            SaveMemoryInfo();
            break;
        }
    }
    Clear();
}




///////////////////////////////////////////////////// Naive Method /////////////////////////////////////////////////////
void HierarchyMapping::MapNaive()
{
    int node_num_partition = PIMCOM_2_AG_partition.size();
    for (int i = 0; i < node_num_partition; ++i)
    {
        int replication_num = PIMCOM_2_AG_partition[i].replication_num;
        for (int j = 0; j < replication_num; ++j)
        {
            int ag_num_current_replication = PIMCOM_2_AG_partition[i].replication[j].AG_list.size();
            for (int k = 0; k < ag_num_current_replication; ++k)
            {
                int crossbar_need_current_ag = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_crossbar_list.size();
                bool mapped = false;
                for (int l = 0; l < ChipW * ChipH; ++l)
                {
                    if (crossbar_need_current_ag <= ResourceList[l])
                    {
                        struct std::vector<struct PIMCOM_2_virtual_crossbar> whole;
                        for (int m = 0; m < crossbar_need_current_ag; ++m)
                        {
                            int crossbar_index = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_crossbar_list[m];
                            PIMCOM_2_virtual_crossbar[crossbar_index].vcore_index = l;
                            PIMCOM_3_virtual_core_crossbar_map[l].push_back(crossbar_index);
                            PIMCOM_2_virtual_crossbar[crossbar_index].vcore = l;
                            PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_core_list[m] = l;
                            PIMCOM_2_virtual_crossbar[crossbar_index].index_in_vcore = CoreW*CoreH-ResourceList[l];
                            ResourceList[l] -= 1;
                            whole.push_back(PIMCOM_2_virtual_crossbar[crossbar_index]);
                        }
                        PIMCOM_3_hierarchy_map.whole.push_back(whole);
                        PIMCOM_3_hierarchy_map.whole_index.push_back(ArrayGroupIndex);
//                        ResourceList[l] -= crossbar_need_current_ag;
                        mapped = true;
                        break;
                    }
                }
                if (!mapped) // 这里暂时不考虑，也就是不考虑一个AG被拆分的情况
                {
                    struct std::vector<struct PIMCOM_2_virtual_crossbar> split;
                    int rram_current_index = 0;
                    for (int l = 0; l < ChipW * ChipH; ++l)
                    {
                        if (crossbar_need_current_ag <= ResourceList[l])
                        {
                            for (int m = 0; m < crossbar_need_current_ag; ++m)
                            {
                                int crossbar_index = PIMCOM_2_AG_partition[j].replication[j].AG_list[k].virtual_crossbar_list[rram_current_index++];
                                PIMCOM_2_virtual_crossbar[crossbar_index].vcore_index = l;
                                PIMCOM_3_virtual_core_crossbar_map[l].push_back(crossbar_index);
                                PIMCOM_2_virtual_crossbar[crossbar_index].vcore = l;
                                PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_core_list[rram_current_index] = l;
                                PIMCOM_2_virtual_crossbar[crossbar_index].index_in_vcore = CoreW*CoreH-ResourceList[l];
                                ResourceList[l] -= 1;
                                split.push_back(PIMCOM_2_virtual_crossbar[crossbar_index]);
                            }
//                            ResourceList[l] -= crossbar_need_current_ag;
                            break;
                        }
                        else
                        {
                            crossbar_need_current_ag -= ResourceList[l];
                            int resource_rest = ResourceList[l];
                            for (int m = 0; m < resource_rest; ++m)
                            {
                                int crossbar_index = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_crossbar_list[rram_current_index++];
                                PIMCOM_2_virtual_crossbar[crossbar_index].vcore_index = l;
                                PIMCOM_3_virtual_core_crossbar_map[l].push_back(crossbar_index);
                                PIMCOM_2_virtual_crossbar[crossbar_index].vcore = l;
                                PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_core_list[rram_current_index] = l;
                                PIMCOM_2_virtual_crossbar[crossbar_index].index_in_vcore = CoreW*CoreH-ResourceList[l];
                                ResourceList[l] -= 1;
                                split.push_back(PIMCOM_2_virtual_crossbar[crossbar_index]);
                            }
//                            ResourceList[l] = 0;
                        }
                    }
                    PIMCOM_3_hierarchy_map.split.push_back(split);
                    PIMCOM_3_hierarchy_map.split_index.push_back(ArrayGroupIndex);
                }
                ArrayGroupIndex += 1;
            }
        }
    }
    for (int i = 0; i < ChipW * ChipH; ++i)
    {
        std::cout << ResourceList[i] << " ";
    }
    std::cout << std::endl;
    std::cout << "split AG num: " << PIMCOM_3_hierarchy_map.split_index.size() << std::endl;
}



////////////////////////////////////////////////// Distributed Method //////////////////////////////////////////////////
int cmp(const std::pair<struct MapSortStruct, int> &x, const std::pair<struct MapSortStruct, int> &y) { return x.first.ratio > y.first.ratio; }
int cmp2(const struct AGMapStruct &x, const struct AGMapStruct &y) {return x.AG_index_in_total < y.AG_index_in_total;}

void HierarchyMapping::MapDistributed()
{
    //// 预处理，得到排序后的节点序列
    for (int i = 0; i < PIMCOM_2_effective_node.size(); ++i)
    {
        struct MapSortStruct tmp;
        int input_cycle_per_replication = PIMCOM_2_AG_partition[i].replication[0].input_cycle_this_replication;
//        int crossbar_num_per_AG = PIMCOM_2_AG_partition[i].replication[0].AG_list[0].virtual_crossbar_list.size();
        int crossbar_num_per_AG = PIMCOM_2_AG_partition[i].crossbar_num_per_AG;
//        int AG_num_per_replication = PIMCOM_2_AG_partition[i].replication[0].AG_list.size() ;
        int AG_num_per_replication = PIMCOM_2_AG_partition[i].AG_num_per_replication;
        int node_index = PIMCOM_2_AG_partition[i].index;
        int height = PIMCOM_2_AG_partition[i].Height;
        int crossbar_num_per_replication = AG_num_per_replication * crossbar_num_per_AG;
        float ratio = float(input_cycle_per_replication) / float(crossbar_num_per_replication);
        tmp.input_cycle_per_replication = input_cycle_per_replication;
        tmp.crossbar_num_per_AG = crossbar_num_per_AG;
        tmp.AG_num_per_replication = AG_num_per_replication;
        tmp.crossbar_num_per_replication = crossbar_num_per_replication;
        tmp.node_index = node_index;
        tmp.operation = PIMCOM_node_list[node_index].operation;
        tmp.height = height;
        tmp.ratio = ratio;
        PIMCOM_3_compute_crossbar_ratio.push_back(std::pair<struct MapSortStruct, int>(tmp,i));
    }
    std::sort(PIMCOM_3_compute_crossbar_ratio.begin(), PIMCOM_3_compute_crossbar_ratio.end(), cmp);

    GatherForAccelerate();

    //// 多次调用分散映射函数，得到合法解
    int try_index = 0;
    PIMCOM_3_mapping_result.resize(ChipH * ChipW);
    while (!MapDistributedTry())
    {
        if (try_index > 1000)
        {
            throw std::runtime_error("Mapping Failed. Please Change Replication Num");
        }
        try_index ++ ;
        for (int i = 0; i < ChipW * ChipH; ++i)
        {
            ResourceList[i] = CoreW * CoreH;
        }
        PIMCOM_3_mapping_result.clear();
        PIMCOM_3_mapping_result.resize(ChipH * ChipW);
    }

    //// 对结果进行排序，保证每个核中的AG_index是递增的
    for (int i = 0; i < PIMCOM_3_mapping_result.size(); ++i)
    {
        std::sort(PIMCOM_3_mapping_result[i].begin(), PIMCOM_3_mapping_result[i].end(), cmp2);
        int mvmul_num = 0;
        for (int j = 0; j < PIMCOM_3_mapping_result[i].size(); ++j)
        {
            mvmul_num += PIMCOM_3_mapping_result[i][j].input_cycle_num;
        }
        std::cout << mvmul_num << " " ;
    }
    std::cout << std::endl;
    std::cout << "distributed mapping done" << std::endl;
//    int sss = 0; for (int j = 0; j < ChipH * ChipW; ++j) { std::cout << ResourceList[j] << " "; sss += ResourceList[j]; } std::cout << "  Total:" << sss << std::endl;
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////   准备函数  //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int HierarchyMapping::GetInputElementNumFromAG(int node_index, int index_in_replication)
{
    int H = 0;
    if (PIMCOM_node_list[node_index].operation == "OP_CONV")
    {
        H = PIMCOM_node_list[node_index].param.kernel_h * PIMCOM_node_list[node_index].param.kernel_w * PIMCOM_node_list[node_index].param.input_channel;
    }
    else if(PIMCOM_node_list[node_index].operation == "OP_FC")
    {
        H = PIMCOM_node_list[node_index].param.num_input;
    }
    int AG_H_num_in_replication = ceil(float(H) / float(CrossbarH));
    if (index_in_replication == AG_H_num_in_replication-1)
    {
        return (H - (AG_H_num_in_replication-1)*CrossbarH);
    }
    else
    {
        return CrossbarH;
    }
}

std::vector<int> ACC_input_window_element_num; // [i] 代表node_index为i的完整输出长度
std::vector<std::vector<int>> ACC_input_element_num_from_AG_index_in_replication; // [i][j] 代表node_index为i、AG_index_in_replication为j的AG对应的输入长度
void HierarchyMapping::GatherForAccelerate()
{
    ACC_input_window_element_num.resize(node_num);
    ACC_input_element_num_from_AG_index_in_replication.resize(node_num);
    for (int i = 0; i < PIMCOM_node_list.size(); ++i)
    {
        if (PIMCOM_node_list[i].operation == "OP_CONV" || PIMCOM_node_list[i].operation == "OP_FC")
        {
            ACC_input_window_element_num[i] = PIMCOM_node_list[i].H; // 实际上只有CONV需要这个参数
        }
    }
    for (int i = 0; i < PIMCOM_3_compute_crossbar_ratio.size(); ++i)
    {
        struct MapSortStruct node_AG_hybrid_info = PIMCOM_3_compute_crossbar_ratio[i].first;
        int node_index = node_AG_hybrid_info.node_index;
        int AG_num_per_replication = node_AG_hybrid_info.AG_num_per_replication;
        int H = node_AG_hybrid_info.height;
        std::string operation = node_AG_hybrid_info.operation;
        for (int j = 0; j < AG_num_per_replication; ++j)
        {
            if (j == AG_num_per_replication-1)
                ACC_input_element_num_from_AG_index_in_replication[node_index].push_back(H - (AG_num_per_replication-1) * CrossbarH);
            else
                ACC_input_element_num_from_AG_index_in_replication[node_index].push_back(CrossbarH);
        }
    }
}

int HierarchyMapping::MapDistributedTry()
{
    srand(unsigned(time(0)));
    int RandomLower = 0;
    int RandomUpper = 0;
    int accumulated_replication_num = 0;
    for (auto iter = PIMCOM_3_compute_crossbar_ratio.begin();  iter != PIMCOM_3_compute_crossbar_ratio.end() ; iter++)
    {
        int effective_node_index = iter->second;
        int AG_num_this_replication = iter->first.AG_num_per_replication;
        int crossbar_num_per_AG = iter->first.crossbar_num_per_AG;
        int crossbar_num_this_replication = iter->first.crossbar_num_per_replication;
        int node_index = PIMCOM_2_AG_partition[effective_node_index].index;
        std::string operation = PIMCOM_node_list[node_index].operation;
        int output_element_num_for_AG = operation == "OP_CONV" ? (PIMCOM_node_list[node_index].param.output_channel) : (PIMCOM_node_list[node_index].param.num_output);
        int replication_num = PIMCOM_2_AG_partition[effective_node_index].replication_num;
//        std::cout << std::endl;
//        std::cout << "crossbar_num_this_replication:" << crossbar_num_this_replication << std::endl;
//        std::cout << "replication_num:" << replication_num << std::endl;
        bool distributed = effective_node_index < 2 ? 1:0;
        for (int i = 0; i < replication_num; ++i)
        {
            bool replication_mapped = false;
            if (distributed)
            {
                for (int j = (i + accumulated_replication_num) % (ChipH * ChipH); j < (i + accumulated_replication_num) % (ChipH * ChipH) + (ChipH * ChipH); ++j)
                {
                    int index_j = j;
                    if (index_j >= ChipH * ChipW)
                    {
                        index_j -= ChipH * ChipW;
                    }
                    if (ResourceList[index_j] >= crossbar_num_this_replication)
                    {
                        ResourceList[index_j] -= crossbar_num_this_replication;
                        replication_mapped = true;

                        for (int k = 0; k < AG_num_this_replication; ++k)
                        {
                            struct AGMapStruct thisAG;
                            thisAG.node_index = node_index;
                            thisAG.replication_index = i;
                            thisAG.index_in_replication = k;
                            thisAG.input_element_num = ACC_input_element_num_from_AG_index_in_replication[node_index][k];
                            thisAG.output_element_num = output_element_num_for_AG;
                            // AG_index_in_total暂时不考虑了，没必要
                            // thisAG.AG_index_in_total = PIMCOM_2_AG_partition[effective_node_index].replication[i].AG_index[k];
                            thisAG.input_cycle_num = PIMCOM_2_AG_partition[effective_node_index].replication[i].input_cycle_this_replication;
                            thisAG.instruction_group_num = ceil(float(thisAG.input_cycle_num) / float(operation_cycle_before_comm));
                            PIMCOM_3_mapping_result[index_j].push_back(thisAG);
                        }

                        if (MapMode == Generation)
                        {
                            for (int k = 0; k < AG_num_this_replication; ++k)
                            {
                                struct std::vector<struct PIMCOM_2_virtual_crossbar> whole;
                                int AG_index = PIMCOM_2_AG_partition[effective_node_index].replication[i].AG_list[k].AG_index;
                                for (int l = 0; l < crossbar_num_per_AG; ++l)
                                {
                                    int crossbar_index = PIMCOM_2_AG_partition[effective_node_index].replication[i].AG_list[k].virtual_crossbar_list[l];
                                    PIMCOM_2_virtual_crossbar[crossbar_index].vcore_index = index_j;
                                    PIMCOM_2_virtual_crossbar[crossbar_index].vcore = index_j;
                                    PIMCOM_2_virtual_crossbar[crossbar_index].index_in_vcore = k * crossbar_num_per_AG + l;
                                    PIMCOM_2_AG_partition[effective_node_index].replication[i].AG_list[k].virtual_core_list[l] = index_j;
                                    PIMCOM_3_virtual_core_crossbar_map[index_j].push_back(crossbar_index);
                                    whole.push_back(PIMCOM_2_virtual_crossbar[crossbar_index]);
                                }
                                PIMCOM_3_hierarchy_map.whole.push_back(whole);
                                PIMCOM_3_hierarchy_map.whole_index.push_back(AG_index);
                            }
                        }
                        break;
                    }
                }
            }
            if (!distributed || !replication_mapped)
            {
//                std::cout << "#########################################################" << std::endl;
//                std::cout << "crossbar_num_per_AG:" << crossbar_num_per_AG << std::endl;
//                std::cout << "AG_num_per_replication:" << AG_num_this_replication << std::endl;
                int already_AG_num = 0;
                bool AG_mapped = false;
                for (int j = (i + accumulated_replication_num) % (ChipH * ChipH); j < ((ChipH * ChipW)+(i+accumulated_replication_num)%(ChipH * ChipH)); ++j)
                {
                    int index_j = j;
                    if (index_j >= ChipH * ChipW)
                        index_j -= ChipH*ChipW;
                    for (int k = already_AG_num; k < AG_num_this_replication; ++k)
                    {
                        if (ResourceList[index_j] >= crossbar_num_per_AG)
                        {
                            ResourceList[index_j] -= crossbar_num_per_AG;
                            already_AG_num++;

                            struct AGMapStruct thisAG;
                            thisAG.node_index = node_index;
                            thisAG.replication_index = i;
                            thisAG.index_in_replication = k;
                            thisAG.input_element_num = ACC_input_element_num_from_AG_index_in_replication[node_index][k];
                            thisAG.output_element_num = output_element_num_for_AG;
                            // AG_index_in_total暂时不考虑了。没必要。
                            // thisAG.AG_index_in_total = PIMCOM_2_AG_partition[effective_node_index].replication[i].AG_index[k];
                            thisAG.input_cycle_num = PIMCOM_2_AG_partition[effective_node_index].replication[i].input_cycle_this_replication;
                            thisAG.instruction_group_num = ceil(float(thisAG.input_cycle_num) / float(operation_cycle_before_comm));
                            PIMCOM_3_mapping_result[index_j].push_back(thisAG);

                            if (MapMode == Generation)
                            {
                                struct std::vector<struct PIMCOM_2_virtual_crossbar> whole;
                                int AG_index = PIMCOM_2_AG_partition[effective_node_index].replication[i].AG_list[k].AG_index;
                                for (int l = 0; l < crossbar_num_per_AG; ++l)
                                {
                                    int crossbar_index = PIMCOM_2_AG_partition[effective_node_index].replication[i].AG_list[k].virtual_crossbar_list[l];
                                    PIMCOM_2_virtual_crossbar[crossbar_index].vcore_index = index_j;
                                    PIMCOM_2_virtual_crossbar[crossbar_index].vcore = index_j;
                                    PIMCOM_2_virtual_crossbar[crossbar_index].index_in_vcore = k * crossbar_num_per_AG + l;
                                    PIMCOM_2_AG_partition[effective_node_index].replication[i].AG_list[k].virtual_core_list[l] = index_j;
                                    PIMCOM_3_virtual_core_crossbar_map[index_j].push_back(crossbar_index);
                                    whole.push_back(PIMCOM_2_virtual_crossbar[crossbar_index]);
                                }
                                PIMCOM_3_hierarchy_map.whole.push_back(whole);
                                PIMCOM_3_hierarchy_map.whole_index.push_back(AG_index);
                            }

                            if (already_AG_num == AG_num_this_replication)
                            {
                                AG_mapped = true;
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                    if (AG_mapped)
                    {
                        break;
                    }
                }
            }
        }
//        int sss = 0; for (int j = 0; j < ChipH * ChipW; ++j) { std::cout << ResourceList[j] << " "; sss += ResourceList[j]; } std::cout << "  Total:" << sss << std::endl;
        int random_factor_3 = rand() % (RandomUpper - RandomLower + 1) + RandomLower + 1;
        accumulated_replication_num += replication_num * random_factor_3;
    }
    int sss = 0;
    for (int j = 0; j < ChipH * ChipW; ++j)
    { sss += ResourceList[j]; }
    return (ChipH*ChipW*CoreH*CoreW-sss == PIMCOM_2_resource_info.RRAMS);
}



static int AG_flags[MAX_AG] = {0};
void HierarchyMapping::AllocateMapInfo()
{
    // 注意Naive写法是针对没有split_AG的情况。每个AG只有一个对应的Core
    // 根据4_physical_crossbar_placement信息提供的Core上Crossbar的关系得到Core上AG的关系
    int crossbar_num = PIMCOM_2_virtual_crossbar.size();
    // （要得到AG的信息，遍历core_Crossbar）
    for (int i = 0; i < crossbar_num; ++i)
    {
        int AG_index = PIMCOM_2_virtual_crossbar[i].array_group_total;
        if (AG_flags[AG_index] != 1)
        {
            AG_flags[AG_index] = 1;
            int core_index = PIMCOM_2_virtual_crossbar[i].vcore;
            int rep_index = PIMCOM_2_virtual_crossbar[i].replication_index;
            int node_index = PIMCOM_2_virtual_crossbar[i].node_index;
            int AG_index_in_total = PIMCOM_2_virtual_crossbar[i].array_group_total;
            int AG_index_in_replication = PIMCOM_2_virtual_crossbar[i].array_group_in_weight;
            int AG_num_per_replication = PIMCOM_2_virtual_crossbar[i].AG_num_per_replication;
            int replication_index = PIMCOM_2_virtual_crossbar[i].replication_index;

            struct AG_info_schedule AGInfo;
            AGInfo.AG_index_in_total = AG_index_in_total;
            AGInfo.AG_index_in_replication = AG_index_in_replication;
            AGInfo.AG_num_per_replication = AG_num_per_replication;
            AGInfo.replication_index = rep_index;
            AGInfo.AGP = PIMCOM_node_list[node_index].AGP;
            AGInfo.agp_index = PIMCOM_2_virtual_crossbar[i].agp_index;
            AGInfo.agp_offset = PIMCOM_2_virtual_crossbar[i].agp_offset;
            AGInfo.replication_num = PIMCOM_node_list[node_index].replication_num;
            AGInfo.replication_num_origin = PIMCOM_node_list[node_index].replication_num_origin;
            AGInfo.input_cycle_in_total = PIMCOM_node_list[node_index].input_cycle_in_total;
            AGInfo.input_cycle_this_replication = PIMCOM_2_virtual_crossbar[i].input_cycle_this_replication;
            AGInfo.input_cycle_this_replication_start = PIMCOM_2_virtual_crossbar[i].input_cycle_this_replication_start;
            AGInfo.input_cycle_this_replication_end = PIMCOM_2_virtual_crossbar[i].input_cycle_this_replication_end;
            AGInfo.level_index = PIMCOM_node_list[node_index].level_index;

            int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
            int crossbar_num_AG = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list.size();
            int crossbar_start_index = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list[0];
            int crossbar_end_index = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list[crossbar_num_AG - 1];
            int input_element_num = PIMCOM_2_virtual_crossbar[crossbar_start_index].height_end - PIMCOM_2_virtual_crossbar[crossbar_start_index].height_start + 1;
            int output_element_num = PIMCOM_2_virtual_crossbar[crossbar_end_index].width_end - PIMCOM_2_virtual_crossbar[crossbar_start_index].width_start + 1;
            AGInfo.input_element_num = input_element_num;
            AGInfo.output_element_num = output_element_num;

            PIMCOM_4_virtual_core_AG_map.core_list[core_index].AG_list.push_back(AGInfo);
            PIMCOM_4_virtual_core_AG_map.core_list[core_index].node_list.push_back(node_index);
        }
    }

    //// 得到每个AG的instruction_group_num
    for (int i = 0; i < PIMCOM_2_AG_partition.size(); ++i)
    {
        int replication_num = PIMCOM_2_AG_partition[i].replication_num;
        for (int j = 0; j < replication_num; ++j)
        {
            PIMCOM_2_AG_partition[i].replication[j].instruction_group_num = static_cast<int>(ceil(float(PIMCOM_2_AG_partition[i].replication[j].input_cycle_this_replication) / float(operation_cycle_before_comm)));
            int AG_num = PIMCOM_2_AG_partition[i].replication[j].AG_list.size();
            for (int k = 0; k < AG_num; ++k)
            {
                int AG_index = PIMCOM_2_AG_partition[i].replication[j].AG_list[k].AG_index;
                PIMCOM_3_hierarchy_map.whole[AG_index][0].instruction_group_num = PIMCOM_2_AG_partition[i].replication[j].instruction_group_num;
                PIMCOM_4_AG_instruction_group_num[AG_index] = PIMCOM_2_AG_partition[i].replication[j].instruction_group_num;
            }
        }
    }
}


static float base_core_memory_usage[MAX_CORE] = {0};
static float base_core_memory_usage_recv[MAX_CORE] = {0};
static float base_core_memory_usage_input[MAX_CORE] = {0};
static float base_core_memory_usage_output[MAX_CORE] = {0};
static float base_node_memory_usage[MAX_NODE] = {0};
static float base_AG_memory_usage[MAX_AG] = {0};
static float base_AG_memory_usage_input[MAX_AG] = {0};
static float base_AG_memory_usage_output[MAX_AG] = {0};
void HierarchyMapping::ShowMemoryInfo()
{
    clock_t USE = 0;
    clock_t time1 = clock();
    for (int i = 0 ; i < PIMCOM_3_mapping_result.size(); i++)
    {
        float memory_usage = 0;
        int AG_num = PIMCOM_3_mapping_result[i].size();
        for (int j = 0; j < AG_num; ++j)
        {
            struct AGMapStruct thisAG = PIMCOM_3_mapping_result[i][j];
            int node_index = thisAG.node_index;
            int AG_index_in_total = thisAG.AG_index_in_total;
            int replication_index = thisAG.replication_index;
            int input_element_num = thisAG.input_element_num;
            int output_element_num = thisAG.output_element_num;
            std::string operation = PIMCOM_node_list[node_index].operation;
            int instruction_group_num_ori = thisAG.instruction_group_num;
            if (operation == "OP_CONV")
            {
                int instruction_group_num_eva;
                if (appointed_instruction_group_num == 0)
                {
                    instruction_group_num_eva = ceil(float(instruction_group_num_ori) / float(instruction_group_reload_num));
                }
                else
                {
                    instruction_group_num_eva = appointed_instruction_group_num < instruction_group_num_ori ? appointed_instruction_group_num : instruction_group_num_ori;
                }

                int input_window_element_num = ACC_input_window_element_num[node_index]; // input_channel_length * kernel_h * kernel_h
                int input_num = (instruction_group_num_eva * operation_cycle_before_comm);

                //// 按窗口读取
                if (j == 0 || PIMCOM_3_mapping_result[i][j-1].node_index != node_index || PIMCOM_3_mapping_result[i][j].replication_index != replication_index)
                {
                    //// 输入(目前还未优化。应该是需要优化。因为窗口之间会有重叠，而目前没有考虑这部分)
                    base_AG_memory_usage_input[AG_index_in_total] += input_num * input_window_element_num;
                    base_core_memory_usage_input[i] += input_num * input_window_element_num;
                    memory_usage += input_num * input_window_element_num;
                    //// 输出(每个rep只用加一次即可，然而这与AG无关，所以base_AG_memory_usage_input其实不同管)
                    base_core_memory_usage_output[i] += input_num * output_element_num;
                    memory_usage += input_num * output_element_num;
                    //// 对于输出的更激进的优化（这里的1是比较理想的情况，MVMUL算完的时候VADD已经算完，否则要比1更大）
                    base_core_memory_usage_output[i] += 1 * output_element_num;
                    memory_usage += 1 * output_element_num;
                }

                //// im2col开销
                base_AG_memory_usage_input[AG_index_in_total] += input_element_num;
                memory_usage += input_element_num;
                //// output开销
//                base_AG_memory_usage_output[AG_index_in_total] += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
//                base_core_memory_usage_output[i] += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
//                memory_usage += operation_cycle_before_comm * instruction_group_num_eva * Instruction.output_element_num;
                //// output开销（考虑空间复用）（如果采用更激进的情况，则不需要这部分，改为上面循环内部的写法）
//                base_AG_memory_usage_output[AG_index_in_total] += Instruction.output_element_num;
//                base_core_memory_usage_output[i] += Instruction.output_element_num;
//                memory_usage += Instruction.output_element_num;
            }
            else
            {
                base_AG_memory_usage_input[AG_index_in_total] += input_element_num;
                base_core_memory_usage_input[i] += input_element_num;
                base_AG_memory_usage_output[AG_index_in_total] += output_element_num;
                base_core_memory_usage_output[i] += output_element_num;
                memory_usage += input_element_num;
                memory_usage += output_element_num;
            }
        }
        base_core_memory_usage[i] = memory_usage;
    }

    std::cout << "============ core memory statistic ============" << std::endl;
    float core_memory_sum = 0.0;
    for (int i = 0; i != PIMCOM_3_mapping_result.size(); i++)
    {
        std::cout << "core" << i << "  " << base_core_memory_usage[i] * 16 / 8 / 1024 << "KB" << std::endl;
        std::cout << "    input ratio:" << base_core_memory_usage_input[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        std::cout << "    output ratio:" << base_core_memory_usage_output[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        std::cout << "    recv ratio:" << base_core_memory_usage_recv[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        core_memory_sum += base_core_memory_usage[i];
    }
    std::cout << "sum:" << core_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;

    clock_t time2 = clock();
    USE += time2 - time1;
    std::cout << "memory:" << double(USE)/CLOCKS_PER_SEC << "s" << std::endl;
}

void HierarchyMapping::SaveMemoryInfo()
{
    std::ofstream MemoryFile("../memory.txt", std::ios::out | std::ios::trunc);
    MemoryFile << "============ core memory statistic ============" << std::endl;
    float core_memory_sum = 0.0;
    for (int i = 0; i < PIMCOM_3_mapping_result.size(); i++)
    {
        MemoryFile << "core" << i << "  " << base_core_memory_usage[i] * 16 / 8 / 1024 << "KB" << std::endl;
        MemoryFile << "    input ratio:" << base_core_memory_usage_input[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        MemoryFile << "    output ratio:" << base_core_memory_usage_output[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        MemoryFile << "    recv ratio:" << base_core_memory_usage_recv[i]/base_core_memory_usage[i] * 100 << "%" << std::endl;
        core_memory_sum += base_core_memory_usage[i];
    }
    MemoryFile << "sum:" << core_memory_sum * 16 / 8 / 1024 << "KB" << std::endl;
    MemoryFile.close();
}


void HierarchyMapping::ShowOriginalInfo()
{
    int v = 0;
    int node_num_partition = PIMCOM_2_AG_partition.size();
    for (int i = 0; i < node_num_partition; ++i)
    {
        std::cout << "Node Index: " << PIMCOM_2_AG_partition[i].index << ", Operation: " << PIMCOM_2_AG_partition[i].operation << std::endl;
        int replication_num = PIMCOM_2_AG_partition[i].replication_num;
        for (int j = 0; j < replication_num; ++j)
        {
            std::cout << "|---Replication Index: " << j << std::endl;
            int ag_num_current_replication = PIMCOM_2_AG_partition[i].replication[j].AG_list.size();
            for (int k = 0; k < ag_num_current_replication; ++k)
            {
                std::cout << "|-------Array Group Index: " << v << ", Crossbar Number: "
                          << PIMCOM_2_AG_partition[i].replication[j].AG_list[k].virtual_crossbar_list.size() << std::endl;
                v ++;
            }
        }
    }
}


void HierarchyMapping::ShowMappingInfo()
{
    //// 打印映射信息
    std::cout << "****************** Mapping Result ********************" << std::endl;
    for (int i = 0; i < core_num; ++i)
    {
        std::cout << i << std::endl;
        std::cout << " " << "node"
                  << "  " << "r_index"
                  << "  "  << "index_r"
                  << "  | " <<  "AG_index"
                  << " " << "input_cycle"
                  << " " << "IG_num"  << std::endl;
        int AG_num = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list.size();
        for (int j = 0; j < AG_num; ++j)
        {
            std::cout << "    " << std::setw(3) << PIMCOM_4_virtual_core_AG_map.core_list[i].node_list[j]
                      << "    " << std::setw(3) << PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].replication_index
                      << "    " <<  std::setw(3) << PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].AG_index_in_replication
                      << "    | " << std::setw(5) << PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].AG_index_in_total
                      << "    " << std::setw(5) << PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].input_cycle_this_replication
                      << "    " << std::setw(5) << ceil(float(PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].input_cycle_this_replication)/float(operation_cycle_before_comm))  << std::endl;
        }
    }
}


void HierarchyMapping::ShowMappingInfoDistributed()
{
    //// 打印映射信息
    std::cout << "****************** Mapping Result ********************" << std::endl;
    for (int i = 0; i < ChipH * ChipW; ++i)
    {
        std::cout << i << std::endl;
        std::cout << " " << "node"
                  << "  " << "r_index"
                  << "  "  << "index_r"
//                  << "  | " <<  "AG_index"
                  << " " << "input_cycle"
                  << " " << "IG_num"  << std::endl;
        int AG_num = PIMCOM_3_mapping_result[i].size();
        for (int j = 0; j < AG_num; ++j)
        {
            std::cout << "    " << std::setw(3) << PIMCOM_3_mapping_result[i][j].node_index
                      << "    " << std::setw(3) << PIMCOM_3_mapping_result[i][j].replication_index
                      << "    " <<  std::setw(3) << PIMCOM_3_mapping_result[i][j].index_in_replication
//                      << "    | " << std::setw(5) << PIMCOM_3_mapping_result[i][j].AG_index_in_total
                      << "    " << std::setw(5) << PIMCOM_3_mapping_result[i][j].input_cycle_num
                      << "    " << std::setw(5) << PIMCOM_3_mapping_result[i][j].instruction_group_num  << std::endl;
        }
    }
}

void HierarchyMapping::ShowMVMULInfo()
{
    if (MapMode == Exploration)
        return;
    std::vector<int> MVMULnum;
    MVMULnum.resize(core_num);
    for (int i = 0; i < core_num; ++i)
    {
        int AG_num_in_core = PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list.size();
        for (int j = 0; j < AG_num_in_core; ++j)
        {
            MVMULnum[i] += PIMCOM_4_virtual_core_AG_map.core_list[i].AG_list[j].input_cycle_this_replication;
        }
        std::cout << MVMULnum[i]  << " ";
    }
    std::cout << std::endl;
}


void HierarchyMapping::Check()
{
    // 统计crossbar的数量，确保每个crossbar都映射到
    int FLAG[ChipH * ChipW * CoreH * CoreW] = {0};
    for (int i = 0; i < core_num; ++i)
    {
        int rram_num = PIMCOM_3_virtual_core_crossbar_map[i].size();
        for (int j = 0; j < rram_num; ++j)
        {
            int index = PIMCOM_3_virtual_core_crossbar_map[i][j];
            FLAG[index] = 1;
        }
    }
    int sum = 0;
    for (int i = 0; i < ChipH * ChipW * CoreH * CoreW; ++i)
    {
        sum += FLAG[i];
    }
    if (sum == PIMCOM_2_resource_info.RRAMS)
        std::cout << "map check pass!" << std::endl;
}


void HierarchyMapping::Clear()
{
    ArrayGroupIndex = 0;
    for (int & n : AG_flags) {n = 0;}

    for (int i = 0; i < ChipW * ChipH; ++i)
    {
        ResourceList[i] = CoreW * CoreH;
    }
    for (float & n : base_core_memory_usage) {n = 0;}
    for (float & n : base_core_memory_usage_recv) {n = 0;}
    for (float & n : base_core_memory_usage_input) {n = 0;}
    for (float & n : base_core_memory_usage_output) {n = 0;}
    for (float & n : base_node_memory_usage) {n = 0;}
    for (float & n : base_AG_memory_usage) {n = 0;}
    for (float & n : base_AG_memory_usage_input) {n = 0;}
    for (float & n : base_AG_memory_usage_output) {n = 0;}

    ACC_input_window_element_num.clear();
    ACC_input_element_num_from_AG_index_in_replication.clear();
}


//void HierarchyMapping::SaveJsonIR(Json::Value & DNNInfo, std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/3_hm.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}