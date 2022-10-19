//
// Created by SXT on 2022/10/11.
//

/////////////////////////////////////////////// 重要假设 ///////////////////////////////////////////////
/////////////////////////////////////// FC和CONV层只有一个生产者  ///////////////////////////////////////


#include "ElementPipelineSchedule.h"
ElementPipelineSchedule::ElementPipelineSchedule(std::string model_name_)
{
    core_num = PIMCOM_3_virtual_core_crossbar_map.size();
    node_num = PIMCOM_node_list.size();
    model_name = model_name_;
    if (model_name == "inception-v1")
    {
        last_node_index = 142;
        last_node_output_channel_num = 1;
    }
    else if (model_name == "alexnet")
    {
        last_node_index = 23;
        last_node_output_channel_num = 1;
    }
    else if (model_name == "vgg16")
    {
        last_node_index = 41;
        last_node_output_channel_num = 1;
    }
    else if (model_name == "resnet18")
    {
        last_node_index = 60;
        last_node_output_channel_num = 1;
    }
    else if (model_name == "resnetsim")
    {
        last_node_index = 23;
        last_node_output_channel_num = 784;
    }
    else if (model_name == "inceptionsim")
    {
        last_node_index = 39;
        last_node_output_channel_num = 169;
    }
    else
    {
        last_node_index = -1;
        last_node_output_channel_num = -1;
    }
}

std::set<std::string> no_consider_node = {"OP_INPUT", "OP_FLATTEN", "OP_RESHAPE", "OP_DROPOUT", "OP_LRN", "OP_SOFTMAX"};
std::vector<std::set<int>> early_termination;
std::vector<int> node_output_channel_num;
/////////////////////////////////////// NODE ///////////////////////////////////////
std::vector<std::vector<int>> node_AG_mapping;               //// 记录每个节点有哪些AG
std::vector<std::vector<int>> node_AG0_index_in_replication; //// 记录每个节点若干个rep的AG0的AG_index_in_total
std::vector<int> node_replication_num;                      //// 记录生产-消费关系中每个结点的复制倍数（包括pool、vec等）
std::vector<std::map<int,int>> node_provider_index_2_index_in_all_providers; //// node_provider_index_2_index_in_all_providers[vec_node_index][provider_index]=0、1、2（也就是根据provider_index得到index_in_all_provider）

/////////////////////////////////////// AG ///////////////////////////////////////
std::vector<std::vector<int>> AG_key_input_channel_index;
std::vector<std::vector<int>> AG_min_max_input_channel_index;
static int AG_produce_output_channel_num[MAX_AG] = {0};
std::vector<std::vector<int>> AG_ready_input_channel_list;
static int AG_rest_output_channel_num[MAX_AG] = {0};

/////////////////////////////////////// POOL ///////////////////////////////////////
std::vector<std::vector<std::vector<int>>> pool_rep_min_max_output_channel_index; //// 输出任务的划分。pool_rep_min_max_output_channel_index[pool_node_index][replication_index][0]是min，ool_rep_min_max_output_channel_index[pool_node_index][replication_index][1]是max
std::vector<std::vector<std::vector<int>>> pool_rep_min_max_input_channel_index;  //// 根据输出任务，不同rep需要存储不同的输入通道。pool_rep_min_max_input_channel_index[pool_node_index][replication_index][0]是min，pool_rep_min_max_input_channel_index[pool_node_index][replication_index][1]是max
std::vector<std::vector<std::vector<int>>> pool_rep_key_input_channel_index;      //// 不同rep不同input_cycle的key input channel index。
std::vector<std::vector<int>> pool_rep_produce_output_channel_num;                //// 不同rep已经处理的pool操作数。pool_rep_produce_output_channel_num[pool_node_index][replication_index]
std::vector<std::vector<std::vector<int>>> pool_rep_ready_input_channel_list;

/////////////////////////////////////// VEC ///////////////////////////////////////
// 对于vector操作，input channel即output channel，所以不用区分
std::vector<std::vector<std::vector<int>>> vec_rep_min_max_channel_index; //// vec_rep_min_max_channel_index[vec_node_index][replication_index][0]、vec_rep_min_max_channel_index[vec_node_index][replication_index][1]
std::vector<std::vector<std::vector<std::vector<int>>>> vec_rep_prov_ready_input_channel_list;   ////vec_rep_prov_ready_input_channel_list[vec_node_index][replication_index][provider_index]是一个列表，长度为算子的input_channel_num
std::vector<std::vector<int>> vec_rep_produce_output_channel_num;         //// vec_rep_produce_output_channel_num[vec_node_index][replication_index]就是该算子该rep已经处理的output_channel数目
std::vector<std::vector<std::vector<int>>> vec_prov_rep_index;        //// vec_prov_rep_index[vec_node_index][prov_index]是一个列表，列表的第i位为j，说明vec_node_index的第prov_index的生产者提供的第i个input_channel来自其第j个rep。

/////////////////////////////////////// Ready Data ///////////////////////////////////////
struct ready_input_channel_info
{
    int start_input_channel_index;
    int ready_input_channel_num;
};
std::vector<std::map<int, struct ready_input_channel_info>> AG_new_ready_input_channel_list; //// AG_new_ready_input_channel_list[AG_index][rep_index] 这里的rep_index是前一层的rep_index，因为是前一层的不同rep向本AG来传递数据。
std::vector<std::vector<std::map<int, struct ready_input_channel_info>>> pool_rep_new_ready_input_channel_list;


/////////////////////////////////////// SPLIT ///////////////////////////////////////
std::vector<std::vector<std::vector<int>>> node_rep_split_output_channel_index_list;  //// node_rep_split_output_channel_index_list[node_index][replication_index]是其负责的output_channel_index list
std::vector<std::vector<std::vector<int>>> node_rep_split_key_input_channel_index_list; //// node_rep_split_key_input_channel_index_list[node_index][replication_index]是对应output_channel_index的key input channel index
std::vector<std::vector<std::vector<int>>> node_rep_split_ready_input_channel_index_list;
std::vector<std::vector<int>> node_rep_split_produce_output_channel_num;
std::vector<std::vector<int>> node_rep_split_output_channel_num_list;



std::vector<struct AG_info_schedule> PIMCOM_4_element_AG_info_list;
std::vector<int> PIMCOM_4_element_AG_index_in_replication;
std::vector<std::vector<int>> PIMCOM_4_topology_provider_consumer_relation;
std::vector<std::vector<int>> PIMCOM_4_topology_consumer_provider_relation;

