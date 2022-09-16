//
// Created by SXT on 2022/8/19.
//

#include "CrossbarPartition.h"

static int ArrayGroupIndex = 0;
static int VirtualCrossbarIndex = 0;

void CrossbarPartition::PartitionCrossbar(Json::Value &DNNInfo)
{
    PartitionNaive(DNNInfo);
    Check();
    ArrayGroupIndex = 0;
    VirtualCrossbarIndex = 0;
}

void CrossbarPartition::PartitionNaive(Json::Value &DNNInfo)
{
    Json::Value NodeList = DNNInfo["node_list"];
    int node_num = NodeList.size();
    int WeightIndex = 0;
    int EffectiveNodeNum = 0;
    for (int i = 0; i < node_num; ++i)
    {
        Json::Value Node = NodeList[i];
        Json::Value Param = Node["param"];
        int bitwidth = Node["bitwidth"].asInt();
        if (strcmp(Node["operation"].asCString(),"OP_CONV") == 0 || strcmp(Node["operation"].asCString(),"OP_FC") == 0)
        {
            Json::Value NodePartition;
            NodePartition["index"] = Node["index"].asInt();
            NodePartition["name"] = Node["name"].asCString();
            NodePartition["operation"] = Node["operation"].asCString();

            int origin_rep_num = Node["replication_num"].asInt();

            int Height;
            int Width;
            int sliding_window;
            if (Node["operation"] == "OP_CONV")
            {
                Height = (Param["kernel_h"].asInt() * Param["kernel_w"].asInt() * Param["input_channel"].asInt());
                Width = Param["output_channel"].asInt();
                sliding_window = Node["output_dim"][2].asInt() * Node["output_dim"][3].asInt();
            }
            else // FC
            {
                Height = Param["num_input"].asInt();
                Width = Param["num_output"].asInt();
                sliding_window = 1;
            }
            DNNInfo["node_list"][i]["H"] = Height;
            DNNInfo["node_list"][i]["W"] = Width;
            NodePartition["Height"] = Height;
            NodePartition["Width"] = Width;
            NodePartition["input_cycle_in_total"] = sliding_window;
            // Consider the Bias
            // Height += 1;
            int HBarNum = (Height-1) / CrossbarH + 1;
            ////  这里需要看是逻辑crossbar还是物理。考虑物理的话需要考虑两个方面：bit精度的影响和正负系数的影响
//            int WBarNum = (Width-1)  / (CrossbarW / (bitwidth/CellPrecision)) + 1;
            int WBarNum = (Width-1)  / CrossbarW  + 1;
            //// 考虑Core无法容下一个完整AG的情况
            int AGP = ceil(float(WBarNum) / float(CoreW*CoreH));
            NodePartition["AGP_num"] = AGP;
            NodePartition["replication_num_origin"] = origin_rep_num;
            NodePartition["replication_num"] = origin_rep_num*AGP;

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

            DNNInfo["node_list"][i]["replication"].resize(origin_rep_num * AGP);
            DNNInfo["node_list"][i]["AGP"] = AGP;
            DNNInfo["node_list"][i]["replication_num_origin"] = origin_rep_num;
            DNNInfo["node_list"][i]["replication_num"] = origin_rep_num*AGP;
            DNNInfo["node_list"][i]["input_cycle_in_total"] = NodePartition["input_cycle_in_total"].asInt();

            for (int j = 0; j < origin_rep_num; ++j)
            {
                for (int agp = 0; agp < AGP; ++agp)
                {
                    // 每个AGP块都算一个新weight（这里是为了不打破后面过程的关系）
                    int ArrayGroupIndexInWeight = 0;
                    for (int H = 0; H < HBarNum; ++H)
                    {
                        Json::Value CrossbarToArrayGroup;
                        int WStart = agp * CoreH*CoreW;
                        int WEnd = (agp == AGP-1) ? WBarNum : (agp+1)*(CoreW*CoreH);
                        for (int W = WStart; W < WEnd; ++W)
                        {
                            Json::Value VirtualBar;
                            VirtualBar["index_in_weight"] = H * WBarNum + W;
                            // virtual_index是该crossbar在整个DNN中的编号
                            VirtualBar["virtual_index"] = VirtualCrossbarIndex;
                            CrossbarToArrayGroup.append(VirtualCrossbarIndex);
                            VirtualCrossbarIndex += 1;
                            //// (考虑Core无法容下一个完整AG的情况)
//                            VirtualBar["replication_index"] = j;
                            VirtualBar["replication_index"] = j*AGP+agp;
                            // array_group_in_weight 是在一个Weight块中AG的序号
                            VirtualBar["array_group_in_weight"] = ArrayGroupIndexInWeight;
                            // array_group_total 是AG在全部Weight块中的序号
                            VirtualBar["array_group_total"] = ArrayGroupIndex;
                            VirtualBar["height_start"] = H*CrossbarH;
                            VirtualBar["height_end"] = (H==(HBarNum-1) ? Height-1 : (H+1)*CrossbarH-1 );
                            //// 这里需要看是逻辑crossbar还是物理。考虑物理的话需要考虑两个方面：bit精度的影响和正负系数的影响
    //                        VirtualBar["width_start"] = W*(CrossbarW/(bitwidth/CellPrecision));
    //                        VirtualBar["width_end"] = (W==(WBarNum-1) ? Width-1 : (W+1)*(CrossbarW/(bitwidth/CellPrecision))-1 );
                            VirtualBar["width_start"] = W*CrossbarW;
                            VirtualBar["width_end"] = (W==(WBarNum-1) ? Width-1 : (W+1)*CrossbarW-1 );
                            // 感觉这个weight_index又不是特别有用了。指的是当前weight块在整个DNN中的编号（复制的权重算不同的块）
                            VirtualBar["weight_index"] = WeightIndex;
                            VirtualBar["node_index"] = Node["index"].asInt();
                            //// 增加一个字段：每个权重复制块中包含多少个AG (考虑Core无法容下一个完整AG的情况)
                            VirtualBar["AG_num_per_replication"] = HBarNum;
//                            VirtualBar["AG_num_per_replication"] = HBarNum * AGP;
                            //// 增加字段input_cycle信息
                            VirtualBar["input_cycle_this_replication"] = input_num_per_rep[j];
                            VirtualBar["input_cycle_this_replication_start"] = j == 0 ? 0 : input_num_till_rep[j-1];
                            VirtualBar["input_cycle_this_replication_end"] = j == (origin_rep_num-1) ? (sliding_window-1) : (input_num_till_rep[j]-1);
                            //// 增加agp信息
                            VirtualBar["agp_index"] = agp;
                            VirtualBar["agp_offset"] = WStart * CrossbarW;

//                            DNNInfo["node_list"][i]["replication"][j].append(VirtualBar);
                            DNNInfo["node_list"][i]["replication"][j*AGP+agp].append(VirtualBar);
                            // 增加virtual_crossbar的信息
                            DNNInfo["2_virtual_crossbar"].append(VirtualBar);
                        }
                        Json::Value AG_Wrapper;
                        AG_Wrapper["virtual_crossbar_list"] = CrossbarToArrayGroup;
                        AG_Wrapper["AG_index"] = ArrayGroupIndex;
                        NodePartition["replication"][j*AGP+agp]["AG_list"].append(AG_Wrapper);
                        NodePartition["replication"][j*AGP+agp]["input_cycle_this_replication"] = input_num_per_rep[j];
                        NodePartition["replication"][j*AGP+agp]["input_cycle_this_start"] = j == 0 ? 0 : input_num_till_rep[j-1];
                        NodePartition["replication"][j*AGP+agp]["input_cycle_this_end"] = j == (origin_rep_num-1) ? (sliding_window-1) : (input_num_till_rep[j]-1);
                        NodePartition["replication"][j*AGP+agp]["agp_index"] = agp;

                        ArrayGroupIndex += 1;
                        ArrayGroupIndexInWeight += 1;
                    }
                }
                WeightIndex += 1;
            }
            DNNInfo["2_effective_node"].append(i);
            DNNInfo["2_AG_partition"].append(NodePartition);
            DNNInfo["node_list"][i]["effective_node_index"] = EffectiveNodeNum;
            EffectiveNodeNum += 1;
        }
    }
    std::cout << "#RRAMs needed: " << VirtualCrossbarIndex << std::endl;
    std::cout << "#ArrayGroups needed: " << ArrayGroupIndex << std::endl;
    DNNInfo["2_resource_info"]["RRAMS"] = VirtualCrossbarIndex;
    DNNInfo["2_resource_info"]["AGs"] = ArrayGroupIndex;
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

void CrossbarPartition::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/2_cp.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}