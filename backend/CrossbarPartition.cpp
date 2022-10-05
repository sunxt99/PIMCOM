//
// Created by SXT on 2022/8/19.
//

#include "CrossbarPartition.h"

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;

std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
std::vector<int> PIMCOM_2_effective_node;

static int ArrayGroupIndex = 0;
static int VirtualCrossbarIndex = 0;

void CrossbarPartition::PartitionCrossbar()
{
    PartitionNaive();
    Check();
    ArrayGroupIndex = 0;
    VirtualCrossbarIndex = 0;
}

void CrossbarPartition::PartitionNaive()
{
    int node_num = PIMCOM_node_list.size();
    int WeightIndex = 0;
    int EffectiveNodeNum = 0;
    for (int i = 0; i < node_num; ++i)
    {
        struct PIMCOM_node Node = PIMCOM_node_list[i];
        struct param Param = Node.param;
//        int bitwidth = Node["bitwidth"].asInt();
        int bitwidth = Node.bitwidth;
//        if (strcmp(Node["operation"].asCString(),"OP_CONV") == 0 || strcmp(Node["operation"].asCString(),"OP_FC") == 0)
        if (Node.operation == "OP_CONV" || Node.operation == "OP_FC")
        {
            struct PIMCOM_2_AG_partition pimcom2AgPartition;

            pimcom2AgPartition.index = Node.index;
            pimcom2AgPartition.name = Node.name;
            pimcom2AgPartition.operation = Node.operation;

            int origin_rep_num = Node.replication_num;

            int Height;
            int Width;
            int sliding_window;

            if (Node.operation == "OP_CONV")
            {
                Height = Param.kernel_h * Param.kernel_w * Param.input_channel;
                Width = Param.output_channel;
                sliding_window = Node.output_dim[2] * Node.output_dim[3];
            }
            else // FC
            {
                Height = Param.num_input;
                Width = Param.num_output;
                sliding_window = 1;
            }
            // Consider the Bias
            // Height += 1;

            PIMCOM_node_list[i].H = Height;
            PIMCOM_node_list[i].W = Width;
            pimcom2AgPartition.Height = Height;
            pimcom2AgPartition.Width = Width;
            pimcom2AgPartition.input_cycle_in_total = sliding_window;

            int HBarNum = (Height-1) / CrossbarH + 1;
            ////  这里需要看是逻辑crossbar还是物理。考虑物理的话需要考虑两个方面：bit精度的影响和正负系数的影响
//            int WBarNum = (Width-1)  / (CrossbarW / (bitwidth/CellPrecision)) + 1;
            int WBarNum = (Width-1)  / CrossbarW  + 1;
            //// 考虑Core无法容下一个完整AG的情况
            int AGP = ceil(float(WBarNum) / float(CoreW*CoreH));
            pimcom2AgPartition.AGP_num = AGP;
            pimcom2AgPartition.replication_num_origin = origin_rep_num;
            pimcom2AgPartition.replication_num = origin_rep_num * AGP;

            // 最后还是不要以replication对input进行区分
//            NodePartition["input_cycle_per_replication"] = int(ceil( float(sliding_window)/float(origin_rep_num)));

            int *input_num_per_rep = new int[origin_rep_num];
            for (int p = 0; p < origin_rep_num-1; ++p)
            {
                input_num_per_rep[p] = ceil(float(sliding_window)/float(origin_rep_num));
            }
            input_num_per_rep[origin_rep_num-1] = sliding_window-(origin_rep_num-1)*ceil(float(sliding_window)/float(origin_rep_num));
            int *input_num_till_rep = new int[origin_rep_num];
            int tmpsum = 0;
            for (int p = 0; p < origin_rep_num; ++p)
            {
                // 这里的input_num其实是input_cycle_num
                tmpsum += input_num_per_rep[p];
                input_num_till_rep[p] = tmpsum;
            }

//            DNNInfo["node_list"][i]["replication"].resize(origin_rep_num * AGP);
            PIMCOM_node_list[i].AGP = AGP;
            PIMCOM_node_list[i].replication_num_origin = origin_rep_num;
            PIMCOM_node_list[i].replication_num = origin_rep_num*AGP;
            PIMCOM_node_list[i].input_cycle_in_total = pimcom2AgPartition.input_cycle_in_total;

            pimcom2AgPartition.replication.resize(origin_rep_num * AGP);

            for (int j = 0; j < origin_rep_num; ++j)
            {
                for (int agp = 0; agp < AGP; ++agp)
                {
                    // 每个AGP块都算一个新weight（这里是为了不打破后面过程的关系）
                    int ArrayGroupIndexInWeight = 0;
                    for (int H = 0; H < HBarNum; ++H)
                    {
                        struct AG_list ag_list;
                        int WStart = agp * CoreH*CoreW;
                        int WEnd = (agp == AGP-1) ? WBarNum : (agp+1)*(CoreW*CoreH);
                        for (int W = WStart; W < WEnd; ++W)
                        {
                            struct PIMCOM_2_virtual_crossbar pimcom2VirtualCrossbar;
                            pimcom2VirtualCrossbar.index_in_weight = H * WBarNum + W;
                            // virtual_index是该crossbar在整个DNN中的编号
                            pimcom2VirtualCrossbar.virtual_index = VirtualCrossbarIndex;
                            ag_list.virtual_crossbar_list.push_back(VirtualCrossbarIndex);
                            VirtualCrossbarIndex += 1;
                            // (考虑Core无法容下一个完整AG的情况)
//                            VirtualBar["replication_index"] = j;
                            pimcom2VirtualCrossbar.replication_index = j*AGP+agp;
                            // array_group_in_weight 是在一个Weight块中AG的序号
                            pimcom2VirtualCrossbar.array_group_in_weight = ArrayGroupIndexInWeight;
                            // array_group_total 是AG在全部Weight块中的序号
                            pimcom2VirtualCrossbar.array_group_total = ArrayGroupIndex;
                            pimcom2VirtualCrossbar.height_start = H*CrossbarH;
                            pimcom2VirtualCrossbar.height_end = (H==(HBarNum-1) ? Height-1 : (H+1)*CrossbarH-1 );
                            // 这里需要看是逻辑crossbar还是物理。考虑物理的话需要考虑两个方面：bit精度的影响和正负系数的影响
//                            VirtualBar["width_start"] = W*(CrossbarW/(bitwidth/CellPrecision));
//                            VirtualBar["width_end"] = (W==(WBarNum-1) ? Width-1 : (W+1)*(CrossbarW/(bitwidth/CellPrecision))-1 );
                            pimcom2VirtualCrossbar.width_start = W*CrossbarW;
                            pimcom2VirtualCrossbar.width_end = (W==(WBarNum-1) ? Width-1 : (W+1)*CrossbarW-1 );
                            // 感觉这个weight_index又不是特别有用了。指的是当前weight块在整个DNN中的编号（复制的权重算不同的块）
                            pimcom2VirtualCrossbar.weight_index = WeightIndex;

                            pimcom2VirtualCrossbar.node_index = Node.index;
                            // 增加一个字段：每个权重复制块中包含多少个AG (考虑Core无法容下一个完整AG的情况)
                            pimcom2VirtualCrossbar.AG_num_per_replication = HBarNum;
                            // 增加字段input_cycle信息
                            pimcom2VirtualCrossbar.input_cycle_this_replication = input_num_per_rep[j];
                            pimcom2VirtualCrossbar.input_cycle_this_replication_start = j == 0 ? 0 : input_num_till_rep[j-1];
                            pimcom2VirtualCrossbar.input_cycle_this_replication_end = j == (origin_rep_num-1) ? (sliding_window-1) : (input_num_till_rep[j]-1);
                            // 增加agp信息
                            pimcom2VirtualCrossbar.agp_index = agp;
                            pimcom2VirtualCrossbar.agp_offset = WStart * CrossbarW;
                            PIMCOM_2_virtual_crossbar.push_back(pimcom2VirtualCrossbar);
                        }
                        ag_list.AG_index = ArrayGroupIndex;
                        pimcom2AgPartition.replication[j*AGP+agp].AG_list.push_back(ag_list);
                        pimcom2AgPartition.replication[j*AGP+agp].input_cycle_this_replication = input_num_per_rep[j];
                        pimcom2AgPartition.replication[j*AGP+agp].input_cycle_this_start = j == 0 ? 0 : input_num_till_rep[j-1];
                        pimcom2AgPartition.replication[j*AGP+agp].input_cycle_this_end = j == (origin_rep_num-1) ? (sliding_window-1) : (input_num_till_rep[j]-1);
                        pimcom2AgPartition.replication[j*AGP+agp].agp_index = agp;

                        ArrayGroupIndex += 1;
                        ArrayGroupIndexInWeight += 1;
                    }
                }
                WeightIndex += 1;
            }
            PIMCOM_2_effective_node.push_back(i);
            PIMCOM_2_AG_partition.push_back(pimcom2AgPartition);
            PIMCOM_node_list[i].effective_node_index = EffectiveNodeNum;
//            DNNInfo["node_list"][i]["effective_node_index"] = EffectiveNodeNum;
            EffectiveNodeNum += 1;
        }
    }
    std::cout << "#RRAMs needed: " << VirtualCrossbarIndex << std::endl;
    std::cout << "#ArrayGroups needed: " << ArrayGroupIndex << std::endl;
    PIMCOM_2_resource_info.RRAMS = VirtualCrossbarIndex;
    PIMCOM_2_resource_info.AGs = ArrayGroupIndex;
}


int CrossbarPartition::Check()
{
    if ((VirtualCrossbarIndex-1) > CoreH * CoreW * ChipH * ChipW)
    {
        std::cout << "Illegally Replicate" << std::endl;
        return -1;
    }
    else
    {
        std::cout << "Pass!" << std::endl;
        return 1;
    }
}

//void CrossbarPartition::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
//{
//    std::string strJson = DNNInfo.toStyledString();
//    std::ofstream fob("../ir/"+ModelName+"/2_cp.json", std::ios::trunc | std::ios::out);
//    if (fob.is_open())
//    {
//        fob.write(strJson.c_str(), strJson.length());
//        fob.close();
//    }
//}