void ElementPipelineSchedule::SchedulePreparation()
{
    node_replication_num.resize(node_num);

    //// 生成一个AG_info_list，包括所有AG的各种信息
    int AG_num = PIMCOM_2_resource_info.AGs;
    PIMCOM_4_element_AG_index_in_replication.resize(AG_num);
    node_AG0_index_in_replication.resize(node_num);
    for (int i = 0; i < AG_num; ++i)
    {
        int core_index = PIMCOM_3_hierarchy_map.whole[i][0].vcore;
        int rep_index = PIMCOM_3_hierarchy_map.whole[i][0].replication_index;
        int node_index = PIMCOM_3_hierarchy_map.whole[i][0].node_index;
        int AG_index_in_total = PIMCOM_3_hierarchy_map.whole[i][0].array_group_total;
        int AG_index_in_replication = PIMCOM_3_hierarchy_map.whole[i][0].array_group_in_weight;
        int AG_num_per_replication = PIMCOM_3_hierarchy_map.whole[i][0].AG_num_per_replication;
        int replication_index = PIMCOM_3_hierarchy_map.whole[i][0].replication_index;
        int input_cycle_this_AG_start = PIMCOM_3_hierarchy_map.whole[i][0].input_cycle_this_replication_start;
        bool first_layer = PIMCOM_node_list[node_index].provider_index[0] == 0;
        PIMCOM_4_element_AG_index_in_replication[AG_index_in_total] = AG_index_in_replication;
        node_replication_num[node_index] = PIMCOM_node_list[node_index].replication_num;

        struct AG_info_schedule AGInfo;
        AGInfo.AG_index_in_total = AG_index_in_total;
        AGInfo.AG_index_in_replication = AG_index_in_replication;
        AGInfo.AG_num_per_replication = AG_num_per_replication;
        AGInfo.replication_index = rep_index;
        AGInfo.replication_num = PIMCOM_node_list[node_index].replication_num;
        AGInfo.replication_num_origin = PIMCOM_node_list[node_index].replication_num_origin;
        AGInfo.input_cycle_in_total = PIMCOM_node_list[node_index].input_cycle_in_total;
        AGInfo.level_index = PIMCOM_node_list[node_index].level_index;
        AGInfo.core_index = core_index;
        AGInfo.node_index = node_index;
        AGInfo.input_cycle_this_replication_start = input_cycle_this_AG_start;
        AGInfo.first_layer = first_layer;

        int effective_node_index = PIMCOM_node_list[node_index].effective_node_index;
        int crossbar_num_AG = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list.size();
        int crossbar_start_index = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list[0];
        int crossbar_end_index = PIMCOM_2_AG_partition[effective_node_index].replication[replication_index].AG_list[AG_index_in_replication].virtual_crossbar_list[crossbar_num_AG - 1];
        int input_element_num = PIMCOM_2_virtual_crossbar[crossbar_start_index].height_end - PIMCOM_2_virtual_crossbar[crossbar_start_index].height_start + 1;
        int output_element_num = PIMCOM_2_virtual_crossbar[crossbar_end_index].width_end - PIMCOM_2_virtual_crossbar[crossbar_start_index].width_start + 1;
        AGInfo.input_element_num = input_element_num;
        AGInfo.output_element_num = output_element_num;
        PIMCOM_4_element_AG_info_list.push_back(AGInfo);

        //// 记录每个结点、每个rep中AG0的AG_index_in_total。后续会用到。
        if (AG_index_in_replication == 0)
            node_AG0_index_in_replication[node_index].push_back(AG_index_in_total);
    }


    //// 增加对于复杂拓扑的支持
    PIMCOM_4_topology_provider_consumer_relation.resize(node_num);
    PIMCOM_4_topology_consumer_provider_relation.resize(node_num);
    node_provider_index_2_index_in_all_providers.resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        std::string operation = PIMCOM_node_list[i].operation;
        if (PIMCOM_node_list[i].consumer_num == 0)
        {
            PIMCOM_4_topology_provider_consumer_relation[i].push_back(0);
            continue;
        }
        if (operation == "OP_CONV" || operation == "OP_FC")
        {
            int consumer_index = PIMCOM_node_list[i].consumer_index[0]; // 一般认为CONV和FC只有一个消费者
            std::string consumer_operation = PIMCOM_node_list[consumer_index].operation;
            if (consumer_operation == "OP_RELU" || consumer_operation == "OP_TANH" || consumer_operation == "OP_SIGMOID")
            {
                for (int j = 0; j < PIMCOM_node_list[consumer_index].consumer_index.size(); ++j)
                {
                    int consumer_consumer_index = PIMCOM_node_list[consumer_index].consumer_index[j];
                    PIMCOM_4_topology_provider_consumer_relation[i].push_back(consumer_consumer_index);
                    PIMCOM_4_topology_consumer_provider_relation[consumer_consumer_index].push_back(i);
                    node_provider_index_2_index_in_all_providers[consumer_consumer_index][i] = node_provider_index_2_index_in_all_providers[consumer_consumer_index].size();
                }
            }
            else
            {
                PIMCOM_4_topology_provider_consumer_relation[i].push_back(consumer_index);
                PIMCOM_4_topology_consumer_provider_relation[consumer_index].push_back(i);
                node_provider_index_2_index_in_all_providers[consumer_index][i] = node_provider_index_2_index_in_all_providers[consumer_index].size();
            }
        }
        else
        {
            if (operation == "OP_RELU" || operation == "OP_TANH" || operation == "OP_SIGMOID")
            {
                int provider_index = PIMCOM_node_list[i].provider_index[0];
                if (PIMCOM_node_list[provider_index].operation == "OP_CONV" || PIMCOM_node_list[provider_index].operation == "OP_FC")
                    continue;
            }
            int consumer_num = PIMCOM_node_list[i].consumer_num;
            for (int j = 0; j < consumer_num; ++j)
            {
                int consumer_index = PIMCOM_node_list[i].consumer_index[j];
                PIMCOM_4_topology_provider_consumer_relation[i].push_back(consumer_index);
                PIMCOM_4_topology_consumer_provider_relation[consumer_index].push_back(i);
                node_provider_index_2_index_in_all_providers[consumer_index][i] = node_provider_index_2_index_in_all_providers[consumer_index].size();
            }
        }
    }

    std::cout << "provider - consumer" << std::endl;
    for (int i = 0; i < node_num; ++i)
    {
        for (int j = 0; j < PIMCOM_4_topology_provider_consumer_relation[i].size(); ++j)
        {
            std::cout << i << " :" << PIMCOM_4_topology_provider_consumer_relation[i][j] << std::endl;
        }
    }

    //// 得到每个AG的key_input_channel_index
    AG_key_input_channel_index.resize(AG_num);

    for (int i = 0; i < PIMCOM_2_AG_partition.size(); ++i)
    {
        int node_index = PIMCOM_2_AG_partition[i].index;
        std::string operation = PIMCOM_2_AG_partition[i].operation;
        if (operation == "OP_CONV")
        {
            int replication_num = PIMCOM_2_AG_partition[i].replication_num;
            for (int j = 0; j < replication_num; ++j)
            {
                int AG_num_in_replication = PIMCOM_2_AG_partition[i].replication[j].AG_index.size();
                for (int k = 0; k < AG_num_in_replication; ++k)
                {
                    int AG_index = PIMCOM_2_AG_partition[i].replication[j].AG_index[k];
                    int rest_output_channel_num = PIMCOM_2_AG_partition[i].replication[j].input_cycle_this_replication;
                    AG_rest_output_channel_num[AG_index] = rest_output_channel_num;
                    int input_cycle_start_this_replication = PIMCOM_2_AG_partition[i].replication[j].input_cycle_this_start;
                    int input_cycle_end_this_replication = PIMCOM_2_AG_partition[i].replication[j].input_cycle_this_end;
                    AG_key_input_channel_index[AG_index].resize(input_cycle_end_this_replication - input_cycle_start_this_replication + 1);
//                    std::cout << "AG_index:" << AG_index << "  length:" << (input_cycle_end_this_replication - input_cycle_start_this_replication+1) << std::endl;
                    for (int l = input_cycle_start_this_replication; l <= input_cycle_end_this_replication; ++l)
                    {
                        int key_channel_index = PIMCOM_conv_pool_input_output_info[node_index].output_index[l].back();
//                        AG_key_input_channel_index[AG_index].push_back(key_channel_index); // 这样太慢了
                        AG_key_input_channel_index[AG_index][l-input_cycle_start_this_replication] = key_channel_index;
                    }
                }
            }
        }
        else
        {
            int provider_node_index = PIMCOM_4_topology_consumer_provider_relation[node_index][0]; //// 应该没有问题，FC的生产者一般只有一个。
            //// 找到该fc的前面conv或pool或fc的生产者，这关系到key_input_channel_index
            while (PIMCOM_node_list[provider_node_index].operation != "OP_CONV" && PIMCOM_node_list[provider_node_index].operation != "OP_POOL" && PIMCOM_node_list[provider_node_index].operation != "OP_FC")
                provider_node_index = PIMCOM_4_topology_consumer_provider_relation[provider_node_index][0];
            //// 如果FC的前面是CONV，则key_channel_index是output_channel_num-1。意思是需要等前一个层把全部结果都穿过来之后再进行处理。而如果FC的前面是FC，则为0，只要前面FC传一次就够。
            int key_channel_index = 0;
            if (PIMCOM_node_list[provider_node_index].operation == "OP_CONV" || PIMCOM_node_list[provider_node_index].operation == "OP_POOL")
            {
                int output_channel_num = PIMCOM_node_list[provider_node_index].output_dim[2] * PIMCOM_node_list[provider_node_index].output_dim[3];
                key_channel_index = output_channel_num-1; // output_channel_index = output_channel_num - 1
            }
            else if (PIMCOM_node_list[provider_node_index].operation == "OP_FC")
            {
                key_channel_index = 0;
            }
            int AG_num_in_replication = PIMCOM_2_AG_partition[i].replication[0].AG_index.size();
            for (int k = 0; k < AG_num_in_replication; ++k)
            {
                int AG_index = PIMCOM_2_AG_partition[i].replication[0].AG_index[k];
                int rest_output_channel_num = PIMCOM_2_AG_partition[i].replication[0].input_cycle_this_replication;
                AG_rest_output_channel_num[AG_index] = rest_output_channel_num;
                AG_key_input_channel_index[AG_index].push_back(key_channel_index);
            }
        }
    }

    //// 准备其他AG的信息（用于CONV和FC层的准备与开始）
    AG_ready_input_channel_list.resize(PIMCOM_2_resource_info.AGs);
    AG_new_ready_input_channel_list.resize(PIMCOM_2_resource_info.AGs);
    AG_min_max_input_channel_index.resize(AG_num);
    node_AG_mapping.resize(node_num);
    for (int i = 0; i < AG_num; ++i)
    {
        AG_min_max_input_channel_index[i].resize(2);
        int node_index = PIMCOM_3_hierarchy_map.whole[i][0].node_index;
        int AG_index = PIMCOM_3_hierarchy_map.whole_index[i];
        int input_channel_num = PIMCOM_node_list[node_index].input_dim[2] * PIMCOM_node_list[node_index].input_dim[3];
        if (PIMCOM_node_list[node_index].operation == "OP_CONV")
        {
            int input_cycle_start = PIMCOM_3_hierarchy_map.whole[i][0].input_cycle_this_replication_start;
            int input_cycle_end = PIMCOM_3_hierarchy_map.whole[i][0].input_cycle_this_replication_end;
            int min_max_input_channel_index[2];
            GetMinMaxInputChannelFromInputCycle(min_max_input_channel_index, node_index, input_cycle_start, input_cycle_end);
            AG_min_max_input_channel_index[i][0] = min_max_input_channel_index[0];
            AG_min_max_input_channel_index[i][1] = min_max_input_channel_index[1];
            AG_ready_input_channel_list[i].resize(input_channel_num);
        }
        else
        {
            AG_min_max_input_channel_index[i][0] = 0;
            AG_min_max_input_channel_index[i][1] = 50000;
            //// 这里size不设置成1是因为有的FC的前一层是CONV，需要接收完后才能开始FC的计算
            AG_ready_input_channel_list[i].resize(50000);
        }
        int provider_index = PIMCOM_4_topology_consumer_provider_relation[node_index][0];
        int effective_provider_index = PIMCOM_node_list[provider_index].AG0_node_index; // TODO effective指的是conv和fc，一般只有一个生产者或消费者
        node_AG_mapping[node_index].push_back(AG_index);
    }

    //// 得到每个input_cycle_index所关联的AG
    GetRelatedAGIndex();



    /////////////////////////////////////////////////// POOL-REP ///////////////////////////////////////////////////
    //////////                  pool_rep_min_max_output_channel_index;            /// 输出任务的划分
    //////////                  pool_rep_key_input_channel_index;                 //// 不同rep、不同output_channel的关键输入通道
    //////////                  pool_rep_min_max_input_channel_index;             //// 根据输出任务，不同rep需要存储不同的输入通道
    //////////                  pool_rep_produce_output_channel_num;              //// 不同rep已经处理的pool操作数

    //// 对不同rep的pool进行任务的划分
    pool_rep_min_max_output_channel_index.resize(node_num);
    pool_rep_min_max_input_channel_index.resize(node_num);
    pool_rep_ready_input_channel_list.resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        if (PIMCOM_node_list[i].operation == "OP_POOL")
        {
            int input_channel_num = PIMCOM_node_list[i].input_dim[2] * PIMCOM_node_list[i].input_dim[3]; // pool output channel num
            int output_channel_num = PIMCOM_node_list[i].output_dim[2] * PIMCOM_node_list[i].output_dim[3]; // pool output channel num
            int provider_index = PIMCOM_4_topology_consumer_provider_relation[i][0]; //// POOL只有一个生产者
            int effective_provider_index = PIMCOM_node_list[provider_index].AG0_node_index;
            int replication_num = PIMCOM_node_list[effective_provider_index].replication_num;
            node_replication_num[i] = replication_num;
            pool_rep_min_max_output_channel_index[i].resize(replication_num);
            pool_rep_min_max_input_channel_index[i].resize(replication_num);
            pool_rep_ready_input_channel_list[i].resize(replication_num);
            std::vector<int> start_address_vector;
            std::vector<int> end_address_vector;
            DivideTheOutputChannel(start_address_vector, end_address_vector, output_channel_num, replication_num);
            for (int j = 0; j < replication_num; ++j)
            {
                pool_rep_min_max_output_channel_index[i][j].push_back(start_address_vector[j]); // start_output_index
                pool_rep_min_max_output_channel_index[i][j].push_back(end_address_vector[j]); // end_output_index
                int min_max_input_channel_index[2];
                GetMinMaxInputChannelFromInputCycle(min_max_input_channel_index, i, start_address_vector[j], end_address_vector[j]);
//                int min_input_channel = GetInputChannelFromOutputIndex(i, start_address_vector[j], 0);
//                int max_input_channel = GetInputChannelFromOutputIndex(i, end_address_vector[j], 1);
                pool_rep_min_max_input_channel_index[i][j].push_back(min_max_input_channel_index[0]);
                pool_rep_min_max_input_channel_index[i][j].push_back(min_max_input_channel_index[1]);
                pool_rep_ready_input_channel_list[i][j].resize(input_channel_num);
            }
        }
    }

    pool_rep_key_input_channel_index.resize(node_num);
    pool_rep_new_ready_input_channel_list.resize(node_num);
    pool_rep_produce_output_channel_num.resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        std::string operation = PIMCOM_node_list[i].operation;
        if (operation == "OP_POOL")
        {
            int input_channel_num = PIMCOM_node_list[i].input_dim[2] * PIMCOM_node_list[i].input_dim[3];
            int pre_rep_num = node_replication_num[i]; // previous_conv_replication_num
            pool_rep_new_ready_input_channel_list[i].resize(pre_rep_num);
            pool_rep_key_input_channel_index[i].resize(pre_rep_num);
            pool_rep_produce_output_channel_num[i].resize(pre_rep_num);
            for (int j = 0; j < pre_rep_num; ++j)
            {
                int output_channel_num = pool_rep_min_max_output_channel_index[i][j][1] - pool_rep_min_max_output_channel_index[i][j][0] + 1;
                pool_rep_key_input_channel_index[i][j].resize(output_channel_num);
                for (int k = 0; k < output_channel_num; ++k)
                {
                    int output_channel_index = pool_rep_min_max_output_channel_index[i][j][0] + k;
                    int key_channel_index = PIMCOM_conv_pool_input_output_info[i].output_index[output_channel_index].back();
                    pool_rep_key_input_channel_index[i][j][k] = key_channel_index;
                }
            }
        }
    }


    /////////////////////////////////////////////////// VEC-REP  ///////////////////////////////////////////////////
    ////    vec_rep_min_max_channel_index;
    ////            vec_rep_min_max_channel_index[vec_node_index][replication_index][0]、vec_rep_min_max_channel_index[vec_node_index][replication_index][1]
    ////    std::vector<std::vector<std::vector<std::vector<int>>>> vec_rep_prov_ready_input_channel_list;
    ////            vec_rep_prov_ready_input_channel_list[vec_node_index][replication_index][provider_index]是一个列表，长度为算子的input_channel_num
    ////    std::vector<std::vector<int>> vec_rep_produce_output_channel_num;
    ////            vec_rep_produce_output_channel_num[vec_node_index][replication_index]就是该算子该rep已经处理的output_channel数目
    vec_rep_min_max_channel_index.resize(node_num);
    vec_rep_produce_output_channel_num.resize(node_num);
    vec_rep_prov_ready_input_channel_list.resize(node_num);
    vec_prov_rep_index.resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        std::string operation = PIMCOM_node_list[i].operation;
        if (operation == "OP_ELTWISE" || operation == "OP_CONCAT" || operation == "OP_RELU" || operation == "OP_TANH" || operation == "OP_SIGMOID")
        {
            if (PIMCOM_4_topology_provider_consumer_relation[i].size() == 0)
                continue; // 跳过CONV和FC后的RELU
            int provider_num = PIMCOM_node_list[i].provider_num;
            int effective_provider_index = PIMCOM_node_list[i].AG0_node_index;
            int replication_num = PIMCOM_node_list[effective_provider_index].replication_num;
            node_replication_num[i] = replication_num;
            int output_channel_num = PIMCOM_node_list[i].output_dim[2] * PIMCOM_node_list[i].output_dim[3]; //// output_channel_num == input_channel_num for VEC OP
            std::vector<int> start_address_vector;
            std::vector<int> end_address_vector;
            DivideTheOutputChannel(start_address_vector, end_address_vector, output_channel_num, replication_num);
            vec_rep_min_max_channel_index[i].resize(replication_num);
            vec_rep_prov_ready_input_channel_list[i].resize(replication_num);
            vec_rep_produce_output_channel_num[i].resize(replication_num);
            for (int j = 0; j < replication_num; ++j)
            {
                vec_rep_min_max_channel_index[i][j].push_back(start_address_vector[j]);
                vec_rep_min_max_channel_index[i][j].push_back(end_address_vector[j]);
                vec_rep_prov_ready_input_channel_list[i][j].resize(provider_num);
                for (int k = 0; k < provider_num; ++k)
                {
                    vec_rep_prov_ready_input_channel_list[i][j][k].resize(output_channel_num);
                }
            }

            vec_prov_rep_index[i].resize(provider_num);
            for (int j = 0; j < provider_num; ++j)
            {
                vec_prov_rep_index[i][j].resize(output_channel_num);
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////////////////////////////// SPLIT //////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    node_rep_split_output_channel_index_list.resize(node_num);
    node_rep_split_key_input_channel_index_list.resize(node_num);
    node_rep_split_ready_input_channel_index_list.resize(node_num);
    node_rep_split_produce_output_channel_num.resize(node_num);
    node_rep_split_output_channel_num_list.resize(node_num);
    for (int i = 1; i < node_num; ++i)
    {
        if (PIMCOM_4_topology_provider_consumer_relation[i].size() > 0)
        {
            int replication_num = node_replication_num[i];
            if (replication_num == 0) // 对于flatten、reshape、softmax、lrn等层，其replication_num为0。
                continue;
            node_rep_split_output_channel_index_list[i].resize(replication_num);
            node_rep_split_key_input_channel_index_list[i].resize(replication_num);
            node_rep_split_ready_input_channel_index_list[i].resize(replication_num);
            node_rep_split_produce_output_channel_num[i].resize(replication_num);
            node_rep_split_output_channel_num_list[i].resize(replication_num);
            int output_channel_num;
            if (PIMCOM_node_list[i].operation == "OP_FC")
                output_channel_num = 1;
            else
                output_channel_num = PIMCOM_node_list[i].output_dim[2] * PIMCOM_node_list[i].output_dim[3];
            //// 为每个结点，每个复制倍数，选定它们需要负责的output_channel_index，同时确定key_input_channel_index
            for (int j = 0; j < output_channel_num; ++j)
            {
                int rep_index = j % replication_num;
                node_rep_split_output_channel_index_list[i][rep_index].push_back(j);
                node_rep_split_output_channel_num_list[i][rep_index]++;

                int key_input_channel_index;
                if (PIMCOM_node_list[i].operation == "OP_FC")
                {
                    int provider_node_index = PIMCOM_4_topology_consumer_provider_relation[i][0]; //// 应该没有问题，FC的生产者一般只有一个。
                    //// 找到该fc的前面conv或pool或fc的生产者，这关系到key_input_channel_index
                    while (PIMCOM_node_list[provider_node_index].operation != "OP_CONV" && PIMCOM_node_list[provider_node_index].operation != "OP_POOL" && PIMCOM_node_list[provider_node_index].operation != "OP_FC")
                        provider_node_index = PIMCOM_4_topology_consumer_provider_relation[provider_node_index][0];
                    //// 如果FC的前面是CONV，则key_channel_index是output_channel_num-1。意思是需要等前一个层把全部结果都穿过来之后再进行处理。而如果FC的前面是FC，则为0，只要前面FC传一次就够。
                    if (PIMCOM_node_list[provider_node_index].operation == "OP_CONV" || PIMCOM_node_list[provider_node_index].operation == "OP_POOL")
                    {
                        int tmp_output_channel_num = PIMCOM_node_list[provider_node_index].output_dim[2] * PIMCOM_node_list[provider_node_index].output_dim[3];
                        key_input_channel_index = tmp_output_channel_num - 1; // output_channel_index = output_channel_num - 1
                    }
                    else if (PIMCOM_node_list[provider_node_index].operation == "OP_FC")
                    {
                        key_input_channel_index = 0;
                    }
                }
                else if (PIMCOM_node_list[i].operation == "OP_CONV" || PIMCOM_node_list[i].operation == "OP_POOL")
                {
                    key_input_channel_index = GetInputChannelFromOutputIndex(i, j, 1);
                }
                else
                {
                    key_input_channel_index = j;
                }
                node_rep_split_key_input_channel_index_list[i][rep_index].push_back(key_input_channel_index);
            }

            int input_channel_num;
            if (PIMCOM_node_list[i].operation == "OP_FC")
                input_channel_num = 50000;
            else
                input_channel_num = PIMCOM_node_list[i].input_dim[2] * PIMCOM_node_list[i].input_dim[3];
            for (int j = 0; j < replication_num; ++j)
            {
                node_rep_split_ready_input_channel_index_list[i][j].resize(input_channel_num);
            }
        }
    }

    node_output_channel_num.resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        if (PIMCOM_node_list[i].operation == "OP_FC")
            node_output_channel_num[i] = 1;
        else if (no_consider_node.count(PIMCOM_node_list[i].operation))
            node_output_channel_num[i] = 0;
        else
            node_output_channel_num[i] = PIMCOM_node_list[i].output_dim[2] * PIMCOM_node_list[i].output_dim[3];
    }
}


