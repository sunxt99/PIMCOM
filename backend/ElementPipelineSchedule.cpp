//
// Created by SXT on 2022/10/11.
//

#include "ElementPipelineSchedule.h"
ElementPipelineSchedule::ElementPipelineSchedule()
{
    core_num = PIMCOM_3_virtual_core_crossbar_map.size();
    node_num = PIMCOM_node_list.size();
}


std::vector<std::vector<int>> node_AG_mapping;               //// 记录每个节点有哪些AG
std::vector<std::vector<int>> node_AG0_index_in_replication; //// 记录每个节点若干个rep的AG0的AG_index_in_total

std::vector<std::vector<int>> AG_key_input_channel_index;
std::vector<std::vector<int>> AG_min_max_input_channel_index;
static int AG_produce_output_channel_num[MAX_AG] = {0};
std::vector<std::vector<int>> AG_ready_input_channel_list;
static int AG_rest_output_channel_num[MAX_AG] = {0};

struct ready_input_channel_info
{
    int start_input_channel_index;
    int ready_input_channel_num;
};
std::vector<std::map<int, struct ready_input_channel_info>> AG_new_ready_input_channel_list; //// AG_new_ready_input_channel_list[AG_index][rep_index] 这里的rep_index是前一层的rep_index，因为是前一层的不同rep向本AG来传递数据。

std::vector<std::vector<std::vector<int>>> pool_rep_min_max_output_channel_index; //// 输出任务的划分
std::vector<std::vector<std::vector<int>>> pool_rep_key_input_channel_index;
std::vector<std::vector<std::vector<int>>> pool_rep_min_max_input_channel_index;  //// 根据输出任务，不同rep需要存储不同的输入通道
std::vector<std::vector<int>> pool_rep_produce_output_channel_num;                //// 不同rep已经处理的pool操作数
std::vector<std::vector<std::vector<int>>> pool_rep_ready_input_channel_list;     //// 不同rep已经准备好的通道数目
std::vector<int> pool_rep_mapping; //// 记录每个pool前面的conv的replication_num
std::vector<std::vector<std::map<int, struct ready_input_channel_info>>> pool_rep_new_ready_input_channel_list;

std::vector<struct AG_info_schedule> PIMCOM_4_element_AG_info_list;
std::vector<int> PIMCOM_4_element_AG_index_in_replication;

