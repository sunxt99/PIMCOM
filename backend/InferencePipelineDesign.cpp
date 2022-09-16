//
// Created by SXT on 2022/9/16.
//

#include "InferencePipelineDesign.h"


void InferencePipelineDesign::DesignPipeline(Json::Value & DNNInfo)
{
    NodeList = DNNInfo["node_list"];
    node_num = static_cast<int>(NodeList.size());
    // 下面这一行是为了inception优化过的分层函数。运行别的网络时可以不加。
    GetConcatMaxLevelForInception();
    ClassifyTheNode(0, 0, 0);
    GetAugmentedNodeList(DNNInfo);
    RefineAugmentedNodeList(DNNInfo, 0, 0, 0, 0, 0);
    GetPoolInfo(DNNInfo);
    DNNInfo["5_node_list_augmented"] = NodeList;
}


static int concat_rest_num[MAX_AG] = {0};
static int concat_max_level[MAX_AG] = {0};
void InferencePipelineDesign::GetConcatMaxLevelForInception()
{
    for (int i = 0; i < node_num; ++i)
    {
        Json::Value Node = NodeList[i];
        if (strcmp(Node["operation"].asCString(), "OP_CONCAT") == 0)
        {
            concat_rest_num[i] = 4;
        }
    }
}

void InferencePipelineDesign::ClassifyTheNode(int node_index, int level_index, int index_in_level)
{
    if (node_index == 0)
    {
        NodeList[node_index]["level_index"] = level_index;
        NodeList[node_index]["index_in_level"] = index_in_level;
    }
    int previous_level = NodeList[node_index]["level_index"].asInt();
    if (previous_level < level_index)
    {
        NodeList[node_index]["level_index"] = level_index;
        NodeList[node_index]["index_in_level"] = index_in_level;
    }
    if (NodeList[node_index]["consumer_num"].asInt() == 0)
    {
        return;
    }
    else
    {
        int consumer_num = NodeList[node_index]["consumer_num"].asInt();
        for (int i = 0; i < consumer_num; ++i)
        {
            int consumer_index = NodeList[node_index]["consumer_index"][i].asInt();
            std::string consumer_op = NodeList[consumer_index]["operation"].asCString();
            if (strcmp(consumer_op.c_str(), "OP_CONV") == 0 || strcmp(consumer_op.c_str(), "OP_FC") == 0)
            {
                ClassifyTheNode(consumer_index, level_index+1, 0);
            }
                // 对于inception结构进行优化
            else if (strcmp(consumer_op.c_str(), "OP_CONCAT") == 0)
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
void InferencePipelineDesign::GetAugmentedNodeList(Json::Value &DNNInfo)
{
    // 根据4_physical_crossbar_placement信息，添加CONV层或FC层的AG0_core_index和AG0_index_in_total的信息
    int crossbar_num = DNNInfo["4_physical_crossbar_placement"].size();
    for (int i = 0; i < crossbar_num; ++i)
    {
        int node_index = DNNInfo["4_physical_crossbar_placement"][i]["node_index"].asInt();
        int AG_index_in_replication = DNNInfo["4_physical_crossbar_placement"][i]["array_group_in_weight"].asInt();
        if (AG_index_in_replication == 0 && node_visited[node_index] != 1)
        {
            node_visited[node_index] = 1;
            int core_index = DNNInfo["4_physical_crossbar_placement"][i]["physical_core"].asInt();
            int AG_index_in_total = DNNInfo["4_physical_crossbar_placement"][i]["array_group_total"].asInt();
            NodeList[node_index]["AG0_core_index"] = core_index;
            NodeList[node_index]["AG0_index_in_total"] = AG_index_in_total;
            NodeList[node_index]["AG0_node_index"] = node_index;
        }
    }

    // 找到那些特殊的后处理节点（需要加大offset的）
    //   首先初始化
    for (int i = 0; i < node_num; ++i)
    {
        NodeList[i]["copy_offset_flag"] = 0;
    }
    //   主要函数
    for (int i = 0; i < node_num; ++i)
    {
        Json::Value Node = NodeList[i];
        int level_index = Node["level_index"].asInt();
        int consumer_num = Node["consumer_num"].asInt();
        int equal_to_level_num = 0;
        // 先得到equal_to_level_num指标
        for (int j = 0; j < consumer_num; ++j)
        {
            int consumer_index = Node["consumer_index"][j].asInt();
            int consumer_level = NodeList[consumer_index]["level_index"].asInt();
            if (consumer_level == level_index)
                equal_to_level_num ++;
        }
        if (equal_to_level_num != 0 && equal_to_level_num < consumer_num)
        {
            int copy_offset = 1;
            for (int j = 0; j < consumer_num; ++j)
            {
                int consumer_index = Node["consumer_index"][j].asInt();
                int consumer_level = NodeList[consumer_index]["level_index"].asInt();
                if (consumer_level == level_index)
                {
                    // 注意这里的意思是，第consumer_index的信息中增加了copy_offset字段，假设第x个值是y，说明index为x的节点是该结点的生产者，并且在该生产者看来，这是第y个需要offset的数据。
                    NodeList[consumer_index]["copy_offset_flag"] = 1;
                    NodeList[consumer_index]["tmp_data"]["copy_offset"]["provider_index"][i] = copy_offset;
                    copy_offset += 1;
                }
            }
        }
    }
}

static int visit_refine_node_list[MAX_NODE] = {0};
void InferencePipelineDesign::RefineAugmentedNodeList(Json::Value &DNNInfo, int node_index, int level_index, int AG0_core_index, int AG0_index_in_total, int AG0_node_index)
{
    // 5_1的目的很单纯，就是得到那些后处理节点的AG0_core_index和AG0_index_in_total。是5_2的准备阶段。
    // 如果不先得到这些后处理节点的信息，而是在5_2中一边生成信息一边生成指令，就会出现某些节点的AG0信息还没获取到就先被使用的情况。不可以。

    // 这里的NodeList都是pipeline design中得到的Augmented NodeList
    int consumer_num = NodeList[node_index]["consumer_num"].asInt();
    if (consumer_num == 0)
    {
        return;
    }
    else
    {
        for (int i = 0; i < consumer_num; ++i)
        {
            int consumer_index = NodeList[node_index]["consumer_index"][i].asInt();
            int consumer_level = NodeList[consumer_index]["level_index"].asInt();
            std::string consumer_op = NodeList[consumer_index]["operation"].asCString();
            if( level_index != consumer_level)
            {
                if (strcmp(consumer_op.c_str(), "OP_CONV") == 0 || strcmp(consumer_op.c_str(), "OP_FC") == 0)
                {
                    int consumer_AG0_core_index = NodeList[consumer_index]["AG0_core_index"].asInt();
                    int consumer_AG0_index_in_total = NodeList[consumer_index]["AG0_index_in_total"].asInt();
                    int consumer_AG0_node_index = NodeList[consumer_index]["AG0_node_index"].asInt();
                    RefineAugmentedNodeList(DNNInfo,  consumer_index, consumer_level, consumer_AG0_core_index, consumer_AG0_index_in_total, consumer_AG0_node_index);
                }
            }
            else
            {
                if (visit_refine_node_list[consumer_index] == 0) // 这一句是为了解决一个node会有多个同level的生产者，这样每个该level的生产者都会处理一下该node，造成重复。这里设置的意义是只运行一次后处理即可。
                {
                    visit_refine_node_list[consumer_index] = 1;
                    NodeList[consumer_index]["AG0_core_index"] = AG0_core_index;
                    NodeList[consumer_index]["AG0_index_in_total"] = AG0_index_in_total;
                    NodeList[consumer_index]["AG0_node_index"] = AG0_node_index;
                    RefineAugmentedNodeList(DNNInfo, consumer_index, level_index, AG0_core_index, AG0_index_in_total, AG0_node_index);
                }
            }
        }
    }
}

//void InferencePipelineDesign::GetPoolInfo(Json::Value &DNInfo)
//{
//    int input_W = 6;
//    int input_H = 6;
//    int pool_kernel_w = 2;
//    int pool_kernel_h = 2;
//    int pool_padding_h0 = 0;
//    int pool_padding_h1 = 0;
//    int pool_padding_w0 = 0;
//    int pool_padding_w1 = 0;
//    int pool_stride_w = 2;
//    int pool_stride_h = 2;
//    int output_W = floor(float(input_W + pool_padding_w0 + pool_padding_w1 - pool_kernel_w) / float(pool_stride_w)) + 1;
//    int output_H = floor(float(input_H + pool_padding_h0 + pool_padding_h1 - pool_kernel_h) / float(pool_stride_h)) + 1;
//    int output_index = 0;
//    for (int i = 0; i < output_H; ++i)
//    {
//        for (int j = 0; j < output_W; ++j)
//        {
//            std::cout << output_index << std::endl;
//            int start_address = i * pool_stride_h * input_W + j *  pool_stride_w;
//            if (i != 0)
//                start_address -= pool_padding_h0 * input_W;
//            if (j != 0)
//                start_address -= pool_padding_w0;
//            int start_row = start_address / input_W;
//            int start_col = start_address % input_W;
//
//            int pool_h_num = pool_kernel_h;
//            if (i == 0)
//                pool_h_num -= pool_padding_h0;
//            else if (i == output_H-1)
//                if (start_row + pool_kernel_h > input_H)
//                    pool_h_num -= pool_padding_h1;
//
//            int pool_w_num = pool_kernel_w;
//            if (j == 0)
//                pool_w_num -= pool_padding_w0;
//            else if (j == output_W-1)
//                if (start_col + pool_kernel_w > input_W)
//                    pool_w_num -= pool_padding_w1;
//
//            for (int h = 0; h < pool_h_num ; ++h)
//            {
//                for (int w = 0; w < pool_w_num; ++w)
//                {
//                    int position = start_address + w + h * input_W;
//                    std::cout << "  " << position ;
//                }
//            }
//            output_index += 1;
//            std::cout << std::endl;
//        }
//    }
//}

void InferencePipelineDesign::GetPoolInfo(Json::Value &DNNInfo)
{
    for (int n = 0; n < node_num; ++n)
    {
        Json::Value Node = NodeList[n];
        if (strcmp(Node["operation"].asCString(), "OP_POOL") != 0)
            continue;
        Json::Value Params = Node["param"];
        int input_H = Node["input_dim"][2].asInt();
        int input_W = Node["input_dim"][3].asInt();
        int pool_kernel_w = Params["kernel_w"].asInt();
        int pool_kernel_h = Params["kernel_h"].asInt();
        int pool_padding_h0 = Params["pad_h0"].asInt();
        int pool_padding_h1 = Params["pad_h1"].asInt();
        int pool_padding_w0 = Params["pad_w0"].asInt();
        int pool_padding_w1 = Params["pad_w1"].asInt();
        int pool_stride_w = Params["stride_w"].asInt();
        int pool_stride_h = Params["stride_h"].asInt();

        int output_W = floor(float(input_W + pool_padding_w0 + pool_padding_w1 - pool_kernel_w) / float(pool_stride_w)) + 1;
        int output_H = floor(float(input_H + pool_padding_h0 + pool_padding_h1 - pool_kernel_h) / float(pool_stride_h)) + 1;
        int info_output_W = Node["output_dim"][3].asInt();
        int info_output_H = Node["output_dim"][2].asInt();
        if (info_output_W != output_W || info_output_H != output_H)
        {
            std::cout << " Output Size Doesn't Match" << std::endl;
            return;
        }
        NodeList[n]["pool_info"]["input_index"].resize(output_W * output_H);
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
                        NodeList[n]["pool_info"]["input_index"][position].append(output_index);
                    }
                }
                output_index += 1;
            }
        }
    }
}




void InferencePipelineDesign::ShowClassificationInfo(Json::Value &DNNInfo)
{
    std::cout << "==================== Number Info ====================" << std::endl;
    for (int i = 0; i < node_num; ++i)
    {
        std::cout.setf(std::ios::left); //设置对齐方式为left
        std::cout.width(40); //设置宽度，不足用空格填充
        std::cout << NodeList[i]["name"] ;
        std::cout << "    " << NodeList[i]["level_index"] << "    " << NodeList[i]["index_in_level"] << std::endl;
    }
}

void InferencePipelineDesign::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/5_pd.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}