//
// Created by SXT on 2022/9/16.
//

#include "RowPipelineDesign.h"

void RowPipelineDesign::DesignPipeline(Json::Value &DNNInfo)
{
    NodeList = DNNInfo["node_list"];
    EffectiveNodeList = DNNInfo["2_effective_node"];
    node_num = static_cast<int>(NodeList.size());
    effective_node_num = static_cast<int>(EffectiveNodeList.size());
//    TryRowPipeline1();
    GetStartPosition(DNNInfo);
    GetRecvStartInfo(DNNInfo);
    TryRowPipeline2(DNNInfo);
//    ShowRecvStartInfo(DNNInfo);
    DNNInfo["5_node_list_augmented"] = NodeList;
}

/*
void RowPipelineDesign::TryRowPipeline1()
{
    int period_num = 50;
    int node_num = 4;
    int inference_num = 3;
    int processed_start_log[5] = {0};
    int processed_recv_log[5] = {0};
    int input_W[5] = {7,5,3,3,1};
    int output_H[5] = {5,3,3,3,1};
    int kernel_size[5] = {3,3,3,3,0};
    int start_log[5] = {0};
    int recv_log[5] = {0};
    int start_info[5][5] = {{0,1,2,3,4},
                            {3,4,5},
                            {5,6,7},
                            {7,8,9},
                            {10}};
    int recv_info[5][5] = {{-1},
                           {0,1,2,3,4},
                           {3,4,5,-1,-1,},
                           {5,6,7,-1,-1},
                           {7,8,9}};


    int record_log[5][3] = {0}; // recv compute send

    for (int i = 0; i < period_num; ++i)
    {
        int the_next_period = false;
        for (int j = 0; j < node_num; ++j)
        {
            if (processed_start_log[j] != inference_num)
                the_next_period = true;
        }
        if (!the_next_period)
        {
            std::cout << "node" <<  std::setw(5)   << "  " << "Recv" << "  " << "Com" << "  " << "Send" << std::endl;
            for (int k = 0; k < node_num; ++k)
            {
                std::cout << "node_" <<  k  << "  " << std::setw(4) << record_log[k][0] << "  " << std::setw(4) << record_log[k][1] << "  " << std::setw(4) << record_log[k][2] << std::endl;
            }
            return;
        }

        std::cout << i << std::endl;
        for (int j = 0; j < node_num; ++j)
        {
            if (j == 0)
            {
                if (start_log[j] < output_H[j] && start_info[j][start_log[j]] == i)
                {
                    std::cout << "  " << j << " compute " << start_log[j] << std::endl;
                    std::cout << "  " << j << " send to " << j + 1 <<  std::endl;
                    start_log[j]++;
                    record_log[j][1]++;
                    record_log[j][2]++;
                }
                if (start_log[j] == output_H[j] && processed_start_log[j] < inference_num)
                {
                    processed_start_log[j]++;
                    if (processed_start_log[j] == inference_num)
                    {
                        for (int k = 0; k < 5; ++k)
                        {
                            start_info[j][k] = -1;
                            start_log[j] = -1;
                        }
                    }
                }
                if (start_log[j] == output_H[j])
                {
                    for (int k = 0; k < 5; ++k)
                    {
                        start_info[j][k] += 5;
                        start_log[j] = 0;
                    }
                }
            }
            else
            {
                if ( start_log[j] < output_H[j] && start_info[j][start_log[j]] == i)
                {
                    std::cout << "  " << j << " compute " <<  start_log[j] <<  std::endl;
                    record_log[j][1]++;
                    if (j != node_num-1)
                    {
                        std::cout << "  " << j << " send to " << j + 1 << std::endl;
                        record_log[j][2]++;
                    }
                    start_log[j]++;
                }
                if (start_log[j] == output_H[j] && processed_start_log[j] < inference_num)
                {
                    processed_start_log[j]++;
                    if (processed_start_log[j] == inference_num)
                    {
                        for (int k = 0; k < 5; ++k)
                        {
                            start_info[j][k] = -1;
                            start_log[j] = -1;
                        }
                    }
                }
                if (start_log[j] == output_H[j])
                {
                    start_log[j] = 0;
                    for (int k = 0; k < 5; ++k)
                    {
                        start_info[j][k] += 5;
                    }
                }

                if ( recv_log[j] < output_H[j-1] && recv_info[j][recv_log[j]] == i)
                {
                    std::cout << "  " << j << " recv from " << j - 1 <<  std::endl;
                    recv_log[j]++;
                    record_log[j][0]++;
                }
                if (recv_log[j] == output_H[j-1] && processed_recv_log[j] < inference_num)
                {
                    processed_recv_log[j]++;
                    if (processed_recv_log[j] == inference_num)
                    {
                        for (int k = 0; k < 5; ++k)
                        {
                            recv_info[j][k] = -1;
                            recv_log[j] = -1;
                        }
                    }
                }
                if (recv_log[j] == output_H[j-1])
                {
                    recv_log[j] = 0;
                    for (int k = 0; k < 5; ++k)
                    {
                        recv_info[j][k] += 5;
                    }
                }
            }
        }
    }
}
*/


