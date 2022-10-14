//
// Created by SXT on 2022/9/30.
//

#include "InferencePipelineDesign.h"

void InferencePipelineDesign::DesignPipeline()
{
    node_num = PIMCOM_node_list.size();
    // 下面这一行是为了inception优化过的分层函数。运行别的网络时可以不加。
    GetConcatMaxLevelForInception();
    ClassifyTheNode(0, 0, 0);
    GetAugmentedNodeList();
    RefineAugmentedNodeList(0, 0, 0, 0, 0);
    Clear();
//    ShowClassificationInfo(DNNInfo);
}


static int concat_rest_num[MAX_AG] = {0};
static int concat_max_level[MAX_AG] = {0};
void InferencePipelineDesign::GetConcatMaxLevelForInception()
{
    for (int i = 0; i < node_num; ++i)
    {
        if (PIMCOM_node_list[i].operation == "OP_CONCAT")
        {
            concat_rest_num[i] = PIMCOM_node_list[i].provider_num;
        }
    }
}

void InferencePipelineDesign::ClassifyTheNode(int node_index, int level_index, int index_in_level)
{
    if (node_index == 0)
    {
        PIMCOM_node_list[node_index].level_index = level_index;
        PIMCOM_node_list[node_index].index_in_level = index_in_level;
    }
    int previous_level = PIMCOM_node_list[node_index].level_index;
    if (previous_level < level_index)
    {
        PIMCOM_node_list[node_index].level_index = level_index;
        PIMCOM_node_list[node_index].index_in_level = index_in_level;
    }
    if (PIMCOM_node_list[node_index].consumer_num == 0)
    {
        return;
    }
    else
    {
        int consumer_num = PIMCOM_node_list[node_index].consumer_num;
        for (int i = 0; i < consumer_num; ++i)
        {
            int consumer_index = PIMCOM_node_list[node_index].consumer_index[i];
            std::string consumer_op = PIMCOM_node_list[consumer_index].operation;
            if ( consumer_op == "OP_CONV" || consumer_op == "OP_FC")
            {
                ClassifyTheNode(consumer_index, level_index+1, 0);
            }
                // 对于inception结构进行优化
            else if (consumer_op == "OP_CONCAT")
            {
                if (level_index > concat_max_level[consumer_index])
                    concat_max_level[consumer_index] = level_index;
                if (concat_rest_num[consumer_index] != 1)
                {
                    concat_rest_num[consumer_index]--;
                }
                else
                    ClassifyTheNode(consumer_index, concat_max_level[consumer_index], index_in_level+1);
            }
            else
                ClassifyTheNode(consumer_index, level_index, index_in_level+1);
        }
        return;
    }
}

static int node_visited[MAX_NODE] = {0};
void InferencePipelineDesign::GetAugmentedNodeList()
{
    // 根据4_physical_crossbar_placement信息，添加CONV层或FC层的AG0_core_index和AG0_index_in_total的信息
    int crossbar_num = PIMCOM_2_virtual_crossbar.size();
    for (int i = 0; i < crossbar_num; ++i)
    {
        int node_index = PIMCOM_2_virtual_crossbar[i].node_index;
        int AG_index_in_replication = PIMCOM_2_virtual_crossbar[i].array_group_in_weight;
        if (AG_index_in_replication == 0 && node_visited[node_index] != 1)
        {
            node_visited[node_index] = 1;
            int core_index = PIMCOM_2_virtual_crossbar[i].vcore;
            int AG_index_in_total = PIMCOM_2_virtual_crossbar[i].array_group_total;
            PIMCOM_node_list[node_index].AG0_core_index = core_index;
            PIMCOM_node_list[node_index].AG0_index_in_total = AG_index_in_total;
            PIMCOM_node_list[node_index].AG0_node_index = node_index;
        }
    }

    // 找到那些特殊的后处理节点（需要加大offset的）
    //   首先初始化
    for (int i = 0; i < node_num; ++i)
    {
        PIMCOM_node_list[i].copy_offset_flag = 0;
    }

    //   主要函数
    for (int i = 0; i < node_num; ++i)
    {
        int level_index = PIMCOM_node_list[i].level_index;
        int consumer_num = PIMCOM_node_list[i].consumer_num;
        int equal_to_level_num = 0;
        // 先得到equal_to_level_num指标
        for (int j = 0; j < consumer_num; ++j)
        {
            int consumer_index = PIMCOM_node_list[i].consumer_index[j];
            int consumer_level = PIMCOM_node_list[consumer_index].level_index;
            if (consumer_level == level_index)
                equal_to_level_num ++;
        }
        if (equal_to_level_num != 0 && equal_to_level_num < consumer_num)
        {
            int copy_offset = 1;
            for (int j = 0; j < consumer_num; ++j)
            {
                int consumer_index = PIMCOM_node_list[i].consumer_index[j];
                int consumer_level = PIMCOM_node_list[consumer_index].level_index;
                if (consumer_level == level_index)
                {
                    // 注意这里的意思是，第consumer_index的信息中增加了copy_offset字段，假设第x个值是y，说明index为x的节点是该结点的生产者，并且在该生产者看来，这是第y个需要offset的数据。
                    PIMCOM_node_list[consumer_index].copy_offset_flag = 1;
                    // TODO:这里未考虑
//                    NodeList[consumer_index]["tmp_data"]["copy_offset"]["provider_index"][i] = copy_offset;
                    copy_offset += 1;
                }
            }
        }
    }
}

