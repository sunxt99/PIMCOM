//
// Created by SXT on 2022/9/16.
//

#include "ElementPipelineDesign.h"

void ElementPipelineDesign::ShowWaitToActInfo(Json::Value &DNNInfo)
{
    NodeList = DNNInfo["node_list"];
    node_num = static_cast<int>(NodeList.size());

    for (int n = 0; n < node_num; ++n)
    {
        Json::Value Node = NodeList[n];
        if (strcmp(Node["operation"].asCString(), "OP_CONV") != 0)
            continue;
        Json::Value Params = Node["param"];
        int input_h = Node["input_dim"][2].asInt();
        int input_w = Node["input_dim"][3].asInt();
        int input_c = Node["input_dim"][1].asInt();
        int kernel_w = Params["kernel_w"].asInt();
        int kernel_h = Params["kernel_h"].asInt();
        int padding_h0 = Params["pad_h0"].asInt();
        int padding_h1 = Params["pad_h1"].asInt();
        int padding_w0 = Params["pad_w0"].asInt();
        int padding_w1 = Params["pad_w1"].asInt();
        int stride_w = Params["stride_w"].asInt();
        int stride_h = Params["stride_h"].asInt();

        int output_w = floor(float(input_w + padding_w0 + padding_w1 - kernel_w) / float(stride_w)) + 1;
        int output_h = floor(float(input_h + padding_h0 + padding_h1 - kernel_h) / float(stride_h)) + 1;
        int info_output_w = Node["output_dim"][3].asInt();
        int info_output_h = Node["output_dim"][2].asInt();
        if (info_output_w != output_w || info_output_h != output_h)
        {
            std::cout << " Output Size Doesn't Match" << std::endl;
            return;
        }
        std::cout << "input: " << input_w << " kernel:" << kernel_w << " stride:" << stride_w << " padding:" << padding_w0 << std::endl;
        std::cout << "  " << 100* float(input_w * input_h - (input_h - kernel_h + padding_h0+ 1) * (input_w - kernel_w + padding_w0 + 1 )) / float (input_h * input_w) << "%" << std::endl;
        int rest_input_w = input_w - (kernel_w - padding_w0 - 1);
        int rest_input_h = input_h - (kernel_h - padding_h0 - 1);
        int effective_input_w = (kernel_w - padding_w0 - 1);
        int effective_input_h = (kernel_h - padding_h0 - 1);
        int blank_element_w = rest_input_w - output_w;
        int blank_element_h = rest_input_h - output_h;
        if (blank_element_w < 0)  // stride = 1 and padding_w1 > 0
        {
            effective_input_w += rest_input_w;
            output_w = rest_input_w;
        }
        else if (blank_element_w <= output_w - 1)
            effective_input_w += rest_input_w;
        else
            effective_input_w += output_w + (output_w-1) * (stride_w-1);
        if (blank_element_h < 0) // stride = 1 and padding_h1 > 0
        {
            effective_input_h += rest_input_h;
            output_h = rest_input_h;
        }
        else if (blank_element_h <= output_h - 1)
            effective_input_h += rest_input_h;
        else
            effective_input_h += output_h + (output_h-1) * (stride_h-1);

        std::cout << "  " << effective_input_h << "  " << effective_input_w << "  " << output_h << "  " << output_w << std::endl;
        std::cout << "  " << 100 * float(effective_input_w * effective_input_h - (output_w * output_h))/ float(effective_input_w * effective_input_h) << "%" << std::endl;
    }
}