void RowPipelineDesign::TryRowPipeline2(Json::Value & DNNInfo)
{
    int period_num = 50;
    int inference_num = 3;
    int processed_start_log[MAX_NODE] = {0};
    int processed_recv_log[MAX_NODE] = {0};
    int start_log[MAX_NODE] = {0};
    int recv_log[MAX_NODE] = {0};
    int record_log[MAX_NODE][3] = {0}; // recv compute send
    int Period = NodeList[EffectiveNodeList[0].asInt()]["output_dim"][2].asInt();

    for (int i = 0; i < period_num; ++i)
    {
        int the_next_period = false;
        for (int j = 0; j < effective_node_num; ++j)
        {
            if (processed_start_log[j] != inference_num)
                the_next_period = true;
        }
        if (!the_next_period)
        {
            std::cout << "node" <<  std::setw(5)   << "  " << "Recv" << "  " << "Com" << "  " << "Send" << std::endl;
            for (int k = 0; k < effective_node_num; ++k)
            {
                std::cout << "node_" <<  k  << "  " << std::setw(4) << record_log[k][0]
                                            << "  " << std::setw(4) << record_log[k][1]
                                            << "  " << std::setw(4) << record_log[k][2] << std::endl;
            }
            return;
        }

        std::cout << i << std::endl;
        for (int j = 0; j < effective_node_num; ++j)
        {
            int node_index = EffectiveNodeList[j].asInt();
            int output_h = NodeList[node_index]["output_dim"][2].asInt();
            int input_h  = NodeList[node_index]["input_dim"][2].asInt();

            //// Compute (All Nodes) and Send (Except the Last One)
            if (start_log[j] >= 0 && start_log[j] < output_h && DNNInfo["5_start_info"][j][start_log[j]].asInt() == i)
            {
                std::cout << "  " << j << " compute " <<  start_log[j] <<  std::endl;
                record_log[j][1]++;
                if (j != effective_node_num-1)
                {
                    std::cout << "  " << j << " send to " << j + 1 << std::endl;
                    record_log[j][2]++;
                }
                start_log[j]++;
            }
            if (start_log[j] == output_h && processed_start_log[j] < inference_num)
            {
                processed_start_log[j]++;
                if (processed_start_log[j] == inference_num)
                {
                    start_log[j] = -1;
                }
                else
                {
                    start_log[j] = 0;
                    int tmp_num = DNNInfo["5_start_info"][j].size();
                    for (int k = 0; k < tmp_num; ++k)
                    {
                        DNNInfo["5_start_info"][j][k] = DNNInfo["5_start_info"][j][k].asInt() + Period;
                    }
                }
            }

            //// Receive Data (Except the First One)
            if (j != 0)
            {
                if ( recv_log[j] >= 0 && recv_log[j] < input_h && DNNInfo["5_recv_info"][j][recv_log[j]] == i)
                {
                    std::cout << "  " << j << " recv from " << j - 1 <<  std::endl;
                    recv_log[j]++;
                    record_log[j][0]++;
                }
                if (recv_log[j] == input_h && processed_recv_log[j] < inference_num)
                {
                    processed_recv_log[j]++;
                    if (processed_recv_log[j] == inference_num)
                    {
                        recv_log[j] = -1;
                    }
                    else
                    {
                        recv_log[j] = 0;
                        int tmp_num = DNNInfo["5_recv_info"][j].size();
                        for (int k = 0; k < tmp_num; ++k)
                        {
                            DNNInfo["5_recv_info"][j][k] = DNNInfo["5_recv_info"][j][k].asInt() + Period;
                        }
                    }
                }
            }
        }
    }
}