static int visit_refine_node_list[MAX_NODE] = {0};
void InferencePipelineDesign::RefineAugmentedNodeList(int node_index, int level_index, int AG0_core_index, int AG0_index_in_total, int AG0_node_index)
{
    // GetAugmentedNodeList只得到了CONV和FC的AG0_core_index和AG0_index_in_total，
    // RefineAugmentedNodeList的目的是得到那些后处理节点的AG0_core_index和AG0_index_in_total
    // 如果不先得到这些后处理节点的信息，而是在5_2中一边生成信息一边生成指令，就会出现某些节点的AG0信息还没获取到就先被使用的情况。不可以。
    int consumer_num = PIMCOM_node_list[node_index].consumer_num;
    if (consumer_num == 0)
    {
        return;
    }
    else
    {
        for (int i = 0; i < consumer_num; ++i)
        {
            int consumer_index = PIMCOM_node_list[node_index].consumer_index[i];
            int consumer_level = PIMCOM_node_list[consumer_index].level_index;
            std::string consumer_op = PIMCOM_node_list[consumer_index].operation;
            if( level_index != consumer_level)
            {
                if (consumer_op == "OP_CONV" || consumer_op == "OP_FC")
                {
                    int consumer_AG0_core_index = PIMCOM_node_list[consumer_index].AG0_core_index;
                    int consumer_AG0_index_in_total = PIMCOM_node_list[consumer_index].AG0_index_in_total;
                    int consumer_AG0_node_index = PIMCOM_node_list[consumer_index].AG0_node_index;
                    RefineAugmentedNodeList( consumer_index, consumer_level, consumer_AG0_core_index, consumer_AG0_index_in_total, consumer_AG0_node_index);
                }
            }
            else
            {
                if (visit_refine_node_list[consumer_index] == 0) // 这一句是为了解决一个node会有多个同level的生产者，这样每个该level的生产者都会处理一下该node，造成重复。这里设置的意义是只运行一次后处理即可。
                {
                    visit_refine_node_list[consumer_index] = 1;
                    PIMCOM_node_list[consumer_index].AG0_core_index = AG0_core_index;
                    PIMCOM_node_list[consumer_index].AG0_index_in_total = AG0_index_in_total;
                    PIMCOM_node_list[consumer_index].AG0_node_index = AG0_node_index;
                    RefineAugmentedNodeList(consumer_index, level_index, AG0_core_index, AG0_index_in_total, AG0_node_index);
                }
            }
        }
    }
}



void InferencePipelineDesign::Clear()
{
    for (int i = 0; i < MAX_AG; ++i) concat_rest_num[i] = 0;
    for (int i = 0; i < MAX_AG; ++i) concat_max_level[i] = 0;
    for (int i = 0; i < MAX_NODE; ++i) node_visited[i] = 0;
    for (int i = 0; i < MAX_NODE; ++i) visit_refine_node_list[i] = 0;
}

void InferencePipelineDesign::ShowClassificationInfo()
{
    std::cout << "****************** Classification Result ********************" << std::endl;
    for (int i = 0; i < node_num; ++i)
    {
        std::cout.setf(std::ios::left); //设置对齐方式为left
        std::cout.width(40); //设置宽度，不足用空格填充
        std::cout << PIMCOM_node_list[i].name;
        std::cout << "    " << PIMCOM_node_list[i].level_index << "    " << PIMCOM_node_list[i].index_in_level << std::endl;
    }
}

//void InferencePipelineDesign::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/5_pd.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}