void ElementPipelineSchedule::SchedulePreparation()
{
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


    //// 前面生成的生产者消费者关系不太对劲，缺少了29，这里修改一下
//    PIMCOM_4_provider_consumer_relation_with_pool[29].clear();
//    PIMCOM_4_provider_consumer_relation_with_pool[29].insert(31);
//    PIMCOM_4_provider_consumer_relation_with_pool[31].insert(33);
//    std::cout << "provider - consumer" << std::endl;
//    for (auto iter = PIMCOM_4_provider_consumer_relation_with_pool.begin(); iter != PIMCOM_4_provider_consumer_relation_with_pool.end() ; ++iter)
//    {
//        std::cout << iter->first << " :" ;
//        for (auto iter2 = iter->second.begin();  iter2 != iter->second.end() ; ++iter2)
//        {
//            std::cout << *iter2 << " " ;
//            PIMCOM_4_consumer_provider_relation_with_pool[*iter2].insert(iter->first);
//        }
//        std::cout << std::endl;
//    }
//    std::cout << "consumer - provider" << std::endl;
//    for (auto iter = PIMCOM_4_consumer_provider_relation_with_pool.begin(); iter != PIMCOM_4_consumer_provider_relation_with_pool.end() ; ++iter)
//    {
//        std::cout << iter->first << " :" ;
//        for (auto iter2 = iter->second.begin();  iter2 != iter->second.end() ; ++iter2)
//        {
//            std::cout << *iter2 << " " ;
//        }
//        std::cout << std::endl;
//    }
    //// 对于简单的直线拓扑也可以直接一些
    for (int i = 0; i < node_num; ++i)
    {
        if (PIMCOM_node_list[i].operation == "OP_CONV" || PIMCOM_node_list[i].operation == "OP_FC" || PIMCOM_node_list[i].operation == "OP_POOL")
        {
            int consumer_index = i+1;
            while(consumer_index < node_num && PIMCOM_node_list[consumer_index].operation != "OP_CONV" && PIMCOM_node_list[consumer_index].operation != "OP_FC" && PIMCOM_node_list[consumer_index].operation != "OP_POOL")
            {
                consumer_index++;
            }
            if (consumer_index < node_num)
            {
                PIMCOM_4_consumer_provider_relation_with_pool[consumer_index].insert(i);
                PIMCOM_4_provider_consumer_relation_with_pool[i].insert(consumer_index);
            }
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
                    AG_key_input_channel_index[AG_index].resize(input_cycle_end_this_replication - input_cycle_start_this_replication+1);
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
            // TODO 仍然只能处理VGG等简单数据流，所以只选择了begin的元素。未来可能还需要支持更多网络
            int effective_provider_node_index = *PIMCOM_4_consumer_provider_relation_with_pool[node_index].begin();
            //// 如果FC的前面是CONV，则key_channel_index是output_channel_num-1。意思是需要等前一个层把全部结果都穿过来之后再进行处理。而如果FC的前面是FC，则为0，只要前面FC传一次就够。
            int key_channel_index = 0;
            if (PIMCOM_node_list[effective_provider_node_index].operation == "OP_CONV" || PIMCOM_node_list[effective_provider_node_index].operation == "OP_POOL")
            {
                int output_channel_num = PIMCOM_node_list[effective_provider_node_index].output_dim[2] * PIMCOM_node_list[effective_provider_node_index].output_dim[3];
                key_channel_index = output_channel_num-1; // output_channel_index = output_channel_num - 1
            }
            else if (PIMCOM_node_list[effective_provider_node_index].operation == "OP_FC")
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
            int min_input_channel_index = GetInputChannelFromOutputIndex(node_index, input_cycle_start, 0);
            int max_input_channel_index = GetInputChannelFromOutputIndex(node_index, input_cycle_end, 1);
            AG_min_max_input_channel_index[i][0] = min_input_channel_index;
            AG_min_max_input_channel_index[i][1] = max_input_channel_index;
            AG_ready_input_channel_list[i].resize(input_channel_num);
        }
        else
        {
            AG_min_max_input_channel_index[i][0] = 0;
            AG_min_max_input_channel_index[i][1] = 50000;
            //// 这里size不设置成1是因为有的FC的前一层是CONV，需要接收完后才能开始FC的计算
            AG_ready_input_channel_list[i].resize(50000);
        }
        int effective_provider_index = *PIMCOM_4_effective_consumer_provider_relation[node_index].begin(); // TODO effective指的是conv和fc，一般只有一个生产者或消费者
//        int effect
        node_AG_mapping[node_index].push_back(AG_index);
    }

    //// 得到每个input_cycle_index所关联的AG
    GetRelatedAGIndex();


/////////////////////////////////////////////////// POOL-REP划分方式 ///////////////////////////////////////////////////
//////////                  pool_rep_min_max_output_channel_index;            /// 输出任务的划分
//////////                  pool_rep_key_input_channel_index;                 //// 不同rep、不同output_channel的关键输入通道
//////////                  pool_rep_min_max_input_channel_index;             //// 根据输出任务，不同rep需要存储不同的输入通道
//////////                  pool_rep_produce_output_channel_num;              //// 不同rep已经处理的pool操作数
//////////                  pool_rep_ready_input_channel_list;               / /// 不同rep已经准备好的通道数目

    //// 对不同rep的pool进行任务的划分
    pool_rep_min_max_output_channel_index.resize(node_num);
    pool_rep_min_max_input_channel_index.resize(node_num);
    pool_rep_mapping.resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        if (PIMCOM_node_list[i].operation == "OP_POOL")
        {
            int output_channel_num = PIMCOM_node_list[i].output_dim[2] * PIMCOM_node_list[i].output_dim[3]; // pool output channel num
            int provider_index = *PIMCOM_4_consumer_provider_relation_with_pool[i].begin();
            int replication_num = PIMCOM_node_list[provider_index].replication_num;
            pool_rep_mapping[i] = replication_num;
            pool_rep_min_max_output_channel_index[i].resize(replication_num);
            pool_rep_min_max_input_channel_index[i].resize(replication_num);
            std::vector<int> start_address_vector;
            std::vector<int> end_address_vector;
            DivideTheOutputChannel(start_address_vector, end_address_vector, output_channel_num, replication_num);
            for (int j = 0; j < replication_num; ++j)
            {
                pool_rep_min_max_output_channel_index[i][j].push_back(start_address_vector[j]); // start_output_index
                pool_rep_min_max_output_channel_index[i][j].push_back(end_address_vector[j]); // end_output_index
                int min_input_channel = GetInputChannelFromOutputIndex(i, start_address_vector[j], 0);
                int max_input_channel = GetInputChannelFromOutputIndex(i, end_address_vector[j], 1);
                pool_rep_min_max_input_channel_index[i][j].push_back(min_input_channel);
                pool_rep_min_max_input_channel_index[i][j].push_back(max_input_channel);
            }
        }
    }

    pool_rep_key_input_channel_index.resize(node_num);
    pool_rep_ready_input_channel_list.resize(node_num);
    pool_rep_new_ready_input_channel_list.resize(node_num);
    pool_rep_produce_output_channel_num.resize(node_num);
    for (int i = 0; i < node_num; ++i)
    {
        std::string operation = PIMCOM_node_list[i].operation;
        if (operation == "OP_POOL")
        {
            int input_channel_num = PIMCOM_node_list[i].input_dim[2] * PIMCOM_node_list[i].input_dim[3];
            int pre_rep_num = pool_rep_mapping[i]; // previous_conv_replication_num
            pool_rep_ready_input_channel_list[i].resize(pre_rep_num);
            pool_rep_new_ready_input_channel_list[i].resize(pre_rep_num);
            pool_rep_key_input_channel_index[i].resize(pre_rep_num);
            pool_rep_produce_output_channel_num[i].resize(pre_rep_num);
            for (int j = 0; j < pre_rep_num; ++j)
            {
                int output_channel_num = pool_rep_min_max_output_channel_index[i][j][1] - pool_rep_min_max_output_channel_index[i][j][0] + 1;
                pool_rep_ready_input_channel_list[i][j].resize(input_channel_num);
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
}

void ElementPipelineSchedule::DivideTheOutputChannel(std::vector<int> &start_address_vector, std::vector<int> &end_address_vector, int output_channel_num_total, int replication_num)
{
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

clock_t part_time_use = 0;

bool ElementPipelineSchedule::CheckNormalPrepared(int AG_index_in_total)
{
    int produced_output_channel_num = AG_produce_output_channel_num[AG_index_in_total];
    int key_input_channel_index = AG_key_input_channel_index[AG_index_in_total][produced_output_channel_num];
    int start = AG_min_max_input_channel_index[AG_index_in_total][0];
    for (int i = start; i <= key_input_channel_index; ++i)
    {
        if (AG_ready_input_channel_list[AG_index_in_total][i] != 1)
        {
            return false;
        }
    }
    return true;
}


bool ElementPipelineSchedule::NewCheckNormalPrepared(int AG_index_in_total)
{
    int produced_output_channel_num = AG_produce_output_channel_num[AG_index_in_total];
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
                return true;
            else
            {
                needed_start_index = this_rep_ready_end_input_channel_index;
            }
        }
    }
    return false;
}


bool ElementPipelineSchedule::CheckPoolRepPrepared(int node_index,int replication_index, int pool_key_input_channel_index_current)
{
    int start = pool_rep_min_max_input_channel_index[node_index][replication_index][0];
    for (int i = start; i <= pool_key_input_channel_index_current; ++i)
    {
        if (pool_rep_ready_input_channel_list[node_index][replication_index][i] != 1)
            return false;
    }
    return true;
}

bool ElementPipelineSchedule::NewCheckPoolRepPrepared(int node_index,int replication_index, int pool_key_input_channel_index_current)
{
    int needed_start_index = pool_rep_min_max_input_channel_index[node_index][replication_index][0];
    int needed_end_index = pool_key_input_channel_index_current + 1;

    for (auto iter = pool_rep_new_ready_input_channel_list[node_index][replication_index].begin(); iter != pool_rep_new_ready_input_channel_list[node_index][replication_index].end() ; ++iter)
    {
        int this_rep_ready_start_input_channel_index = iter->second.start_input_channel_index;
        int this_rep_ready_input_channel_num = iter->second.ready_input_channel_num;
        int this_rep_ready_end_input_channel_index = this_rep_ready_start_input_channel_index + this_rep_ready_input_channel_num;

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
    return false;
}


std::vector<int> NodeCheck;
void ElementPipelineSchedule::ScheduleNaiveAbstract(std::ofstream & OutFile,int instruction_group_index)
{
    OutFile << instruction_group_index << std::endl;
    int AG_num = PIMCOM_2_resource_info.AGs;
    for (int i = 0; i < AG_num; ++i)
    {
        int AG_index_in_replication = PIMCOM_4_element_AG_index_in_replication[i];
        if (AG_index_in_replication != 0)
            continue;
        struct AG_info_schedule thisAG = PIMCOM_4_element_AG_info_list[i];
        int AG_index_in_total = thisAG.AG_index_in_total;
        int replication_index = thisAG.replication_index;
        int input_cycle_this_replication_start = thisAG.input_cycle_this_replication_start;
        int node_index = thisAG.node_index;
        int replication_num = thisAG.replication_num;
        bool first_layer = thisAG.first_layer;

        //// 只需要考虑AG_index_in_replication==0的情况
        if (  ( first_layer && AG_rest_output_channel_num[AG_index_in_total] > 0) ||
              ( AG_rest_output_channel_num[AG_index_in_total] > 0 && NewCheckNormalPrepared(AG_index_in_total) ) )
        {
            int output_channel_index = input_cycle_this_replication_start + AG_produce_output_channel_num[AG_index_in_total];
            int consumer_index = *PIMCOM_4_provider_consumer_relation_with_pool[node_index].begin(); // TODO 目前只考虑VGG，只有一个消费者

//            OutFile << std::endl;
//            OutFile << "    Normal Begin!" << std::endl;
//            OutFile << "    node_index:" << node_index  << std::endl;
//            OutFile << "    output_index:" << output_channel_index << std::endl;
//            OutFile << "    Normal End" << std::endl << std::endl;
            NodeCheck[node_index]++;

            if (PIMCOM_node_list[consumer_index].operation == "OP_POOL")
            {
                for (int k = 0; k < replication_num; ++k)
                {
                    int pool_rep_produced_output_channel_num = pool_rep_produce_output_channel_num[consumer_index][k];
                    if (pool_rep_produced_output_channel_num >= (pool_rep_min_max_output_channel_index[consumer_index][k][1] - pool_rep_min_max_output_channel_index[consumer_index][k][0] + 1))
                        continue;
                    // 传输(传递个其他rep，因为池化是分在各个rep处理的)
                    if(pool_rep_min_max_input_channel_index[consumer_index][k][0] <= output_channel_index &&
                       pool_rep_min_max_input_channel_index[consumer_index][k][1] >= output_channel_index)
                    {
//                        pool_rep_ready_input_channel_list[consumer_index][k][output_channel_index] = 1; // k就是需要的核
                        if (pool_rep_new_ready_input_channel_list[consumer_index][k].count(replication_index) == 0)
                        {
                            pool_rep_new_ready_input_channel_list[consumer_index][k][replication_index].start_input_channel_index = output_channel_index;
                            pool_rep_new_ready_input_channel_list[consumer_index][k][replication_index].ready_input_channel_num = 1;
                        }
                        else
                        {
                            pool_rep_new_ready_input_channel_list[consumer_index][k][replication_index].ready_input_channel_num += 1;
                        }
                    }
                    // 再进行池化操作
                    int pool_rep_key_input_channel_index_current = pool_rep_key_input_channel_index[consumer_index][k][pool_rep_produced_output_channel_num];
                    if (NewCheckPoolRepPrepared(consumer_index, k, pool_rep_key_input_channel_index_current))
                    {
//                        OutFile << std::endl;
//                        OutFile << "    POOL Begin!" << std::endl;
//                        OutFile << "    node_index:" << consumer_index  << std::endl;
//                        OutFile << "    replication_index:" << k << std::endl;
//                        OutFile << "    pool_rep_produced_output_channel_index:" << pool_rep_produced_output_channel_num << std::endl;
//                        OutFile << "    input_channel_index:" << output_channel_index << std::endl;

                        int consumer_consumer_index = *PIMCOM_4_provider_consumer_relation_with_pool[consumer_index].begin();// TODO 目前只考虑VGG，只有一个消费者
                        int input_channel_index_for_next = pool_rep_produced_output_channel_num + pool_rep_min_max_output_channel_index[consumer_index][k][0];
                        int related_AG_num = input_channel_related_AG_index[consumer_consumer_index][input_channel_index_for_next].size();
                        for (int l = 0; l < related_AG_num; ++l)
                        {
                            int related_AG_index = input_channel_related_AG_index[consumer_consumer_index][input_channel_index_for_next][l];
//                            AG_ready_input_channel_list[related_AG_index][input_channel_index_for_next] = 1;
                            if (AG_new_ready_input_channel_list[related_AG_index].count(replication_index) == 0)
                            {
                                AG_new_ready_input_channel_list[related_AG_index][replication_index].start_input_channel_index = input_channel_index_for_next;
                                AG_new_ready_input_channel_list[related_AG_index][replication_index].ready_input_channel_num = 1;
                            }
                            else
                            {
                                AG_new_ready_input_channel_list[related_AG_index][replication_index].ready_input_channel_num += 1;
                            }
                        }
                        pool_rep_produce_output_channel_num[consumer_index][k]++;
//                        OutFile << "    POOL End!" << std::endl;
                    }
                }
            }
            else if ( (PIMCOM_node_list[consumer_index].operation == "OP_CONV" || PIMCOM_node_list[consumer_index].operation == "OP_FC"))
            {
                int related_AG_num = input_channel_related_AG_index[consumer_index][output_channel_index].size();
                for (int k = 0; k < related_AG_num; ++k)
                {
                    int related_AG_index = input_channel_related_AG_index[consumer_index][output_channel_index][k];
//                    AG_ready_input_channel_list[related_AG_index][output_channel_index] = 1; // 该层的输出通道index即下一层输入通道index
                    if (AG_new_ready_input_channel_list[related_AG_index].count(replication_index) == 0)
                    {
                        AG_new_ready_input_channel_list[related_AG_index][replication_index].start_input_channel_index = output_channel_index;
                        AG_new_ready_input_channel_list[related_AG_index][replication_index].ready_input_channel_num = 1;
                    }
                    else
                    {
                        AG_new_ready_input_channel_list[related_AG_index][replication_index].ready_input_channel_num += 1;
                    }
                }
            }
            AG_rest_output_channel_num[AG_index_in_total] -= 1;
            AG_produce_output_channel_num[AG_index_in_total] += 1;
        }
    }
}


void ElementPipelineSchedule::ScheduleNaiveMain(int instruction_group_index)
{
    int AG_num = PIMCOM_2_resource_info.AGs;
    for (int i = 0; i < AG_num; ++i)
    {
        int AG_index_in_replication = PIMCOM_4_element_AG_index_in_replication[i];
        if (AG_index_in_replication != 0)
            continue;
        struct AG_info_schedule thisAG = PIMCOM_4_element_AG_info_list[i];
        int AG_index_in_total = thisAG.AG_index_in_total;
        int replication_index = thisAG.replication_index;
        int input_cycle_this_replication_start = thisAG.input_cycle_this_replication_start;
        int node_index = thisAG.node_index;
        int replication_num = thisAG.replication_num;
        int AG_num_this_replication = thisAG.AG_num_per_replication;
        bool first_layer = thisAG.first_layer;

        //// 只需要考虑AG_index_in_replication==0的情况
        if (  ( first_layer && AG_rest_output_channel_num[AG_index_in_total] > 0) ||
              ( AG_rest_output_channel_num[AG_index_in_total] > 0 && NewCheckNormalPrepared(AG_index_in_total) ) )
        {
            int output_channel_index = input_cycle_this_replication_start + AG_produce_output_channel_num[AG_index_in_total];

            ////////////////////////////////////// Stage1、Stage2、Stage3 And ACT //////////////////////////////////////
            ScheduleNaiveStage1(instruction_group_index, AG_index_in_total, AG_num_this_replication, output_channel_index);

            ScheduleNaiveStage2(instruction_group_index, AG_index_in_total, AG_num_this_replication);

            ScheduleNaiveStage3(instruction_group_index, AG_index_in_total, AG_num_this_replication);

            ScheduleNaiveStageACT(instruction_group_index, AG_index_in_total);
            ////////////////////////////////////////////////// Stage4 //////////////////////////////////////////////////
            int consumer_index = *PIMCOM_4_provider_consumer_relation_with_pool[node_index].begin(); // TODO 目前只考虑VGG，只有一个消费者
            NodeCheck[node_index]++;
            if (PIMCOM_node_list[consumer_index].operation == "OP_POOL")
            {
                for (int k = 0; k < replication_num; ++k)
                {
                    int pool_rep_produced_output_channel_num = pool_rep_produce_output_channel_num[consumer_index][k];
                    if (pool_rep_produced_output_channel_num >= (pool_rep_min_max_output_channel_index[consumer_index][k][1] - pool_rep_min_max_output_channel_index[consumer_index][k][0] + 1))
                        continue;
                    // 传输(传递个其他rep，因为池化是分在各个rep处理的，每个rep处理一部分，保存在min_max中)
                    if(pool_rep_min_max_input_channel_index[consumer_index][k][0] <= output_channel_index &&
                       pool_rep_min_max_input_channel_index[consumer_index][k][1] >= output_channel_index)
                    {
                        // pool_rep_ready_input_channel_list[consumer_index][k][output_channel_index] = 1; //已弃用,速度太慢
                        if (pool_rep_new_ready_input_channel_list[consumer_index][k].count(replication_index) == 0)
                        {
                            // k就是需要的rep_index
                            pool_rep_new_ready_input_channel_list[consumer_index][k][replication_index].start_input_channel_index = output_channel_index;
                            pool_rep_new_ready_input_channel_list[consumer_index][k][replication_index].ready_input_channel_num = 1;
                        }
                        else
                        {
                            pool_rep_new_ready_input_channel_list[consumer_index][k][replication_index].ready_input_channel_num += 1;
                        }
                        //// 从AG_index_in_total到related_AG_index的数据传递
                        int recv_AG_index = node_AG0_index_in_replication[node_index][k];
                        ScheduleNaiveCOMM(instruction_group_index, AG_index_in_total, recv_AG_index);
                    }
                    // 再进行池化操作
                    int pool_rep_key_input_channel_index_current = pool_rep_key_input_channel_index[consumer_index][k][pool_rep_produced_output_channel_num];
                    if (NewCheckPoolRepPrepared(consumer_index, k, pool_rep_key_input_channel_index_current))
                    {
                        //// 计算这是整个pool层的第多少个输出
                        int input_channel_index_for_next = pool_rep_produced_output_channel_num + pool_rep_min_max_output_channel_index[consumer_index][k][0];
                        //// 进行池化操作
                        int execution_AG_index = node_AG0_index_in_replication[node_index][k];
//                        ScheduleNaiveStage4Pool(instruction_group_index, consumer_index, execution_AG_index, input_channel_index_for_next);
                        //// 把池化结果向后续传递
                        int consumer_consumer_index = *PIMCOM_4_provider_consumer_relation_with_pool[consumer_index].begin();// TODO 目前只考虑VGG，只有一个消费者
                        int related_AG_num = input_channel_related_AG_index[consumer_consumer_index][input_channel_index_for_next].size();
                        for (int l = 0; l < related_AG_num; ++l)
                        {
                            int related_AG_index = input_channel_related_AG_index[consumer_consumer_index][input_channel_index_for_next][l];
                            // AG_ready_input_channel_list[related_AG_index][input_channel_index_for_next] = 1; //已弃用,速度太慢
                            if (AG_new_ready_input_channel_list[related_AG_index].count(replication_index) == 0)
                            {
                                AG_new_ready_input_channel_list[related_AG_index][replication_index].start_input_channel_index = input_channel_index_for_next;
                                AG_new_ready_input_channel_list[related_AG_index][replication_index].ready_input_channel_num = 1;
                            }
                            else
                            {
                                AG_new_ready_input_channel_list[related_AG_index][replication_index].ready_input_channel_num += 1;
                            }
                            int send_AG_index = node_AG0_index_in_replication[node_index][k];
                            ScheduleNaiveCOMM(instruction_group_index, send_AG_index, related_AG_index);
                        }
                        pool_rep_produce_output_channel_num[consumer_index][k]++;
                    }
                }
            }
            else if ( (PIMCOM_node_list[consumer_index].operation == "OP_CONV" || PIMCOM_node_list[consumer_index].operation == "OP_FC"))
            {
                int related_AG_num = input_channel_related_AG_index[consumer_index][output_channel_index].size();
                for (int k = 0; k < related_AG_num; ++k)
                {
                    int related_AG_index = input_channel_related_AG_index[consumer_index][output_channel_index][k];
//                    AG_ready_input_channel_list[related_AG_index][output_channel_index] = 1; // 该层的输出通道index即下一层输入通道index
                    if (AG_new_ready_input_channel_list[related_AG_index].count(replication_index) == 0)
                    {
                        AG_new_ready_input_channel_list[related_AG_index][replication_index].start_input_channel_index = output_channel_index;
                        AG_new_ready_input_channel_list[related_AG_index][replication_index].ready_input_channel_num = 1;
                    }
                    else
                    {
                        AG_new_ready_input_channel_list[related_AG_index][replication_index].ready_input_channel_num += 1;
                    }
                    //// 传递给后续层
                    ScheduleNaiveCOMM(instruction_group_index, AG_index_in_total, related_AG_index);
                }
            }
            AG_rest_output_channel_num[AG_index_in_total] -= 1;
            AG_produce_output_channel_num[AG_index_in_total] += 1;
        }
    }
}


void ElementPipelineSchedule::ScheduleNaiveStage1(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication, int input_cycle_index)
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

void ElementPipelineSchedule::ScheduleNaiveStage2(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication)
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

static int comm_index = 0;
void ElementPipelineSchedule::ScheduleNaiveStage3(int instruction_group_index, int start_AG_index_in_total, int AG_num_this_replication)
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

void ElementPipelineSchedule::ScheduleNaiveStageACT(int instruction_group_index, int start_AG_index_in_total)
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


void ElementPipelineSchedule::ScheduleNaiveCOMM(int instruction_group_index, int send_AG_index, int recv_AG_index)
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


void ElementPipelineSchedule::ScheduleNaiveStage4Pool(int instruction_group_index, int pool_node_index, int execution_AG_index, int input_cycle_index)
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


void ElementPipelineSchedule::ScheduleNaive()
{
    NodeCheck.resize(node_num);

    //// ABSTRACT
//    std::ofstream  OutFile("../isaac.txt", std::ios::out | std::ios::trunc);
//    for (int i = 0; i < 32000; ++i)
//    {
//        ElementPipelineSchedule::ScheduleNaiveAbstract(OutFile, i);
//    }
//    OutFile.close();
//    for (int i = 0; i < node_num; ++i)
//    {
//        std::cout << i << ":" << NodeCheck[i] << std::endl;
//    }
//    std::cout << "time:" << double(part_time_use) / CLOCKS_PER_SEC << "s" << std::endl;

    //// MAIN
    int InstructionGroupNum = 32000;
    PIMCOM_4_base_instruction_ir.resize(InstructionGroupNum);
    for (int i = 0; i < InstructionGroupNum; ++i)
    {
        ElementPipelineSchedule::ScheduleNaiveMain(i);
    }
    for (int i = 0; i < node_num; ++i)
    {
        std::cout << i << ":" << NodeCheck[i] << std::endl;
    }
}



void ElementPipelineSchedule::ScheduleExecution()
{
    SchedulePreparation();
    ScheduleNaive();
    Clear();
}


void ElementPipelineSchedule::Clear()
{
    node_AG_mapping.clear();
    node_AG0_index_in_replication.clear();
    for (int & n : AG_rest_output_channel_num) {n = 0;}
    for (int & n : AG_produce_output_channel_num) {n = 0;}
    AG_key_input_channel_index.clear();
    AG_min_max_input_channel_index.clear();
    AG_ready_input_channel_list.clear();
    pool_rep_min_max_output_channel_index.clear();
    pool_rep_min_max_input_channel_index.clear();
    pool_rep_ready_input_channel_list.clear();
    pool_rep_produce_output_channel_num.clear();
    pool_rep_mapping.clear();
    pool_rep_key_input_channel_index.clear();
    pool_rep_new_ready_input_channel_list.clear();
    input_channel_related_AG_index.clear();
    PIMCOM_4_element_AG_info_list.clear();
    PIMCOM_4_element_AG_index_in_replication.clear();
    input_channel_related_AG_index.clear();
    NodeCheck.clear();
    part_time_use = 0;
    comm_index = 0;
}



int ElementPipelineSchedule::GetInputChannelFromOutputIndex(int node_index, int output_index, bool is_last)
{
    struct PIMCOM_node Node = PIMCOM_node_list[node_index];
    struct param Params = Node.param;
    int input_H = Node.input_dim[2];
    int input_W = Node.input_dim[3];
    int conv_kernel_w = Params.kernel_w;
    int conv_kernel_h = Params.kernel_h;
    int conv_padding_h0 = Params.pad_h0;
    int conv_padding_h1 = Params.pad_h1;
    int conv_padding_w0 = Params.pad_w0;
    int conv_padding_w1 = Params.pad_w1;
    int conv_stride_w = Params.stride_w;
    int conv_stride_h = Params.stride_h;

    int output_W = floor(float(input_W + conv_padding_w0 + conv_padding_w1 - conv_kernel_w) / float(conv_stride_w)) + 1;
    int output_H = floor(float(input_H + conv_padding_h0 + conv_padding_h1 - conv_kernel_h) / float(conv_stride_h)) + 1;
    int info_output_W = Node.output_dim[3];
    int info_output_H = Node.output_dim[2];
    if (info_output_W != output_W || info_output_H != output_H)
    {
        std::cout << info_output_H << " " << output_W << std::endl;
        std::cout << " Output Size Doesn't Match" << std::endl;
        return -1;
    }
    int normal_start_index_in_w = conv_padding_w0/conv_stride_w + (conv_padding_w0 % conv_stride_w == 0 ? 0 : 1);
    int normal_start_index_in_h = conv_padding_h0/conv_stride_h + (conv_padding_h0 % conv_stride_h == 0 ? 0 : 1);

    int i = output_index / output_W;
    int j = output_index % output_W;
    int start_address = i * conv_stride_h * input_W + j *  conv_stride_w;
    if (j < normal_start_index_in_w)
        start_address -= (j * conv_stride_w);
    else
        start_address -= conv_padding_w0;
    if (i < normal_start_index_in_h)
        start_address -= (i * conv_stride_h * input_W);
    else
        start_address -= conv_padding_h0 * input_W;

    int start_row = start_address / input_W;
    int start_col = start_address % input_W;

    int conv_w_num = conv_kernel_w;
    if (j < normal_start_index_in_w)
        conv_w_num = conv_w_num - conv_padding_w0 + j * conv_stride_w;
    if (start_col + conv_kernel_w > input_W)
        conv_w_num = conv_w_num - (start_col + conv_kernel_w - input_W);

    int conv_h_num = conv_kernel_h;
    if (i < normal_start_index_in_h)
        conv_h_num = conv_h_num - conv_padding_h0 + i * conv_stride_h;
    if (start_row + conv_kernel_h > input_H)
        conv_h_num = conv_h_num - (start_row + conv_kernel_h - input_H);

    int h = 0;
    int w = 0;
    if (is_last)
    {
        h = conv_h_num-1;
        w = conv_w_num-1;
    }
    int position = start_address + w + h * input_W; // input_index
    return position;
}

void ElementPipelineSchedule::SaveInstruction()
{
    std::ofstream OutFile("../element_instruction.txt", std::ios::out | std::ios::trunc);
    for (int inf = inference_start; inf <= inference_end ; ++inf)
    {
        OutFile << "***************************************************  inference_index " << inf << " *************************************************" << std::endl;
        int instruction_group_num = PIMCOM_4_base_instruction_ir.size();
        for (int i = 0; i < instruction_group_num; ++i)
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