void RowPipelineDesign::GetStartPosition(Json::Value &DNNInfo)
{
    for (int n = 0; n < node_num; ++n)
    {
        Json::Value Node = NodeList[n];
        if (strcmp(Node["operation"].asCString(), "OP_CONV") != 0)
            continue;
        Json::Value Params = Node["param"];
        int input_H = Node["input_dim"][2].asInt();
        int input_W = Node["input_dim"][3].asInt();
        int kernel_w = Params["kernel_w"].asInt();
        int kernel_h = Params["kernel_h"].asInt();
        int padding_h0 = Params["pad_h0"].asInt();
        int padding_h1 = Params["pad_h1"].asInt();
        int padding_w0 = Params["pad_w0"].asInt();
        int padding_w1 = Params["pad_w1"].asInt();
        int stride_w = Params["stride_w"].asInt();
        int stride_h = Params["stride_h"].asInt();

        int output_W = floor(float(input_W + padding_w0 + padding_w1 - kernel_w) / float(stride_w)) + 1;
        int output_H = floor(float(input_H + padding_h0 + padding_h1 - kernel_h) / float(stride_h)) + 1;
        int info_output_W = Node["output_dim"][3].asInt();
        int info_output_H = Node["output_dim"][2].asInt();
        if (info_output_W != output_W || info_output_H != output_H)
        {
            std::cout << " Output Size Doesn't Match" << std::endl;
            return;
        }
        NodeList[n]["start_position"]["output_row_index"].resize(output_H);
        for (int i = 0; i < output_H; ++i)
        {
            int start_address = i * stride_h * input_W;
            if (i != 0)
                start_address -= padding_h0 * input_W;
            int start_row = start_address / input_W;

            int h_num = kernel_h;
            if (i == 0)
                h_num -= padding_h0;
            else if (i == output_H-1)
                if (start_row + kernel_h > input_H)
                    h_num -= padding_h1;

            NodeList[n]["start_position"]["output_row_index"][i] = (start_address + (h_num-1) * input_W )/ input_W;
        }
    }
}

void RowPipelineDesign::GetRecvStartInfo(Json::Value &DNNInfo)
{
    for (int i = 0; i < effective_node_num; ++i)
    {
        int node_index = EffectiveNodeList[i].asInt();
        int output_h = NodeList[node_index]["output_dim"][2].asInt();
        int input_h = NodeList[node_index]["input_dim"][2].asInt();
        if (i == 0)
        {
            for (int j = 0; j < output_h; ++j)
            {
                DNNInfo["5_start_info"][i].append(j);
            }
        }
        else
        {
            for (int j = 0; j < input_h; ++j)
            {
                int recv_timestamp = DNNInfo["5_start_info"][i-1][j].asInt();
                DNNInfo["5_recv_info"][i].append(recv_timestamp);
            }
            for (int j = 0; j < output_h; ++j)
            {
                int start_position = NodeList[node_index]["start_position"]["output_row_index"][j].asInt();
                int start_timestamp = DNNInfo["5_recv_info"][i][start_position].asInt() + 1;
                if (j != 0 && start_timestamp == DNNInfo["5_start_info"][i][j-1].asInt())
                    start_timestamp += 1;
                DNNInfo["5_start_info"][i].append(start_timestamp);
            }
        }
    }
}

//int recv_info[5][5] = {{-1},
//                       {0,1,2,3,4},
//                       {3,4,5,-1,-1,},
//                       {5,6,7,-1,-1},
//                       {7,8,9}};
//int start_info[5][5] = {{0,1,2,3,4},
//                        {3,4,5},
//                        {5,6,7},
//                        {7,8,9},
//                        {10}};


void RowPipelineDesign::ShowRecvStartInfo(Json::Value &DNNInfo)
{
    std::cout << "recv_info" << std::endl;
    int recv_num = DNNInfo["5_recv_info"].size();
    for (int i = 0; i < recv_num; ++i)
    {
        std::cout << i << ":";
        int element_num = DNNInfo["5_recv_info"][i].size();
        for (int j = 0; j < element_num; ++j)
        {
            std::cout << " "  << DNNInfo["5_recv_info"][i][j];
        }
        std::cout << std::endl;
    }
    std::cout << "start_info" << std::endl;
    int start_num = DNNInfo["5_start_info"].size();
    for (int i = 0; i < start_num; ++i)
    {
        std::cout << i << ":";
        int element_num = DNNInfo["5_start_info"][i].size();
        for (int j = 0; j < element_num; ++j)
        {
            std::cout << " "  << DNNInfo["5_start_info"][i][j];
        }
        std::cout << std::endl;
    }
}

void RowPipelineDesign::SaveJsonIR(Json::Value &DNNInfo, std::string ModelName)
{
    std::string strJson = DNNInfo.toStyledString();
    std::ofstream fob("../ir/"+ModelName+"/5_pd.json", std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
}