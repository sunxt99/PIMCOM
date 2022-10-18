//
// Created by SXT on 2022/9/16.
//

#include "ElementPipelineDesign.h"

void ElementPipelineDesign::DesignPipeline()
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
void ElementPipelineDesign::GetConcatMaxLevelForInception()
{
    for (int i = 0; i < node_num; ++i)
    {
        if (PIMCOM_node_list[i].operation == "OP_CONCAT")
        {
            concat_rest_num[i] = PIMCOM_node_list[i].provider_num;
        }
    }
}

void ElementPipelineDesign::ClassifyTheNode(int node_index, int level_index, int index_in_level)
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
void ElementPipelineDesign::GetAugmentedNodeList()
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

void ElementPipelineDesign::RefineAugmentedNodeList(int node_index, int level_index, int AG0_core_index, int AG0_index_in_total, int AG0_node_index)
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



void ElementPipelineDesign::Clear()
{
    for (int i = 0; i < MAX_AG; ++i) concat_rest_num[i] = 0;
    for (int i = 0; i < MAX_AG; ++i) concat_max_level[i] = 0;
    for (int i = 0; i < MAX_NODE; ++i) node_visited[i] = 0;
    for (int i = 0; i < MAX_NODE; ++i) visit_refine_node_list[i] = 0;
}

void ElementPipelineDesign::ShowClassificationInfo()
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







//void ElementPipelineDesign::ShowWaitToActInfo(Json::Value &DNNInfo)
//{
//    NodeList = DNNInfo["node_list"];
//    node_num = static_cast<int>(NodeList.size());
//
//    for (int n = 0; n < node_num; ++n)
//    {
//        Json::Value Node = NodeList[n];
//        if (strcmp(Node["operation"].asCString(), "OP_CONV") != 0)
//            continue;
//        Json::Value Params = Node["param"];
//        int input_h = Node["input_dim"][2].asInt();
//        int input_w = Node["input_dim"][3].asInt();
//        int input_c = Node["input_dim"][1].asInt();
//        int kernel_w = Params["kernel_w"].asInt();
//        int kernel_h = Params["kernel_h"].asInt();
//        int padding_h0 = Params["pad_h0"].asInt();
//        int padding_h1 = Params["pad_h1"].asInt();
//        int padding_w0 = Params["pad_w0"].asInt();
//        int padding_w1 = Params["pad_w1"].asInt();
//        int stride_w = Params["stride_w"].asInt();
//        int stride_h = Params["stride_h"].asInt();
//
//        int output_w = floor(float(input_w + padding_w0 + padding_w1 - kernel_w) / float(stride_w)) + 1;
//        int output_h = floor(float(input_h + padding_h0 + padding_h1 - kernel_h) / float(stride_h)) + 1;
//        int info_output_w = Node["output_dim"][3].asInt();
//        int info_output_h = Node["output_dim"][2].asInt();
//        if (info_output_w != output_w || info_output_h != output_h)
//        {
//            std::cout << " Output Size Doesn't Match" << std::endl;
//            return;
//        }
//        std::cout << "input: " << input_w << " kernel:" << kernel_w << " stride:" << stride_w << " padding:" << padding_w0 << std::endl;
//        std::cout << "  " << 100* float(input_w * input_h - (input_h - kernel_h + padding_h0+ 1) * (input_w - kernel_w + padding_w0 + 1 )) / float (input_h * input_w) << "%" << std::endl;
//        int rest_input_w = input_w - (kernel_w - padding_w0 - 1);
//        int rest_input_h = input_h - (kernel_h - padding_h0 - 1);
//        int effective_input_w = (kernel_w - padding_w0 - 1);
//        int effective_input_h = (kernel_h - padding_h0 - 1);
//        int blank_element_w = rest_input_w - output_w;
//        int blank_element_h = rest_input_h - output_h;
//        if (blank_element_w < 0)  // stride = 1 and padding_w1 > 0
//        {
//            effective_input_w += rest_input_w;
//            output_w = rest_input_w;
//        }
//        else if (blank_element_w <= output_w - 1)
//            effective_input_w += rest_input_w;
//        else
//            effective_input_w += output_w + (output_w-1) * (stride_w-1);
//        if (blank_element_h < 0) // stride = 1 and padding_h1 > 0
//        {
//            effective_input_h += rest_input_h;
//            output_h = rest_input_h;
//        }
//        else if (blank_element_h <= output_h - 1)
//            effective_input_h += rest_input_h;
//        else
//            effective_input_h += output_h + (output_h-1) * (stride_h-1);
//
//        std::cout << "  " << effective_input_h << "  " << effective_input_w << "  " << output_h << "  " << output_w << std::endl;
//        std::cout << "  " << 100 * float(effective_input_w * effective_input_h - (output_w * output_h))/ float(effective_input_w * effective_input_h) << "%" << std::endl;
//    }
//}