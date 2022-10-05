//
// Created by SXT on 2022/9/30.
//

#include "InferencePipelineDesign.h"

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;
extern std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
extern struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
extern std::vector<int> PIMCOM_2_effective_node;
extern struct PIMCOM_3_hierarchy_map PIMCOM_3_hierarchy_map;
extern std::map<int, std::vector<int>> PIMCOM_3_virtual_core_crossbar_map;
extern std::map<int,int> PIMCOM_4_physical_core_placement;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_4_physical_crossbar_placement;

//std::map<int, struct PIMCOM_5_pool_info> PIMCOM_5_pool_info;
std::vector<struct PIMCOM_5_pool_info> PIMCOM_5_pool_info;

void InferencePipelineDesign::DesignPipeline()
{
    node_num = PIMCOM_node_list.size();
    // 下面这一行是为了inception优化过的分层函数。运行别的网络时可以不加。
    GetConcatMaxLevelForInception();
    ClassifyTheNode(0, 0, 0);
    GetAugmentedNodeList();
    RefineAugmentedNodeList(0, 0, 0, 0, 0);
    GetPoolInfo();
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
    int crossbar_num = PIMCOM_4_physical_crossbar_placement.size();
    for (int i = 0; i < crossbar_num; ++i)
    {
        int node_index = PIMCOM_4_physical_crossbar_placement[i].node_index;
        int AG_index_in_replication = PIMCOM_4_physical_crossbar_placement[i].array_group_in_weight;
        if (AG_index_in_replication == 0 && node_visited[node_index] != 1)
        {
            node_visited[node_index] = 1;
            int core_index = PIMCOM_4_physical_crossbar_placement[i].physical_core;
            int AG_index_in_total = PIMCOM_4_physical_crossbar_placement[i].array_group_total;
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
    // 5_1的目的很单纯，就是得到那些后处理节点的AG0_core_index和AG0_index_in_total。是5_2的准备阶段。
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

void InferencePipelineDesign::GetPoolInfo()
{
    PIMCOM_5_pool_info.resize(node_num);
    for (int n = 0; n < node_num; ++n)
    {
        struct PIMCOM_node Node = PIMCOM_node_list[n];
        if (Node.operation != "OP_POOL")
            continue;

        struct param Params = Node.param;
        int input_H = Node.input_dim[2];
        int input_W = Node.input_dim[3];
        int pool_kernel_w = Params.kernel_w;
        int pool_kernel_h = Params.kernel_h;
        int pool_padding_h0 = Params.pad_h0;
        int pool_padding_h1 = Params.pad_h1;
        int pool_padding_w0 = Params.pad_w0;
        int pool_padding_w1 = Params.pad_w1;
        int pool_stride_w = Params.stride_w;
        int pool_stride_h = Params.stride_h;

        int output_W = floor(float(input_W + pool_padding_w0 + pool_padding_w1 - pool_kernel_w) / float(pool_stride_w)) + 1;
        int output_H = floor(float(input_H + pool_padding_h0 + pool_padding_h1 - pool_kernel_h) / float(pool_stride_h)) + 1;
        int info_output_W = Node.output_dim[3];
        int info_output_H = Node.output_dim[2];
        if (info_output_W != output_W || info_output_H != output_H)
        {
            std::cout << " Output Size Doesn't Match" << std::endl;
            return;
        }
        PIMCOM_5_pool_info[n].input_index.resize(Node.input_dim[2] * Node.input_dim[3]);
        PIMCOM_5_pool_info[n].output_index.resize(info_output_W * info_output_H);
        int output_index = 0;
        for (int i = 0; i < output_H; ++i)
        {
            for (int j = 0; j < output_W; ++j)
            {
                int start_address = i * pool_stride_h * input_W + j *  pool_stride_w;
                if (i != 0)
                    start_address -= pool_padding_h0 * input_W;
                if (j != 0)
                    start_address -= pool_padding_w0;
                int start_row = start_address / input_W;
                int start_col = start_address % input_W;

                int pool_h_num = pool_kernel_h;
                if (i == 0)
                    pool_h_num -= pool_padding_h0;
                else if (i == output_H-1)
                    if (start_row + pool_kernel_h > input_H)
                        pool_h_num -= pool_padding_h1;

                int pool_w_num = pool_kernel_w;
                if (j == 0)
                    pool_w_num -= pool_padding_w0;
                else if (j == output_W-1)
                    if (start_col + pool_kernel_w > input_W)
                        pool_w_num -= pool_padding_w1;

                for (int h = 0; h < pool_h_num ; ++h)
                {
                    for (int w = 0; w < pool_w_num; ++w)
                    {
                        int position = start_address + w + h * input_W;
                        PIMCOM_5_pool_info[n].input_index[position].push_back(output_index);
                        PIMCOM_5_pool_info[n].output_index[output_index].push_back(position);
                    }
                }
                output_index += 1;
            }
        }
    }
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