//std::vector<struct AG_info_schedule> PIMCOM_4_element_AG_info_list;
//std::vector<int> PIMCOM_4_element_AG_index_in_replication;
//std::vector<std::vector<int>> PIMCOM_4_topology_provider_consumer_relation;
//std::vector<std::vector<int>> PIMCOM_4_topology_consumer_provider_relation;


void ElementPipelineSchedule::SavePreparation()
{
    Json::Value JsonPreparation;

    ///////////////////////////////////////// NODE ///////////////////////////////////////
    //std::vector<std::vector<int>> node_AG_mapping;               //// 记录每个节点有哪些AG
    //std::vector<std::vector<int>> node_AG0_index_in_replication; //// 记录每个节点若干个rep的AG0的AG_index_in_total
    //std::vector<int> node_replication_num;                      //// 记录生产-消费关系中每个结点的复制倍数（包括pool、vec等）
    //std::vector<std::map<int,int>> node_provider_index_2_index_in_all_providers; //// node_provider_index_2_index_in_all_providers[vec_node_index][provider_index]=0、1、2（也就是根据provider_index得到index_in_all_provider）

    JsonPreparation["node_AG_mapping"]["node"].resize(node_num);
    JsonPreparation["node_AG0_index_in_replication"]["node"].resize(node_num);
    JsonPreparation["node_replication_num"]["node"].resize(node_num);
    JsonPreparation["node_provider_index_2_index_in_all_providers"]["node"].resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        for (int j = 0; j < node_AG_mapping[i].size(); ++j)
        {
            JsonPreparation["node_AG_mapping"]["node"][i]["AG_index"][j] = node_AG_mapping[i][j];
        }
        for (int j = 0; j < node_AG0_index_in_replication[i].size(); ++j)
        {
            JsonPreparation["node_AG0_index_in_replication"]["node"][i]["AG0_index"][j] = node_AG0_index_in_replication[i][j];
        }
        JsonPreparation["node_replication_num"]["node"][i] = node_replication_num[i];

        int ii = 0;
        for (auto iter = node_provider_index_2_index_in_all_providers[i].begin(); iter != node_provider_index_2_index_in_all_providers[i].end(); iter++)
        {
            int provider_index = iter->first;
            int index_in_providers = iter->second;
            JsonPreparation["node_provider_index_2_index_in_all_providers"]["node"][i]["providers"][ii]["index_in_all_providers_of_this_node"] = index_in_providers;
            JsonPreparation["node_provider_index_2_index_in_all_providers"]["node"][i]["providers"][ii]["original_provider_index"] = provider_index;
            ii ++;
        }
    }

    ///////////////////////////////////////// AG ///////////////////////////////////////
    //std::vector<std::vector<int>> AG_key_input_channel_index;
    //std::vector<std::vector<int>> AG_min_max_input_channel_index;

    int AG_num = PIMCOM_2_resource_info.AGs;
    JsonPreparation["AG_key_input_channel_index"]["AG_list"].resize(AG_num);
    JsonPreparation["AG_min_max_input_channel_index"]["AG_list"].resize(AG_num);
    for (int i = 0; i < AG_num; ++i)
    {
        for (int j = 0; j < AG_key_input_channel_index[i].size(); ++j)
        {
            JsonPreparation["AG_key_input_channel_index"]["AG_list"][i]["key_input_channel_index"][j] = AG_key_input_channel_index[i][j];
        }

        JsonPreparation["AG_min_max_input_channel_index"]["AG_list"][i]["min_input_channel_index"] = AG_min_max_input_channel_index[i][0];
        JsonPreparation["AG_min_max_input_channel_index"]["AG_list"][i]["max_input_channel_index"] = AG_min_max_input_channel_index[i][1];
    }

    ///////////////////////////////////////// POOL ///////////////////////////////////////
    //std::vector<std::vector<std::vector<int>>> pool_rep_min_max_output_channel_index; //// 输出任务的划分。pool_rep_min_max_output_channel_index[pool_node_index][replication_index][0]是min，ool_rep_min_max_output_channel_index[pool_node_index][replication_index][1]是max
    //std::vector<std::vector<std::vector<int>>> pool_rep_min_max_input_channel_index;  //// 根据输出任务，不同rep需要存储不同的输入通道。pool_rep_min_max_input_channel_index[pool_node_index][replication_index][0]是min，pool_rep_min_max_input_channel_index[pool_node_index][replication_index][1]是max
    //std::vector<std::vector<std::vector<int>>> pool_rep_key_input_channel_index;      //// 不同rep不同input_cycle的key input channel index。

    JsonPreparation["pool_rep_min_max_output_channel_index"]["node_index"].resize(node_num);
    JsonPreparation["pool_rep_min_max_input_channel_index"]["node_index"].resize(node_num);
    JsonPreparation["pool_rep_key_input_channel_index"]["node_index"].resize(node_num);

    for (int i = 0; i < node_num; ++i)
    {
        for (int j = 0; j < pool_rep_min_max_output_channel_index[i].size(); ++j)
        {
            JsonPreparation["pool_rep_min_max_output_channel_index"]["node_index"][i]["replication_index"][j]["min_output_channel_index"] = pool_rep_min_max_output_channel_index[i][j][0];
            JsonPreparation["pool_rep_min_max_output_channel_index"]["node_index"][i]["replication_index"][j]["max_output_channel_index"] = pool_rep_min_max_output_channel_index[i][j][1];
        }

        for (int j = 0; j < pool_rep_min_max_input_channel_index[i].size(); ++j)
        {
            JsonPreparation["pool_rep_min_max_input_channel_index"]["node_index"][i]["replication_index"][j]["min_input_channel_index"] = pool_rep_min_max_input_channel_index[i][j][0];
            JsonPreparation["pool_rep_min_max_input_channel_index"]["node_index"][i]["replication_index"][j]["max_input_channel_index"] = pool_rep_min_max_input_channel_index[i][j][1];
        }

        for (int j = 0; j < pool_rep_key_input_channel_index[i].size(); ++j)
        {
            for (int k = 0; k < pool_rep_key_input_channel_index[i][j].size(); ++k)
            {
                JsonPreparation["pool_rep_key_input_channel_index"]["node_index"][i]["replication_index"][j]["key_input_channel_index"][k] = pool_rep_key_input_channel_index[i][j][k];
            }
        }
    }

    ///////////////////////////////////////// VEC ///////////////////////////////////////
    //std::vector<std::vector<std::vector<int>>> vec_rep_min_max_channel_index; //// vec_rep_min_max_channel_index[vec_node_index][replication_index][0]、vec_rep_min_max_channel_index[vec_node_index][replication_index][1]
    JsonPreparation["vec_rep_min_max_channel_index"]["node_index"].resize(node_num);
    for (int i = 0; i < node_num ; ++i)
    {
        for (int j = 0; j < vec_rep_min_max_channel_index[i].size(); ++j)
        {
            JsonPreparation["vec_rep_min_max_channel_index"]["node_index"][i]["replication_index"][j]["min_channel_index"] = vec_rep_min_max_channel_index[i][j][0];
            JsonPreparation["vec_rep_min_max_channel_index"]["node_index"][i]["replication_index"][j]["max_channel_index"] = vec_rep_min_max_channel_index[i][j][1];
        }
    }


    ///////////////////////////////////////// SPLIT ///////////////////////////////////////
    //std::vector<std::vector<std::vector<int>>> node_rep_split_output_channel_index_list;  //// node_rep_split_output_channel_index_list[node_index][replication_index]是其负责的output_channel_index list
    //std::vector<std::vector<std::vector<int>>> node_rep_split_key_input_channel_index_list; //// node_rep_split_key_input_channel_index_list[node_index][replication_index]是对应output_channel_index的key input channel index
    //std::vector<std::vector<int>> node_rep_split_output_channel_num_list;
    JsonPreparation["split_node_rep_split_output_channel_index_list"].resize(node_num);
    JsonPreparation["split_node_rep_split_key_input_channel_index_list"].resize(node_num);
    JsonPreparation["split_node_rep_split_output_channel_num_list"].resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        for (int j = 0; j < node_rep_split_output_channel_index_list[i].size(); j++)
        {
            JsonPreparation["split_node_rep_split_output_channel_num_list"][i][j] = node_rep_split_output_channel_num_list[i][j];
            for (int k = 0; k < node_rep_split_output_channel_index_list[i][j].size(); ++k)
            {
                JsonPreparation["split_node_rep_split_output_channel_index_list"][i][j][k] = node_rep_split_output_channel_index_list[i][j][k];
                JsonPreparation["split_node_rep_split_key_input_channel_index_list"][i][j][k] = node_rep_split_key_input_channel_index_list[i][j][k];
            }
        }
    }

    Json::Reader jsonReader;
    Json::Value DNNInfo;
    std::ifstream jsonFile("../models/JSON/"+model_name+".json");
    if(!jsonReader.parse(jsonFile, DNNInfo, true))
    {
        std::cout << "error" << std::endl;
        return ;
    }

    JsonPreparation["0_node_list"] = DNNInfo["node_list"];

    std::string strJson = JsonPreparation.toStyledString();
    std::ofstream fob("../output/Preparation.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}

void ElementPipelineSchedule::DivideTheOutputChannel(std::vector<int> &start_address_vector, std::vector<int> &end_address_vector, int output_channel_num_total, int replication_num)
{
    //// 如果有output_channel_num < replication_num的情况，那么处理长度为0的rep的start会比end高1。可以根据这一点来跳过这些rep。
    start_address_vector.clear();
    end_address_vector.clear();
    std::vector<int> output_channel_allocated;
    for (int i = 0; i < replication_num; ++i)
        output_channel_allocated.push_back(ceil(float(output_channel_num_total) / float(replication_num)));
    int minus_num = ceil(float(output_channel_num_total) / float(replication_num)) * replication_num - output_channel_num_total;
    for (int i = 0; i < minus_num; ++i)
        output_channel_allocated[replication_num-1-i] -= 1;
    int start_address;
    int end_address = -1;
    for (int i = 0; i < replication_num; ++i)
    {
        start_address = end_address + 1;
        end_address = start_address + output_channel_allocated[i] - 1;
        start_address_vector.push_back(start_address);
        end_address_vector.push_back(end_address);
    }
}

//// 把channel_related_AG_index从output_channel改成了input_channel。根据对比可以发现，这样应该更好。因为pool的存在，原来那种做法可能会找到一个并不直接相连的两层。
std::vector<std::vector<std::vector<int>>> input_channel_related_AG_index; // input_channel_related_AG_index[node_index][output_channel_index]中包含一系列AG
void ElementPipelineSchedule::GetRelatedAGIndex()
{
    input_channel_related_AG_index.resize(node_num);
    for (int i = 0; i < PIMCOM_2_AG_partition.size(); ++i)
    {
        int node_index = PIMCOM_2_AG_partition[i].index;
        int input_channel_num = PIMCOM_node_list[node_index].input_dim[2] * PIMCOM_node_list[node_index].input_dim[3];
        if (PIMCOM_node_list[node_index].operation == "OP_FC")
            input_channel_num = 50000;
        input_channel_related_AG_index[node_index].resize(input_channel_num);
        int AG_num = node_AG_mapping[node_index].size();
        for (int j = 0; j < AG_num; ++j)
        {
            int AG_index = node_AG_mapping[node_index][j];
            for (int k = 0; k < input_channel_num; ++k)
            {
                if (AG_min_max_input_channel_index[AG_index][0] <= k && AG_min_max_input_channel_index[AG_index][1] >= k)
                {
                    input_channel_related_AG_index[node_index][k].push_back(AG_index);
                }
            }
        }
    }
}


//bool ElementPipelineSchedule::CheckNormalPrepared(int AG_index_in_total)
//{
//    int produced_output_channel_num = AG_produce_output_channel_num[AG_index_in_total];
//    int key_input_channel_index = AG_key_input_channel_index[AG_index_in_total][produced_output_channel_num];
//    int start = AG_min_max_input_channel_index[AG_index_in_total][0];
//    for (int i = start; i <= key_input_channel_index; ++i)
//    {
//        if (AG_ready_input_channel_list[AG_index_in_total][i] != 1)
//        {
//            return false;
//        }
//    }
//    return true;
//}

//bool ElementPipelineSchedule::CheckPoolRepPrepared(int node_index,int replication_index, int pool_key_input_channel_index_current)
//{
//    int start = pool_rep_min_max_input_channel_index[node_index][replication_index][0];
//    for (int i = start; i <= pool_key_input_channel_index_current; ++i)
//    {
//        if (pool_rep_ready_input_channel_list[node_index][replication_index][i] != 1)
//            return false;
//    }
//    return true;
//}

bool ElementPipelineSchedule::NewCheckNormalPrepared(int AG_index_in_total)
{
    int produced_output_channel_num = AG_produce_output_channel_num[AG_index_in_total];
    if (AG_key_input_channel_index[AG_index_in_total].size() == 0)
        return false;
    int key_input_channel_index = AG_key_input_channel_index[AG_index_in_total][produced_output_channel_num];
    int needed_start_index = AG_min_max_input_channel_index[AG_index_in_total][0];
    int needed_end_index = key_input_channel_index + 1;
    for (auto iter = AG_new_ready_input_channel_list[AG_index_in_total].begin(); iter != AG_new_ready_input_channel_list[AG_index_in_total].end() ; ++iter)
    {
        int this_rep_ready_start_input_channel_index = iter->second.start_input_channel_index;
        int this_rep_ready_end_input_channel_index = this_rep_ready_start_input_channel_index + iter->second.ready_input_channel_num;
        if (needed_start_index < this_rep_ready_start_input_channel_index)
            return false;
        else
        {
            if (needed_end_index <= this_rep_ready_end_input_channel_index)
            {
                return true;
            }
            else
            {
                needed_start_index = this_rep_ready_end_input_channel_index;
            }
        }
    }
    return false;
}

bool ElementPipelineSchedule::NewCheckPoolRepPrepared(int node_index, int replication_index, int pool_key_input_channel_index_current)
{
    int needed_start_index = pool_rep_min_max_input_channel_index[node_index][replication_index][0];
    int needed_end_index = pool_key_input_channel_index_current + 1;

    for (auto iter = pool_rep_new_ready_input_channel_list[node_index][replication_index].begin(); iter != pool_rep_new_ready_input_channel_list[node_index][replication_index].end() ; ++iter)
    {
        int this_rep_ready_start_input_channel_index = iter->second.start_input_channel_index;
        int this_rep_ready_input_channel_num = iter->second.ready_input_channel_num;
        int this_rep_ready_end_input_channel_index = this_rep_ready_start_input_channel_index + this_rep_ready_input_channel_num;
//        if (node_index == 121)
//        {
//            std::cout << iter->first << "  " << this_rep_ready_start_input_channel_index << "  " << this_rep_ready_end_input_channel_index << std::endl;
//        }
            if (needed_start_index < this_rep_ready_start_input_channel_index)
            return false;
        else
        {
            if (needed_end_index <= this_rep_ready_end_input_channel_index)
                return true;
            else
            {
                needed_start_index = this_rep_ready_end_input_channel_index;
            }
        }
    }
//    if (node_index == 121)
//    {
//        std::cout << std::endl;
//    }
    return false;
}

bool ElementPipelineSchedule::NewCheckVecPrepared(int node_index, int replication_index, int vec_rep_key_input_channel_index_current)
{
    int provider_num = PIMCOM_4_topology_consumer_provider_relation[node_index].size();
    for (int i = 0; i < provider_num; ++i)
    {
        int provider_index = PIMCOM_4_topology_consumer_provider_relation[node_index][i];
        int index_in_all_providers = node_provider_index_2_index_in_all_providers[node_index][provider_index];
        if (vec_rep_prov_ready_input_channel_list[node_index][replication_index][index_in_all_providers][vec_rep_key_input_channel_index_current] == 0)
        {
            return false;
        }
    }
    return true;
}


std::vector<std::set<int>> CheckForIndex;
std::vector<int> CheckForNum;
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Dataflow Gather ///////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void ElementPipelineSchedule::ScheduleNaiveGatherDataFlowMain(std::ofstream & OutFile,int instruction_group_index)
{
//    OutFile << instruction_group_index << std::endl;
    int AG_num = PIMCOM_2_resource_info.AGs;
    for (int i = 0; i < AG_num; ++i)
    {
        int AG_index_in_replication = PIMCOM_4_element_AG_index_in_replication[i];
        if (AG_index_in_replication != 0)
            continue;
        struct AG_info_schedule thisAG = PIMCOM_4_element_AG_info_list[i];
        int node_index = thisAG.node_index;

        //// 一个node提前终止的判断
        bool Process = false;
        if (early_termination[node_index].size() == 0) //// early_termination[node_index].size() == 0说明该CONV/FC后面无"后续操作"（不包括紧跟的relu）
        {
            if (CheckForIndex[node_index].size() != node_output_channel_num[node_index] || CheckForNum[node_index] != node_output_channel_num[node_index])
                Process = true;
        }
        else  //// early_termination[node_index].size() != 0说明该CONV/FC后面有后续操作（不包括紧跟的relu，而是说pool、concat之类）
        {
            //// 如果全部后续操作都进行完成了，那么可以不再遍历。
            for (auto iter = early_termination[node_index].begin(); iter != early_termination[node_index].end(); iter++)
            {
                if (CheckForIndex[*iter].size() != node_output_channel_num[*iter] || CheckForNum[*iter] != node_output_channel_num[*iter])
                    Process = true;
            }
        }
        if (!Process)
        {
            continue;
        }

        int AG_index_in_total = thisAG.AG_index_in_total;
        int input_cycle_this_replication_start = thisAG.input_cycle_this_replication_start;
        bool first_layer = thisAG.first_layer;
        int replication_index = thisAG.replication_index;

        //// 只需要考虑 AG_index_in_replication==0 的情况
        if ( first_layer ||  NewCheckNormalPrepared(AG_index_in_total) )
        {
            int consumer_index = *PIMCOM_4_topology_provider_consumer_relation[node_index].begin(); //// 一般情况CONV或FC都只有一个消费者
            //// 把"AG_rest_output_channel_num[AG_index_in_total] > 0"的判断设置在这里是因为当有的层结束后，后面还依赖于该层进入POST操作
            if (AG_rest_output_channel_num[AG_index_in_total] > 0)
            {
                int output_channel_index = input_cycle_this_replication_start + AG_produce_output_channel_num[AG_index_in_total];
                int produced_output_channel_num = AG_produce_output_channel_num[AG_index_in_total];
                int key_input_channel_index = AG_key_input_channel_index[AG_index_in_total][produced_output_channel_num];

//                OutFile << std::endl;
//                OutFile << "    Normal Begin!" << std::endl;
//                OutFile << "    node_index:" << node_index  << std::endl;
//                OutFile << "    output_index:" << output_channel_index << std::endl;
//                OutFile << "    Normal End" << std::endl << std::endl;

                CheckForIndex[node_index].insert(output_channel_index);
                CheckForNum[node_index]++;
                AG_rest_output_channel_num[AG_index_in_total] -= 1;
                AG_produce_output_channel_num[AG_index_in_total] += 1;
                ScheduleNaiveGatherDataFlowPost(OutFile, instruction_group_index, node_index, consumer_index, output_channel_index, replication_index, 1);
            }
            else
            {
                // TODO 这里有一点要注意：最后一个节点（consumer_num==0）的consumer设置为0，也就是会回到0，但是此时0肯定已经完成了。所以最终能顺利结束。
                int output_channel_index = input_cycle_this_replication_start + AG_produce_output_channel_num[AG_index_in_total] - 1; //// 已经算完的CONV-REP的output_channel_index，一直以最后一个output_channel_index进入后续处理
                ScheduleNaiveGatherDataFlowPost(OutFile, instruction_group_index, node_index, consumer_index, output_channel_index, replication_index, 0);
            }
        }
    }
}

void ElementPipelineSchedule::ScheduleNaiveGatherDataFlowPost(std::ofstream & OutFile, int instruction_group_index, int this_node_index, int next_node_index, int input_channel_index, int complete_replication_index, bool effective_post)
{
    std::string next_operation = PIMCOM_node_list[next_node_index].operation;
    if (next_operation == "OP_POOL")
    {
        int replication_num = node_replication_num[next_node_index];
        for (int k = 0; k < replication_num; ++k)
        {
            int pool_rep_produced_output_channel_num = pool_rep_produce_output_channel_num[next_node_index][k];
            if ( (pool_rep_produced_output_channel_num < (pool_rep_min_max_output_channel_index[next_node_index][k][1] - pool_rep_min_max_output_channel_index[next_node_index][k][0] + 1)))
            {
                //// 传输(传递给其他rep，因为池化是分在各个rep处理的)
                if(pool_rep_min_max_input_channel_index[next_node_index][k][0] <= input_channel_index &&
                   pool_rep_min_max_input_channel_index[next_node_index][k][1] >= input_channel_index &&
                   pool_rep_ready_input_channel_list[next_node_index][k][input_channel_index] == 0)
                {
                    pool_rep_ready_input_channel_list[next_node_index][k][input_channel_index] = 1; //// 这里的设置和以前的作用不同。详情见10-16笔记。是个比较复杂的问题。
                    if (pool_rep_new_ready_input_channel_list[next_node_index][k].count(complete_replication_index) == 0)
                    {
                        pool_rep_new_ready_input_channel_list[next_node_index][k][complete_replication_index].start_input_channel_index = input_channel_index;
                        pool_rep_new_ready_input_channel_list[next_node_index][k][complete_replication_index].ready_input_channel_num = 1;
                    }
                    else
                    {
                        pool_rep_new_ready_input_channel_list[next_node_index][k][complete_replication_index].ready_input_channel_num += 1;
                    }
                }
                //// 再进行池化操作
                int pool_rep_key_input_channel_index_current = pool_rep_key_input_channel_index[next_node_index][k][pool_rep_produced_output_channel_num];
                if (NewCheckPoolRepPrepared(next_node_index, k, pool_rep_key_input_channel_index_current))
                {
//                    OutFile << std::endl;
//                    OutFile << "    POOL Begin!" << std::endl;
//                    OutFile << "    node_index:" << next_node_index  << std::endl;
//                    OutFile << "    replication_index:" << k << std::endl;
//                    OutFile << "    pool_rep_produced_output_channel_index:" << pool_rep_produced_output_channel_num << std::endl;
//                    OutFile << "    pool_rep_key_input_channel_index_current:" << pool_rep_key_input_channel_index_current << std::endl; // 新增的字段
//                    OutFile << "    input_channel_index:" << input_channel_index << std::endl;
//                    OutFile << "    POOL End!" << std::endl;
                    pool_rep_produce_output_channel_num[next_node_index][k]++;

                    int input_channel_index_for_next = pool_rep_produced_output_channel_num + pool_rep_min_max_output_channel_index[next_node_index][k][0];
                    CheckForIndex[next_node_index].insert(input_channel_index_for_next);
                    CheckForNum[next_node_index]++;
                    //// 考虑多个消费者
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveGatherDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 1);
                    }
                }
            }
            else //// 同样是笔记10-16的问题，有可能后续操作还没有完成，所以必须重复进入循环
            {
                //// 这里又是需要注意的！！对于类似resnet 58这种output尺寸为1*1，所以有的rep根本没任务，那它就不用进入下面的操作。
                if (pool_rep_min_max_output_channel_index[next_node_index][k][1] >= pool_rep_min_max_output_channel_index[next_node_index][k][0])
                {
                    int input_channel_index_for_next = pool_rep_produced_output_channel_num + pool_rep_min_max_output_channel_index[next_node_index][k][0] - 1;
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveGatherDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 0);
                    }
                }
            }
        }
    }
    else if (next_operation == "OP_ELTWISE" || next_operation == "OP_CONCAT" || next_operation == "OP_RELU" || next_operation == "OP_TANH" || next_operation == "OP_SIGMOID")
    {
        int replication_num = node_replication_num[next_node_index];
        int index_in_all_providers = node_provider_index_2_index_in_all_providers[next_node_index][this_node_index];
        for (int k = 0; k < replication_num; ++k)
        {
            int vec_rep_produced_output_channel_num = vec_rep_produce_output_channel_num[next_node_index][k];
            if (vec_rep_produced_output_channel_num < (vec_rep_min_max_channel_index[next_node_index][k][1] - vec_rep_min_max_channel_index[next_node_index][k][0] + 1))
            {
                //// 传输(传递给其他rep，因为需要分在各个rep处理的)
                if(vec_rep_min_max_channel_index[next_node_index][k][0] <= input_channel_index &&
                   vec_rep_min_max_channel_index[next_node_index][k][1] >= input_channel_index &&
                   vec_rep_prov_ready_input_channel_list[next_node_index][k][index_in_all_providers][input_channel_index] == 0)
                {
                    vec_rep_prov_ready_input_channel_list[next_node_index][k][index_in_all_providers][input_channel_index] = 1;
                }
                //// 再进行VEC操作
                int vec_rep_key_input_channel_index_current = vec_rep_produced_output_channel_num + vec_rep_min_max_channel_index[next_node_index][k][0];
                if (NewCheckVecPrepared(next_node_index, k, vec_rep_key_input_channel_index_current))
                {
//                    OutFile << std::endl;
//                    OutFile << "    " << next_operation << " Begin!" << std::endl;
//                    OutFile << "    node_index:" << next_node_index  << std::endl;
//                    OutFile << "    replication_index:" << k << std::endl;
//                    OutFile << "    rep_produced_output_channel_index:" << vec_rep_produced_output_channel_num << std::endl;
//                    OutFile << "    input_channel_index:" << input_channel_index << std::endl;

                    vec_rep_produce_output_channel_num[next_node_index][k]++;
//
                    int input_channel_index_for_next = vec_rep_produced_output_channel_num + vec_rep_min_max_channel_index[next_node_index][k][0];
                    CheckForIndex[next_node_index].insert(input_channel_index_for_next);
                    CheckForNum[next_node_index]++;
                    //// 考虑多个消费者
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveGatherDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 1);
                    }
                }
            }
            else //// 同样是笔记10-16的问题，有可能后续操作还没有完成，所以必须重复进入循环
            {
                if (vec_rep_min_max_channel_index[next_node_index][k][1] >= vec_rep_min_max_channel_index[next_node_index][k][0])
                {
                    int input_channel_index_for_next = vec_rep_produced_output_channel_num + vec_rep_min_max_channel_index[next_node_index][k][0] - 1;
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        std::string next_next_operation = PIMCOM_node_list[next_next_node_index].operation;
                        if (next_next_operation == "OP_ELTWISE" || next_next_operation == "OP_CONCAT" || next_next_operation == "OP_RELU" || next_next_operation == "OP_TANH" || next_next_operation == "OP_SIGMOID")
                            continue;
                        ScheduleNaiveGatherDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 0);
                    }
                }
            }
        }
    }
    else if (no_consider_node.count(next_operation))
    {
        for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
        {
            int next_next_node_index = *iter;
            ScheduleNaiveGatherDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index, complete_replication_index, effective_post);
        }
    }
    else if (next_operation == "OP_CONV" || next_operation == "OP_FC")
    {
        if (effective_post)
        {
            int related_AG_num = input_channel_related_AG_index[next_node_index][input_channel_index].size();
            for (int k = 0; k < related_AG_num; ++k)
            {
                int related_AG_index = input_channel_related_AG_index[next_node_index][input_channel_index][k];
                if (AG_ready_input_channel_list[related_AG_index][input_channel_index] != 1)
                {
                    AG_ready_input_channel_list[related_AG_index][input_channel_index] = 1;
                    if (AG_new_ready_input_channel_list[related_AG_index].count(complete_replication_index) == 0)
                    {
                        AG_new_ready_input_channel_list[related_AG_index][complete_replication_index].start_input_channel_index = input_channel_index;
                        AG_new_ready_input_channel_list[related_AG_index][complete_replication_index].ready_input_channel_num = 1;
                    }
                    else
                    {
                        AG_new_ready_input_channel_list[related_AG_index][complete_replication_index].ready_input_channel_num += 1;
                    }
                }
            }
        }
    }
    else
    {
//        std::cout << next_node_index << " " << next_operation << std::endl;
    }
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////// Dataflow Split  ///////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ElementPipelineSchedule::ScheduleNaiveSplitDataFlowMain(std::ofstream & OutFile,int instruction_group_index)
{
    OutFile << instruction_group_index << std::endl;
    int AG_num = PIMCOM_2_resource_info.AGs;
    for (int i = 0; i < AG_num; ++i)
    {
        int AG_index_in_replication = PIMCOM_4_element_AG_index_in_replication[i];
        if (AG_index_in_replication != 0)
            continue;
        struct AG_info_schedule thisAG = PIMCOM_4_element_AG_info_list[i];
        int node_index = thisAG.node_index;
        bool first_layer = thisAG.first_layer;
        int replication_index = thisAG.replication_index;

        //// 只需要考虑 AG_index_in_replication==0 的情况
        int produced_output_channel_num = node_rep_split_produce_output_channel_num[node_index][replication_index];
        if (produced_output_channel_num >= node_rep_split_key_input_channel_index_list[node_index][replication_index].size())
            produced_output_channel_num = node_rep_split_key_input_channel_index_list[node_index][replication_index].size();
        int key_input_channel_index = node_rep_split_key_input_channel_index_list[node_index][replication_index][produced_output_channel_num];
        if ( first_layer || node_rep_split_ready_input_channel_index_list[node_index][replication_index][key_input_channel_index] == 1 )
        {
            int consumer_index = *PIMCOM_4_topology_provider_consumer_relation[node_index].begin(); //// 一般情况CONV或FC都只有一个消费者
            //// 把"AG_rest_output_channel_num[AG_index_in_total] > 0"的判断设置在这里是因为当有的层结束后，后面还依赖于该层进入POST操作
            if (produced_output_channel_num < node_rep_split_output_channel_num_list[node_index][replication_index])
            {
                int output_channel_index = node_rep_split_output_channel_index_list[node_index][replication_index][produced_output_channel_num];
                OutFile << std::endl;
                OutFile << "    Normal Begin!" << std::endl;
                OutFile << "    node_index:" << node_index  << std::endl;
                OutFile << "    output_index:" << output_channel_index << std::endl;
                OutFile << "    Normal End" << std::endl << std::endl;

                CheckForIndex[node_index].insert(output_channel_index);
                node_rep_split_produce_output_channel_num[node_index][replication_index] ++;
                ScheduleNaiveSplitDataFlowPost(OutFile, instruction_group_index, node_index, consumer_index, output_channel_index, replication_index, 1);
            }
            else
            {
                int output_channel_index =  node_rep_split_output_channel_index_list[node_index][replication_index][produced_output_channel_num-1]; //// 已经算完的CONV-REP的output_channel_index，一直以最后一个output_channel_index进入后续处理
                ScheduleNaiveSplitDataFlowPost(OutFile, instruction_group_index, node_index, consumer_index, output_channel_index, replication_index, 0);
            }
        }
    }
}

