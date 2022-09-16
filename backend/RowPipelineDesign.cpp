//
// Created by SXT on 2022/9/16.
//

#include "RowPipelineDesign.h"

void RowPipelineDesign::TryRowPipeline()
{
    int period_num = 50;
    int node_num = 4;
    int inference_num = 3;
    int processed_start_log[4] = {0};
    int processed_recv_log[4] = {0};
    int input_W[4] = {7,5,3,3};
    int output_H[4] = {5,3,3,3};
    int kernel_size[4] = {3,3,3,3};
    int recv_info[4][5] = {{0},
                           {1,2,3,4,5},
                           {4,5,6,-1,-1,},
                           {6,7,8,-1,-1}};
    int start_info[4][5] = {{1,2,3,4,5},
                            {4,5,6},
                            {5,6,7},
                            {7,8,9}};
    int recv_log[4] = {0};
    int start_log[4] = {0};
    int record_log[4][3] = {0}; // recv compute send

    for (int i = 1; i <= period_num; ++i)
    {
        int the_next_period = false;
        for (int j = 0; j < node_num; ++j)
        {
            if (processed_start_log[j] != inference_num)
                the_next_period = true;
        }
        if (!the_next_period)
        {
            for (int k = 0; k < node_num; ++k)
            {
                std::cout << k << std::endl;
                std::cout << "  " << record_log[k][0] << "  " << record_log[k][1] << "  " << record_log[k][2] << std::endl;
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
//            else if (j == node_num-1)
//            {
//
//            }
            else
            {
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

                if ( start_log[j] < output_H[j] && start_info[j][start_log[j]] == i)
                {
                    std::cout << "  " << j << " compute " <<  start_log[j] <<  std::endl;
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
                    start_log[j] = 0;
                    for (int k = 0; k < 5; ++k)
                    {
                        start_info[j][k] += 5;
                    }
                }
            }
        }
    }
}