void ElementPipelineSchedule::ScheduleNaiveSplitDataFlowPost(std::ofstream & OutFile, int instruction_group_index, int this_node_index, int next_node_index, int input_channel_index, int complete_replication_index, bool effective_post)
{
    std::string next_operation = PIMCOM_node_list[next_node_index].operation;
    if (next_operation == "OP_POOL")
    {
        int replication_num = node_replication_num[next_node_index];
        for (int k = 0; k < replication_num; ++k)
        {
            int produced_output_channel_num = node_rep_split_produce_output_channel_num[next_node_index][k];
            if (produced_output_channel_num < node_rep_split_output_channel_num_list[next_node_index][k])
            {
                int output_channel_index_in_total = node_rep_split_output_channel_index_list[next_node_index][k][produced_output_channel_num];
//                std::cout << "output_channel_index_in_total:" << output_channel_index_in_total << std::endl;
                //// 传输(传递给其他rep，因为池化是分在各个rep处理的)
//                if (next_node_index == 21 && k == 0 && input_channel_index > 600 && !(std::count(
//                        PIMCOM_conv_pool_input_output_info[next_node_index].output_index[output_channel_index_in_total].begin(),
//                        PIMCOM_conv_pool_input_output_info[next_node_index].output_index[output_channel_index_in_total].end(),input_channel_index)))
//                {
//                    std::cout << output_channel_index_in_total << "  " << input_channel_index << std::endl;
//                    for (int i = 0; i < PIMCOM_conv_pool_input_output_info[next_node_index].output_index[output_channel_index_in_total].size(); ++i)
//                    {
//                        std::cout << PIMCOM_conv_pool_input_output_info[next_node_index].output_index[output_channel_index_in_total][i] << " ";
//                    }
//                    std::cout << std::endl << std::endl;
//                }

//                if(std::count(
//                        PIMCOM_conv_pool_input_output_info[next_node_index].output_index[output_channel_index_in_total].begin(),
//                        PIMCOM_conv_pool_input_output_info[next_node_index].output_index[output_channel_index_in_total].end(),input_channel_index))
                {
                    node_rep_split_ready_input_channel_index_list[next_node_index][k][input_channel_index] = 1;
                }

                //// 再进行池化操作
                int key_input_channel_index = node_rep_split_key_input_channel_index_list[next_node_index][k][produced_output_channel_num];
//                std::cout << "key_input_channel_index:" << key_input_channel_index << std::endl;

                if (node_rep_split_ready_input_channel_index_list[next_node_index][k][key_input_channel_index] == 1)
                {
                    OutFile << std::endl;
                    OutFile << "    POOL Begin!" << std::endl;
                    OutFile << "    node_index:" << next_node_index  << std::endl;
                    OutFile << "    replication_index:" << k << std::endl;
                    OutFile << "    output_channel_index:" << output_channel_index_in_total << std::endl;
                    OutFile << "    pool_rep_produced_output_channel_index:" << produced_output_channel_num << std::endl;
                    OutFile << "    pool_rep_key_input_channel_index_current:" << key_input_channel_index << std::endl; // 新增的字段
                    OutFile << "    input_channel_index:" << input_channel_index << std::endl;
                    OutFile << "    POOL End!" << std::endl;

                    node_rep_split_produce_output_channel_num[next_node_index][k]++;

                    int input_channel_index_for_next = output_channel_index_in_total;
                    CheckForIndex[next_node_index].insert(input_channel_index_for_next);
                    //// 考虑多个消费者
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveSplitDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 1);
                    }
                }
            }
            else //// 同样是笔记10-16的问题，有可能后续操作还没有完成，所以必须重复进入循环
            {
                //// 这里又是需要注意的！！对于类似resnet 58这种output尺寸为1*1，所以有的rep根本没任务，那它就不用进入下面的操作。
                if (node_rep_split_output_channel_num_list[next_node_index][k] > 0)
                {
                    int input_channel_index_for_next = node_rep_split_output_channel_index_list[next_node_index][k][produced_output_channel_num - 1];
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveSplitDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 0);
                    }
                }
            }
        }
    }
    else if (next_operation == "OP_ELTWISE" || next_operation == "OP_CONCAT" || next_operation == "OP_RELU" || next_operation == "OP_TANH" || next_operation == "OP_SIGMOID")
    {
        int replication_num = node_replication_num[next_node_index];
        int index_in_all_providers = node_provider_index_2_index_in_all_providers[next_node_index][this_node_index];
        for (int k = 0; k < replication_num; ++k)
        {
            int vec_rep_produced_output_channel_num = node_rep_split_produce_output_channel_num[next_node_index][k];
            if (vec_rep_produced_output_channel_num < node_rep_split_output_channel_num_list[next_node_index][k])
            {
                //// 传输(传递给其他rep，因为需要分在各个rep处理的)
//                if(vec_rep_min_max_channel_index[next_node_index][k][0] <= input_channel_index &&
//                   vec_rep_min_max_channel_index[next_node_index][k][1] >= input_channel_index &&
//                   vec_rep_prov_ready_input_channel_list[next_node_index][k][index_in_all_providers][input_channel_index] == 0)
                {
//                    vec_rep_prov_ready_input_channel_list[next_node_index][k][index_in_all_providers][input_channel_index] = 1;
                    node_rep_split_ready_input_channel_index_list[next_node_index][k][input_channel_index] = 1;
                }
                //// 再进行VEC操作
                int vec_rep_key_input_channel_index_current = node_rep_split_output_channel_index_list[next_node_index][k][vec_rep_produced_output_channel_num];

                if (node_rep_split_ready_input_channel_index_list[next_node_index][k][vec_rep_key_input_channel_index_current] == 1)
                {
                    OutFile << std::endl;
                    OutFile << "    " << next_operation << " Begin!" << std::endl;
                    OutFile << "    node_index:" << next_node_index  << std::endl;
                    OutFile << "    replication_index:" << k << std::endl;
                    OutFile << "    rep_produced_output_channel_index:" << vec_rep_produced_output_channel_num << std::endl;
                    OutFile << "    input_channel_index:" << input_channel_index << std::endl;

                    node_rep_split_produce_output_channel_num[next_node_index][k]++;

                    //
                    int input_channel_index_for_next = input_channel_index;
                    CheckForIndex[next_node_index].insert(input_channel_index_for_next);
                    //// 考虑多个消费者
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveSplitDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 1);
                    }
                }
            }
            else //// 同样是笔记10-16的问题，有可能后续操作还没有完成，所以必须重复进入循环
            {
                if (node_rep_split_output_channel_num_list[next_node_index][k] > 0)
                {
                    int input_channel_index_for_next = node_rep_split_output_channel_index_list[next_node_index][k][vec_rep_produced_output_channel_num - 1];
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveSplitDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 0);
                    }
                }
            }
        }
    }
    else if (no_consider_node.count(next_operation))
    {
        for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
        {
            int next_next_node_index = *iter;
            ScheduleNaiveSplitDataFlowPost(OutFile, instruction_group_index, next_node_index, next_next_node_index, input_channel_index, complete_replication_index, effective_post);
        }
    }
    else if (next_operation == "OP_CONV" || next_operation == "OP_FC")
    {
//        if (effective_post)
//        {
//            int related_AG_num = input_channel_related_AG_index[next_node_index][input_channel_index].size();
//            for (int k = 0; k < related_AG_num; ++k)
//            {
//                int related_AG_index = input_channel_related_AG_index[next_node_index][input_channel_index][k];
//                if (AG_ready_input_channel_list[related_AG_index][input_channel_index] != 1)
//                {
//                    AG_ready_input_channel_list[related_AG_index][input_channel_index] = 1;
//                }
//            }
//        }
        for (int i = 0; i < node_rep_split_ready_input_channel_index_list[next_node_index].size(); ++i)
        {
//            std::cout << next_node_index << " " << i << " " << input_channel_index << std::endl;
            node_rep_split_ready_input_channel_index_list[next_node_index][i][input_channel_index] = 1;
        }
    }
    else
    {
//        std::cout << next_node_index << " " << next_operation << std::endl;
    }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Instruction (Detail) ////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionPost(int instruction_group_index, int this_node_index, int next_node_index, int input_channel_index, int complete_replication_index, bool append_instruction)
{
    std::string next_operation = PIMCOM_node_list[next_node_index].operation;
    if (next_operation == "OP_POOL")
    {
        int replication_num = node_replication_num[next_node_index];
        for (int k = 0; k < replication_num; ++k)
        {
            int pool_rep_produced_output_channel_num = pool_rep_produce_output_channel_num[next_node_index][k];
            if ( (pool_rep_produced_output_channel_num < (pool_rep_min_max_output_channel_index[next_node_index][k][1] - pool_rep_min_max_output_channel_index[next_node_index][k][0] + 1)))
            {
                //// 传输(传递给其他rep，因为池化是分在各个rep处理的)
                if(pool_rep_min_max_input_channel_index[next_node_index][k][0] <= input_channel_index &&
                   pool_rep_min_max_input_channel_index[next_node_index][k][1] >= input_channel_index &&
                   pool_rep_ready_input_channel_list[next_node_index][k][input_channel_index] == 0)
                {
                    pool_rep_ready_input_channel_list[next_node_index][k][input_channel_index] = 1; //// 这里的设置和以前的作用不同。详情见10-16笔记。是个比较复杂的问题。
                    if (pool_rep_new_ready_input_channel_list[next_node_index][k].count(complete_replication_index) == 0)
                    {
                        pool_rep_new_ready_input_channel_list[next_node_index][k][complete_replication_index].start_input_channel_index = input_channel_index;
                        pool_rep_new_ready_input_channel_list[next_node_index][k][complete_replication_index].ready_input_channel_num = 1;
                    }
                    else
                    {
                        pool_rep_new_ready_input_channel_list[next_node_index][k][complete_replication_index].ready_input_channel_num += 1;
                    }

                    //// 从AG_index_in_total到related_AG_index的数据传递
                    int effective_provider_node_index = PIMCOM_node_list[next_node_index].AG0_node_index;
                    int recv_AG_index = node_AG0_index_in_replication[effective_provider_node_index][k];
                    int send_AG_index = node_AG0_index_in_replication[effective_provider_node_index][complete_replication_index];
                    ScheduleNaiveGatherInstructionCOMM(instruction_group_index, send_AG_index, recv_AG_index);
                }
                //// 再进行池化操作
                int pool_rep_key_input_channel_index_current = pool_rep_key_input_channel_index[next_node_index][k][pool_rep_produced_output_channel_num];
                if (NewCheckPoolRepPrepared(next_node_index, k, pool_rep_key_input_channel_index_current))
                {
                    //// 计算这是整个pool层的第多少个输出
                    int input_channel_index_for_next = pool_rep_produced_output_channel_num + pool_rep_min_max_output_channel_index[next_node_index][k][0];
                    //// 进行池化操作
                    int effective_provider_node_index = PIMCOM_node_list[next_node_index].AG0_node_index;
                    int execution_AG_index = node_AG0_index_in_replication[effective_provider_node_index][k];

//                    ScheduleNaiveGatherInstructionStage4Pool(instruction_group_index, next_node_index, execution_AG_index, input_channel_index_for_next);

                    CheckForIndex[next_node_index].insert(input_channel_index_for_next);
                    CheckForNum[next_node_index]++;
                    pool_rep_produce_output_channel_num[next_node_index][k]++;

                    //// 把池化结果向后续传递，考虑多个消费者
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveGatherInstructionPost( instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 1);
                    }
                }
            }
            else //// 同样是笔记10-16的问题，有可能后续操作还没有完成，所以必须重复进入循环
            {
                //// 这里又是需要注意的！！对于类似resnet 58这种output尺寸为1*1，所以有的rep根本没任务，那它就不用进入下面的操作。
                if (pool_rep_min_max_output_channel_index[next_node_index][k][1] >= pool_rep_min_max_output_channel_index[next_node_index][k][0])
                {
                    int input_channel_index_for_next = pool_rep_produced_output_channel_num + pool_rep_min_max_output_channel_index[next_node_index][k][0] - 1;
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveGatherInstructionPost(instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 0);
                    }
                }
            }
        }
    }
    else if (next_operation == "OP_ELTWISE" || next_operation == "OP_CONCAT" || next_operation == "OP_RELU" || next_operation == "OP_TANH" || next_operation == "OP_SIGMOID")
    {
        int replication_num = node_replication_num[next_node_index];
        int index_in_all_providers = node_provider_index_2_index_in_all_providers[next_node_index][this_node_index];
        for (int k = 0; k < replication_num; ++k)
        {
            int vec_rep_produced_output_channel_num = vec_rep_produce_output_channel_num[next_node_index][k];
            if (vec_rep_produced_output_channel_num < (vec_rep_min_max_channel_index[next_node_index][k][1] - vec_rep_min_max_channel_index[next_node_index][k][0] + 1))
            {
                //// 传输(传递给其他rep，因为需要分在各个rep处理的)
                if(vec_rep_min_max_channel_index[next_node_index][k][0] <= input_channel_index &&
                   vec_rep_min_max_channel_index[next_node_index][k][1] >= input_channel_index &&
                   vec_rep_prov_ready_input_channel_list[next_node_index][k][index_in_all_providers][input_channel_index] == 0)
                {
                    vec_rep_prov_ready_input_channel_list[next_node_index][k][index_in_all_providers][input_channel_index] = 1;
                    //// next_node_index的this_node_index的生产者是在第complete_replication_index个rep上产生的第input_channel_index个输入通道。是这么一个意思。这个结构是为了后续生成指令的地址（rep中AG0的序号）
                    vec_prov_rep_index[next_node_index][index_in_all_providers][input_channel_index] = complete_replication_index;

                    //// 从AG_index_in_total到related_AG_index的数据传递
                    int effective_provider_node_index = PIMCOM_node_list[next_node_index].AG0_node_index;
                    int recv_AG_index = node_AG0_index_in_replication[effective_provider_node_index][k];
                    int send_AG_index = node_AG0_index_in_replication[effective_provider_node_index][complete_replication_index];
                    ScheduleNaiveGatherInstructionCOMM(instruction_group_index, send_AG_index, recv_AG_index);
                }
                //// 再进行VEC操作
                int vec_rep_key_input_channel_index_current = vec_rep_produced_output_channel_num + vec_rep_min_max_channel_index[next_node_index][k][0];
                if (NewCheckVecPrepared(next_node_index, k, vec_rep_key_input_channel_index_current))
                {
                    int execution_AG_index = node_AG0_index_in_replication[PIMCOM_node_list[next_node_index].AG0_node_index][k];
                    int input_channel_index_for_next = vec_rep_produced_output_channel_num + vec_rep_min_max_channel_index[next_node_index][k][0];

//                    if (next_operation == "OP_ELTWISE")
//                    {
//                        ScheduleNaiveGatherInstructionStage4Eltwise(instruction_group_index, next_node_index, execution_AG_index, input_channel_index_for_next);
//                    }
//                    else if (next_operation == "OP_CONCAT")
//                    {
//                        ScheduleNaiveGatherInstructionStage4Concat(instruction_group_index, next_node_index, execution_AG_index, input_channel_index_for_next);
//                    }
//                    else if(next_operation == "OP_RELU" || next_operation == "OP_TANH" || next_operation == "OP_SIGMOID")
//                    {
//                        ScheduleNaiveGatherInstructionStage4Activate(instruction_group_index, next_node_index, execution_AG_index, input_channel_index_for_next);
//                    }

                    vec_rep_produce_output_channel_num[next_node_index][k]++;
                    CheckForIndex[next_node_index].insert(input_channel_index_for_next);
                    CheckForNum[next_node_index]++;
                    //// 考虑多个消费者
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        ScheduleNaiveGatherInstructionPost(instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 1);
                    }
                }
            }
            else //// 同样是笔记10-16的问题，有可能后续操作还没有完成，所以必须重复进入循环
            {
                if (vec_rep_min_max_channel_index[next_node_index][k][1] >= vec_rep_min_max_channel_index[next_node_index][k][0])
                {
                    int input_channel_index_for_next = vec_rep_produced_output_channel_num + vec_rep_min_max_channel_index[next_node_index][k][0] - 1;
                    for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
                    {
                        int next_next_node_index = *iter;
                        std::string next_next_operation = PIMCOM_node_list[next_next_node_index].operation;
                        if (next_next_operation == "OP_ELTWISE" || next_next_operation == "OP_CONCAT" || next_next_operation == "OP_RELU" || next_next_operation == "OP_TANH" || next_next_operation == "OP_SIGMOID")
                            continue;
                        ScheduleNaiveGatherInstructionPost(instruction_group_index, next_node_index, next_next_node_index, input_channel_index_for_next, k, 0);
                    }
                }
            }
        }
    }
    else if (no_consider_node.count(next_operation))
    {
        for (auto iter = PIMCOM_4_topology_provider_consumer_relation[next_node_index].begin(); iter != PIMCOM_4_topology_provider_consumer_relation[next_node_index].end() ; ++iter)
        {
            int next_next_node_index = *iter;
            ScheduleNaiveGatherInstructionPost(instruction_group_index, next_node_index, next_next_node_index, input_channel_index, complete_replication_index, append_instruction);
        }
    }
    else if (next_operation == "OP_CONV" || next_operation == "OP_FC")
    {
        if (append_instruction)
        {
            int related_AG_num = input_channel_related_AG_index[next_node_index][input_channel_index].size();
            for (int k = 0; k < related_AG_num; ++k)
            {
                int related_AG_index = input_channel_related_AG_index[next_node_index][input_channel_index][k];
                if (AG_ready_input_channel_list[related_AG_index][input_channel_index] != 1)
                {
                    AG_ready_input_channel_list[related_AG_index][input_channel_index] = 1;
                    if (AG_new_ready_input_channel_list[related_AG_index].count(complete_replication_index) == 0)
                    {
                        AG_new_ready_input_channel_list[related_AG_index][complete_replication_index].start_input_channel_index = input_channel_index;
                        AG_new_ready_input_channel_list[related_AG_index][complete_replication_index].ready_input_channel_num = 1;
                    }
                    else
                    {
                        AG_new_ready_input_channel_list[related_AG_index][complete_replication_index].ready_input_channel_num += 1;
                    }
                }
                //// 传递给后续层
                int send_AG_index = node_AG0_index_in_replication[next_node_index][complete_replication_index];
                ScheduleNaiveGatherInstructionCOMM(instruction_group_index, send_AG_index, related_AG_index);
            }
        }
    }
    else
    {
//        std::cout << next_node_index << " " << next_operation << std::endl;
    }
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionMain(int instruction_group_index)
{
    int AG_num = PIMCOM_2_resource_info.AGs;
    for (int i = 0; i < AG_num; ++i)
    {
        int AG_index_in_replication = PIMCOM_4_element_AG_index_in_replication[i];
        if (AG_index_in_replication != 0)
            continue;
        struct AG_info_schedule thisAG = PIMCOM_4_element_AG_info_list[i];
        int AG_index_in_total = thisAG.AG_index_in_total;
        int input_cycle_this_replication_start = thisAG.input_cycle_this_replication_start;
        int node_index = thisAG.node_index;
        int AG_num_this_replication = thisAG.AG_num_per_replication;
        bool first_layer = thisAG.first_layer;
        int replication_index = thisAG.replication_index;

        //// 一个node提前终止的判断
        bool Process = false;
        if (early_termination[node_index].size() == 0) //// early_termination[node_index].size() == 0说明该CONV/FC后面无"后续操作"（不包括紧跟的relu）
        {
            //// 那么只要该节点完成了所有操作，以后就可以不再遍历该节点的AG
            if (CheckForIndex[node_index].size() != node_output_channel_num[node_index] || CheckForNum[node_index] != node_output_channel_num[node_index])
                Process = true;
        }
        else  //// early_termination[node_index].size() != 0说明该CONV/FC后面有后续操作（不包括紧跟的relu，而是说pool、concat之类）
        {
            //// 如果全部后续操作都进行完成了，那么可以不再遍历该节点的AG
            for (auto iter = early_termination[node_index].begin(); iter != early_termination[node_index].end(); iter++)
            {
                if (CheckForIndex[*iter].size() != node_output_channel_num[*iter] || CheckForNum[*iter] != node_output_channel_num[*iter])
                    Process = true;
            }
        }
        if (!Process)
        {
            continue;
        }

        //// 只需要考虑 AG_index_in_replication==0 的情况
        if ( first_layer ||  NewCheckNormalPrepared(AG_index_in_total) )
        {
            int consumer_index = *PIMCOM_4_topology_provider_consumer_relation[node_index].begin(); //// 一般情况CONV或FC都只有一个消费者
            //// 把"AG_rest_output_channel_num[AG_index_in_total] > 0"的判断设置在这里是因为当有的层结束后，后面还依赖于该层进入POST操作
            if (AG_rest_output_channel_num[AG_index_in_total] > 0)
            {
//                int produced_output_channel_num = AG_produce_output_channel_num[AG_index_in_total];
//                int key_input_channel_index = AG_key_input_channel_index[AG_index_in_total][produced_output_channel_num];
                int output_channel_index = input_cycle_this_replication_start + AG_produce_output_channel_num[AG_index_in_total];
                ////////////////////////////////////// Stage1、Stage2、Stage3 And ACT //////////////////////////////////////
                ScheduleNaiveGatherInstructionStage1MVMUL(instruction_group_index, AG_index_in_total, AG_num_this_replication, output_channel_index);

                ScheduleNaiveGatherInstructionStage2VADD(instruction_group_index, AG_index_in_total, AG_num_this_replication);

                ScheduleNaiveGatherInstructionStage3ACC(instruction_group_index, AG_index_in_total, AG_num_this_replication);

                ScheduleNaiveGatherInstructionStage3ACT(instruction_group_index, AG_index_in_total);

                CheckForIndex[node_index].insert(output_channel_index);
                CheckForNum[node_index]++;
                AG_rest_output_channel_num[AG_index_in_total] -= 1;
                AG_produce_output_channel_num[AG_index_in_total] += 1;

                ScheduleNaiveGatherInstructionPost(instruction_group_index, node_index, consumer_index, output_channel_index, replication_index, 1);
            }
            else
            {
                // TODO 这里有一点要注意：最后一个节点（consumer_num==0）的consumer设置为0，也就是会回到0，但是此时0肯定已经完成了。所以最终能顺利结束。
                int output_channel_index = input_cycle_this_replication_start + AG_produce_output_channel_num[AG_index_in_total] - 1; //// 已经算完的CONV-REP的output_channel_index，一直以最后一个output_channel_index进入后续处理
                ScheduleNaiveGatherInstructionPost(instruction_group_index, node_index, consumer_index, output_channel_index, replication_index, 0);
            }
        }
    }
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionStage1MVMUL(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication, int input_cycle_index)
{
    int node_index = PIMCOM_4_element_AG_info_list[start_AG_index_in_total].node_index;
    int level_index = PIMCOM_4_element_AG_info_list[start_AG_index_in_total].level_index;
    int replication_index = PIMCOM_4_element_AG_info_list[start_AG_index_in_total].replication_index;
    //// 首先为每个AG生成MVMUL操作
    for (int i = 0; i < AG_num_this_replication; ++i)
    {
        int AG_index_in_total = start_AG_index_in_total + i;
        int input_element_num = PIMCOM_4_element_AG_info_list[AG_index_in_total].input_element_num;
        int output_element_num = PIMCOM_4_element_AG_info_list[AG_index_in_total].output_element_num;
        int core_index = PIMCOM_4_element_AG_info_list[AG_index_in_total].core_index;

        struct INST Instruction;
        Instruction.type = MVMUL;
        Instruction.operation = "MVMUL";
        Instruction.input_cycle_index = input_cycle_index;
        Instruction.AG_index_in_total = AG_index_in_total;
        Instruction.replication_index = replication_index;
        Instruction.AG_index_in_replication = i;
        Instruction.conv_or_fc = PIMCOM_node_list[node_index].operation;
        Instruction.node_index = node_index;
        Instruction.destination = Instruction.AG_index_in_total;
        Instruction.source = Instruction.AG_index_in_total;
        Instruction.level_index = level_index;
        Instruction.input_element_num = input_element_num;
        Instruction.output_element_num = output_element_num;
        Instruction.rs_offset = 0;
//        Instruction.rd_offset = node_offset_inference[AG_index_in_total] * output_element_num + agp_offset;
        Instruction.rd_offset = 0;
        Instruction.instruction_group_index = instruction_group_index;
        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[core_index].instruction_ir_list.push_back(Instruction);
    }
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionStage2VADD(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication)
{
    std::map<int, std::vector<int>> core_AG_map; //core_AG_map[core_index]保存了一系列AG
    for (int i = 0; i < AG_num_this_replication; ++i)
    {
        int AG_index_in_total = i + start_AG_index_in_total;
        int core_index = PIMCOM_4_element_AG_info_list[AG_index_in_total].core_index;
        core_AG_map[core_index].push_back(AG_index_in_total);
        if (core_AG_map[core_index].size() > 1)
        {
            struct INST Instruction;
            Instruction.type = VEC2OP;
            Instruction.operation = "VADD";
            Instruction.destination = core_AG_map[core_index][0];
            Instruction.source_1 = core_AG_map[core_index][0];
            Instruction.source_2 = AG_index_in_total;
            Instruction.level_index = PIMCOM_4_element_AG_info_list[AG_index_in_total].level_index;
            Instruction.relative_length = 1;
            Instruction.element_num = Instruction.relative_length * PIMCOM_4_element_AG_info_list[AG_index_in_total].output_element_num; // 同一rep的output element num一致
            Instruction.instruction_group_index = instruction_group_index;
//            Instruction.rd_offset = (node_offset_inference[AG_index_in_total] - 1)* Instruction.element_num;
//            Instruction.rs1_offset = Instruction.rd_offset;
//            Instruction.rs2_offset = Instruction.rd_offset;
            Instruction.rd_offset = 0;
            Instruction.rs1_offset = 0;
            Instruction.rs2_offset = 0;
            PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[core_index].instruction_ir_list.push_back(Instruction);
        }

    }
}

static int comm_index = 0; //// SEND/RECV对的编号。为了后续评估模型而设置。
void ElementPipelineSchedule::ScheduleNaiveGatherInstructionStage3ACC(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication)
{
    int RecvCore = PIMCOM_4_element_AG_info_list[start_AG_index_in_total].core_index;
    int level_index = PIMCOM_4_element_AG_info_list[start_AG_index_in_total].level_index;

    std::map<int, int> core_AG_map; //core_AG_map[core_index]保存了第一个AG，也就是前面累加结果的那个AG
    core_AG_map[RecvCore] = start_AG_index_in_total;
    for (int i = 1; i < AG_num_this_replication; ++i)
    {
        int AG_index_in_total = i + start_AG_index_in_total;
        int current_core = PIMCOM_4_element_AG_info_list[AG_index_in_total].core_index;
        int output_element_num = PIMCOM_4_element_AG_info_list[AG_index_in_total].output_element_num;
        if (core_AG_map.count(current_core) == 0)
        {
            struct INST Instruction_send;
            Instruction_send.type = COMM;
            Instruction_send.level_index = level_index;
            Instruction_send.operation = "SEND";
            Instruction_send.to_core = RecvCore;
            Instruction_send.source = AG_index_in_total;
            Instruction_send.relative_length = 1;
            Instruction_send.element_num = Instruction_send.relative_length * output_element_num;
            Instruction_send.instruction_group_index = instruction_group_index;
            Instruction_send.comm_index = comm_index;
            Instruction_send.instruction_index_in_core = PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[current_core].instruction_ir_list.size();
            PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[current_core].instruction_ir_list.push_back(Instruction_send);

            struct INST Instruction_recv;
            Instruction_recv.type = COMM;
            Instruction_recv.level_index = level_index;
            Instruction_recv.operation = "RECV";
            Instruction_recv.from_core = current_core;
            Instruction_recv.destination = AG_index_in_total;
            Instruction_recv.relative_length = 1;
            Instruction_recv.element_num = Instruction_recv.relative_length * output_element_num;
            Instruction_recv.instruction_group_index = instruction_group_index;
            Instruction_recv.comm_index = comm_index;
            Instruction_recv.instruction_index_in_core = PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[RecvCore].instruction_ir_list.size();
            PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[RecvCore].instruction_ir_list.push_back(Instruction_recv);

            struct INST Instruction_vadd;
            Instruction_vadd.type = VEC2OP;
            Instruction_vadd.level_index = level_index;
            Instruction_vadd.operation = "VADD";
            Instruction_vadd.source_1 = start_AG_index_in_total;
            Instruction_vadd.source_2 = AG_index_in_total;
            Instruction_vadd.destination = start_AG_index_in_total;
//            Instruction_vadd.rd_offset = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[AG_index_in_total];
//            Instruction_vadd.rs1_offset = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[AG_index_in_total];
//            Instruction_vadd.rs2_offset = 0;
            Instruction_vadd.rd_offset = 0;
            Instruction_vadd.rs1_offset = 0;
            Instruction_vadd.rs2_offset = 0;
            Instruction_vadd.relative_length = 1;
            Instruction_vadd.element_num = Instruction_vadd.relative_length * output_element_num;
            Instruction_vadd.instruction_group_index = instruction_group_index;
            PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[RecvCore].instruction_ir_list.push_back(Instruction_vadd);

            comm_index++;
            core_AG_map[current_core] = AG_index_in_total;
        }
        else
        {
            continue;
        }
    }
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionStage3ACT(int instruction_group_index, int start_AG_index_in_total)
{
    int core_index = PIMCOM_4_element_AG_info_list[start_AG_index_in_total].core_index;
    int level_index = PIMCOM_4_element_AG_info_list[start_AG_index_in_total].level_index;
    int output_element_num = PIMCOM_4_element_AG_info_list[start_AG_index_in_total].output_element_num;
    struct INST Instruction_act;
    Instruction_act.type = VEC1OP;
    Instruction_act.level_index = level_index;
    Instruction_act.operation = "VRELU";
    Instruction_act.relative_length = 1;
    Instruction_act.source = start_AG_index_in_total;
    Instruction_act.destination = start_AG_index_in_total;
    Instruction_act.element_num = Instruction_act.relative_length * output_element_num;
    Instruction_act.instruction_group_index = instruction_group_index;
    Instruction_act.rd_offset = 0;
    Instruction_act.rs_offset = 0;
//    Instruction_act.rd_offset = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[Instruction_act.source];
//    Instruction_act.rs_offset = (node_offset_inference_old[AG_index_in_total])*AG_output_element_size[Instruction_act.source];
    PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[core_index].instruction_ir_list.push_back(Instruction_act);
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionCOMM(int instruction_group_index, int send_AG_index, int recv_AG_index)
{
    if (send_AG_index == recv_AG_index)
        return;
    int ToCore = PIMCOM_4_element_AG_info_list[recv_AG_index].core_index;
    int FromCore = PIMCOM_4_element_AG_info_list[send_AG_index].core_index;
    if (ToCore == FromCore)
        return;
    int level_index = PIMCOM_4_element_AG_info_list[send_AG_index].level_index;
    int element_num = PIMCOM_4_element_AG_info_list[send_AG_index].output_element_num;
    struct INST Instruction_send;
    Instruction_send.type = COMM;
    Instruction_send.level_index = level_index;
    Instruction_send.operation = "SEND";
    Instruction_send.to_core = ToCore;
    Instruction_send.source = send_AG_index;
    Instruction_send.relative_length = 1;
    Instruction_send.element_num = Instruction_send.relative_length * element_num;
    Instruction_send.instruction_group_index = instruction_group_index;
    Instruction_send.comm_index = comm_index;
    Instruction_send.instruction_index_in_core = PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[FromCore].instruction_ir_list.size();
    PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[FromCore].instruction_ir_list.push_back(Instruction_send);


    struct INST Instruction_recv;
    Instruction_recv.type = COMM;
    Instruction_recv.level_index = level_index;
    Instruction_recv.operation = "RECV";
    Instruction_recv.from_core = FromCore;
    Instruction_recv.destination = send_AG_index;
    Instruction_recv.relative_length = 1;
    Instruction_recv.element_num = Instruction_recv.relative_length * element_num;
    Instruction_recv.instruction_group_index = instruction_group_index;
    Instruction_recv.comm_index = comm_index;
    Instruction_recv.instruction_index_in_core = PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[ToCore].instruction_ir_list.size();
    PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[ToCore].instruction_ir_list.push_back(Instruction_recv);

    comm_index ++;
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionStage4Pool(int instruction_group_index, int pool_node_index, int execution_AG_index, int input_cycle_index)
{
    int input_channel_num = PIMCOM_conv_pool_input_output_info[pool_node_index].output_index[input_cycle_index].size();
    int execution_core = PIMCOM_4_element_AG_info_list[execution_AG_index].core_index;
    int element_num = PIMCOM_4_element_AG_info_list[execution_AG_index].output_element_num;
    int level_index = PIMCOM_4_element_AG_info_list[execution_AG_index].level_index;

    for (int i = 0; i < input_channel_num; ++i)
    {
        struct INST Instruction;
        Instruction.level_index = level_index;
        Instruction.stage = "POOL";
        Instruction.input_channel_index = PIMCOM_conv_pool_input_output_info[pool_node_index].output_index[input_cycle_index][i];
        Instruction.output_channel_index = input_cycle_index;
        Instruction.node_index = pool_node_index;
        Instruction.relative_length = 1;
        Instruction.element_num = Instruction.relative_length * element_num;
        if (i == 0)
        {
            Instruction.type = VEC1OP;
            Instruction.operation = "VM";
            Instruction.source = execution_AG_index;
            Instruction.destination = execution_AG_index;
//            Instruction.rs_offset = input_index * Instruction.element_num;
//            Instruction.rd_offset = input_element_in_total + output_index * Instruction.element_num;
            Instruction.rs_offset = 0;
            Instruction.rd_offset = 0;
        }
        else
        {
            Instruction.type = VEC2OP;
            Instruction.operation = "VVMAX";
            Instruction.source_1 = execution_AG_index;
            Instruction.source_2 = execution_AG_index;
            Instruction.destination = execution_AG_index;
//            Instruction.rs1_offset = input_index * Instruction.element_num;
//            Instruction.rs2_offset = input_element_in_total + input_index * Instruction.element_num;
//            Instruction.rd_offset = Instruction.rs2_offset;
            Instruction.rs1_offset = 0;
            Instruction.rs2_offset = 0;
            Instruction.rd_offset = 0;
        }
        //// 注意这里全都写进base
        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[execution_core].instruction_ir_list.push_back(Instruction);
    }
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionStage4Activate(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index)
{
    int execution_core = PIMCOM_4_element_AG_info_list[execution_AG_index].core_index;
    int element_num = PIMCOM_4_element_AG_info_list[execution_AG_index].output_element_num;

    std::string act_type;
    std::string consumer_op = PIMCOM_node_list[vec_node_index].operation;
    act_type = consumer_op == "OP_RELU" ? "VRELU" : (consumer_op == "OP_TANH" ? "VTANH" : "VSIGM");
    struct INST Instruction_act;
    Instruction_act.type = VEC1OP;
    Instruction_act.level_index = PIMCOM_node_list[vec_node_index].level_index;
    Instruction_act.operation = act_type;
    Instruction_act.output_channel_index = input_cycle_index;
    Instruction_act.node_index = vec_node_index;
    Instruction_act.source = execution_AG_index;
    Instruction_act.destination = execution_AG_index;
    Instruction_act.rs_offset = 0;
    Instruction_act.rd_offset = 0;
    Instruction_act.relative_length = 1;
    Instruction_act.element_num = Instruction_act.relative_length * element_num;
    // TODO copy_offset_flag在element流水线中同样有用
//    Instruction_act.copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
    PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[execution_core].instruction_ir_list.push_back(Instruction_act);
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionStage4Eltwise(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index)
{
    int execution_core = PIMCOM_4_element_AG_info_list[execution_AG_index].core_index;
    int element_num = PIMCOM_4_element_AG_info_list[execution_AG_index].output_element_num;
    int provider_num = PIMCOM_4_topology_consumer_provider_relation[vec_node_index].size();
    int elt_type = PIMCOM_node_list[vec_node_index].param.eletype;
    std::string elt_operation;
    switch (elt_type)
    {   case 2: elt_operation = "VADD"; break;
        case 4: elt_operation = "VSUB"; break;
    }

    int first_provider_index = PIMCOM_4_topology_consumer_provider_relation[vec_node_index][0];
    int index_in_all_providers1 = node_provider_index_2_index_in_all_providers[vec_node_index][first_provider_index];
    int rep1 = vec_prov_rep_index[vec_node_index][index_in_all_providers1][input_cycle_index];
    int AG1 = node_AG0_index_in_replication[PIMCOM_node_list[first_provider_index].AG0_node_index][rep1];
    for (int i = 1; i < provider_num; ++i)
    {
        int provider_index = PIMCOM_4_topology_consumer_provider_relation[vec_node_index][i];
        struct INST Instruction_elt;
        Instruction_elt.type = VEC2OP;
        Instruction_elt.level_index = PIMCOM_node_list[vec_node_index].level_index;
        Instruction_elt.operation = elt_operation;
        Instruction_elt.stage = "ELTWISE";
        Instruction_elt.node_index = vec_node_index;
        //// node_provider_index_2_index_in_all_providers，得到index为provider_index的生产者在vec_node_index中的生产者编号（0、1、2...）
        int index_in_all_providers2 = node_provider_index_2_index_in_all_providers[vec_node_index][provider_index];
        //// 得到vec_node_index中第index_in_all_providers2个生产者的第input_cycle_index的rep编号
        int rep2 = vec_prov_rep_index[vec_node_index][index_in_all_providers2][input_cycle_index];
        //// 得到该rep编号实际对应的AG编号。因为要以该AG编号作为地址，所以不得不需要它。
        int AG2 = node_AG0_index_in_replication[PIMCOM_node_list[provider_index].AG0_node_index][rep2];
        Instruction_elt.source_1 = AG1;
        Instruction_elt.source_2 = AG2;
        Instruction_elt.destination = AG1;
        Instruction_elt.rs1_offset = 0;
        Instruction_elt.rs2_offset = 0;
        Instruction_elt.rd_offset = 0;
        Instruction_elt.relative_length = 1;
        Instruction_elt.element_num = Instruction_elt.relative_length * element_num;
//        Instruction_elt.copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[execution_core].instruction_ir_list.push_back(Instruction_elt);
    }
}

void ElementPipelineSchedule::ScheduleNaiveGatherInstructionStage4Concat(int instruction_group_index, int vec_node_index, int execution_AG_index, int input_cycle_index)
{
    int execution_core = PIMCOM_4_element_AG_info_list[execution_AG_index].core_index;
    int provider_num = PIMCOM_4_topology_consumer_provider_relation[vec_node_index].size();
    int rd_offset = 0;
    for (int i = 0; i < provider_num; ++i)
    {
        int provider_index = PIMCOM_4_topology_consumer_provider_relation[vec_node_index][i];
        struct INST Instruction_concat;
        Instruction_concat.type = VEC1OP;
        Instruction_concat.level_index = PIMCOM_node_list[vec_node_index].level_index;
        Instruction_concat.operation = "VM";
        Instruction_concat.stage = "ELTWISE";
        Instruction_concat.node_index = vec_node_index;
        //// node_provider_index_2_index_in_all_providers，得到index为provider_index的生产者在vec_node_index中的生产者编号（0、1、2...）
        int index_in_all_providers = node_provider_index_2_index_in_all_providers[vec_node_index][provider_index];
        //// 得到vec_node_index中第index_in_all_providers个生产者的第input_cycle_index的rep编号
        int rep = vec_prov_rep_index[vec_node_index][index_in_all_providers][input_cycle_index];
        //// 得到该rep编号实际对应的AG编号。因为要以该AG编号作为地址，所以不得不需要它。
        int AG = node_AG0_index_in_replication[PIMCOM_node_list[provider_index].AG0_node_index][rep];
        int element_num = PIMCOM_4_element_AG_info_list[AG].output_element_num;
        Instruction_concat.source = AG;
        Instruction_concat.destination = execution_AG_index;
        Instruction_concat.relative_length = 1;
        Instruction_concat.element_num = Instruction_concat.relative_length * element_num;
        Instruction_concat.rs_offset = 0;
        Instruction_concat.rd_offset = rd_offset;
        rd_offset += element_num;
//        Instruction_concat.copy_offset_flag = PIMCOM_node_list[consumer_index].copy_offset_flag;
        PIMCOM_4_base_instruction_ir[instruction_group_index].core_list[execution_core].instruction_ir_list.push_back(Instruction_concat);
    }
}

void ElementPipelineSchedule::Check()
{
    std::cout << "================= Check Result =================" << std::endl;
    std::cout << "Node    Expected    Index     Num" << std::endl;
    bool pass = true;
    for (int i = 0; i < node_num; ++i)
    {
        //// 注意这里的逻辑，PIMCOM_4_topology_provider_consumer_relation中是包含了那些非CONV/FC后的relu，而no_consider_node包括了不考虑的操作
        //// 所以PIMCOM_4_topology_provider_consumer_relation[i].size() > 0是剔除了那些CONV/FC之后紧跟的relu
        if (!no_consider_node.count(PIMCOM_node_list[i].operation) && PIMCOM_4_topology_provider_consumer_relation[i].size() > 0)
        {
            std::cout << std::setw(3) << i << ":" << std::setw(10) <<  node_output_channel_num[i] << std::setw(10) << CheckForIndex[i].size() << std::setw(10) << CheckForNum[i] << std::endl;
            if (node_output_channel_num[i] != CheckForIndex[i].size() || CheckForIndex[i].size() != CheckForNum[i])
                pass = false;
        }
    }
    if (pass)
        std::cout << "================= PASS =================" << std::endl;
    else
        std::cout << "================= FAIL =================" << std::endl;
}

void ElementPipelineSchedule::ScheduleNaiveGatherDataFlow()
{
    std::ofstream  OutFile("../output/isaac.txt", std::ios::out | std::ios::trunc);
    int instruction_group_num = model_name == "vgg16" ? 35000 : 20000;
    for (int i = 0; i < instruction_group_num; ++i)
    {
        ElementPipelineSchedule::ScheduleNaiveGatherDataFlowMain(OutFile, i);
        if (last_node_index > 0 && CheckForIndex[last_node_index].size() == last_node_output_channel_num && CheckForNum[last_node_index] == last_node_output_channel_num)
            break;
    }
    OutFile.close();
}


void ElementPipelineSchedule::ScheduleNaiveGatherInstruction()
{
    int instruction_group_num = model_name == "vgg16" ? 50000 : 20000;
    PIMCOM_4_base_instruction_ir.resize(instruction_group_num);
    for (int i = 0; i < instruction_group_num; ++i)
    {
        ElementPipelineSchedule::ScheduleNaiveGatherInstructionMain(i);
        if (last_node_index > 0 && CheckForIndex[last_node_index].size() == last_node_output_channel_num && CheckForNum[last_node_index] == last_node_output_channel_num)
        {
            effective_instruction_group_num = i+1;
            break;
        }
    }
}

void ElementPipelineSchedule::ScheduleNaiveSplitDataFlow()
{
    std::ofstream  OutFile("../output/isaac-split.txt", std::ios::out | std::ios::trunc);
    for (int i = 0; i < 35000; ++i)
    {
        ElementPipelineSchedule::ScheduleNaiveSplitDataFlowMain(OutFile, i);
        if (last_node_index > 0 && CheckForIndex[last_node_index].size() == last_node_output_channel_num && CheckForNum[last_node_index] == last_node_output_channel_num)
            break;
    }
    OutFile.close();
}


void ElementPipelineSchedule::GetEarlyTermination()
{
    //// early_termination是一个用于加速的结构，它记录了一个CONV/FC到另一个CONV/FC之间的生产者消费者关系，用于提前完成判断。
    //// early_termination确保每个需要处理的后操作（concat、eltwise、非conv后的relu等）都有一个conv/fc的生产者。
    //// 因为整体的逻辑是不断遍历AG列表、通过CONV/FC进入数据流图
    //// 如果一个CONV/FC及其后续的后操作都进行完成，那么该CONV/FC就可以提前结束，以后不用再遍历它的AG。
    for (int i = 1; i < node_num; ++i)
    {
        std::string operation = PIMCOM_node_list[i].operation;
        if (operation != "OP_CONV" && operation != "OP_FC" && !no_consider_node.count(operation))
        {
            if (PIMCOM_4_topology_provider_consumer_relation[i].size())
            {
                int AG0_node_index = PIMCOM_node_list[i].AG0_node_index;
                early_termination[AG0_node_index].insert(i);
            }
        }
    }

//    std::cout << "!!!" << std::endl;
//    for (int i = 0; i < node_num; ++i)
//    {
//        for (auto iter = early_termination[i].begin(); iter != early_termination[i].end(); iter++)
//        {
//            std::cout << i << "  " << *iter << std::endl;
//        }
//    }
}

void ElementPipelineSchedule::ScheduleExecution()
{
    early_termination.resize(node_num);
    SchedulePreparation();
//    SavePreparation();
    CheckForIndex.resize(node_num);
    CheckForNum.resize(node_num);
    GetEarlyTermination();

    //// Dataflow Gather
//    ScheduleNaiveGatherDataFlow();

    //// Instruction Gather
    ScheduleNaiveGatherInstruction();

    //// Dataflow Split (不完善，还没debug结束，暂时不考虑了)
//    ScheduleNaiveSplitDataFlow();

    Check();
    Clear();
}


void ElementPipelineSchedule::Clear()
{
    node_AG_mapping.clear();
    node_AG0_index_in_replication.clear();
    node_replication_num.clear();
    node_provider_index_2_index_in_all_providers.clear();
    for (int & n : AG_rest_output_channel_num) {n = 0;}
    for (int & n : AG_produce_output_channel_num) {n = 0;}
    AG_key_input_channel_index.clear();
    AG_min_max_input_channel_index.clear();
    AG_ready_input_channel_list.clear();
    pool_rep_min_max_output_channel_index.clear();
    pool_rep_min_max_input_channel_index.clear();
    pool_rep_produce_output_channel_num.clear();
    pool_rep_key_input_channel_index.clear();
    pool_rep_new_ready_input_channel_list.clear();
    vec_rep_min_max_channel_index.clear();
    vec_rep_prov_ready_input_channel_list.clear();
    vec_rep_produce_output_channel_num.clear();
    vec_prov_rep_index.clear();
    AG_new_ready_input_channel_list.clear();
    input_channel_related_AG_index.clear();
    PIMCOM_4_element_AG_info_list.clear();
    PIMCOM_4_element_AG_index_in_replication.clear();
    PIMCOM_4_topology_provider_consumer_relation.clear();
    PIMCOM_4_topology_consumer_provider_relation.clear();
    input_channel_related_AG_index.clear();

    node_rep_split_output_channel_index_list.clear();
    node_rep_split_output_channel_num_list.clear();
    node_rep_split_ready_input_channel_index_list.clear();
    node_rep_split_produce_output_channel_num.clear();
    node_rep_split_key_input_channel_index_list.clear();

    comm_index = 0;
    no_consider_node.clear();
    early_termination.clear();
    node_output_channel_num.clear();
    CheckForIndex.clear();
    CheckForNum.clear();
}



void ElementPipelineSchedule::SaveInstruction()
{
    std::ofstream OutFile("../output/element_instruction.txt", std::ios::out | std::ios::trunc);
    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        OutFile << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;
        int instruction_group_num = PIMCOM_4_base_instruction_ir.size();
        for (int i = 0; i < effective_instruction_group_num; ++i)
        {
            OutFile << "========================================= base instruction_group " << i << " =========================================" << std::endl;
            for (int j = 0; j < core_num; ++j)
            {
                int instruction_num = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list.size();
                if (instruction_num == 0)
                    continue;
                OutFile << "core " << j << std::endl;
                for (int k = 0; k < instruction_num; ++k)
                {
                    struct INST Instruction = PIMCOM_4_base_instruction_ir[i].core_list[j].instruction_ir_list[k];
                    int instruction_level_index = Instruction.level_index;
                    if (instruction_level_index > inf)
                    {
                        continue;
                    }
                    SaveSingleInstructionFast(OutFile, Instruction, inf);
                }
            }
        }
    }
    OutFile.close();
}