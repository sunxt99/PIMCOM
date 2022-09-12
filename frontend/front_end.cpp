//
// Created by SXT on 2022/8/15.
//

#include "front_end.hpp"
#include "structure.h"



void GetJsonFile(const char* model_file, const char* json_name)
{
    if (init_tengine() != 0)
    {
        fprintf(stderr, "Initial tengine failed.\n");
        return ;
    }

    ir_graph_t* graph;
    graph = get_ir_graph(NULL, "tengine", model_file);
    if (graph == nullptr)
    {
        fprintf(stderr, "Create graph failed.\n");
        return ;
    }

    ir_tensor_t** tensor_list = graph->tensor_list;
    ir_node_t** node_list = graph->node_list;
    uint16_t tensor_num = graph->tensor_num;
    uint16_t node_num = graph->node_num;
    uint16_t input_num = graph->input_num;
    uint16_t  output_num = graph->output_num;

    std::cout << "node num: " << node_num << std::endl;
    std::cout << "tensor num: " << tensor_num << std::endl << std::endl;

//    for (int i = 0; i < tensor_num; i++)
//    {
//        ir_tensor_t* ttensor = tensor_list[i];
//        std::cout<<"*****************" <<std::endl;
//        std::cout << "index " << ttensor->index << std::endl;
//        std::cout << "name " << ttensor->name << std::endl;
//        std::cout << "dim num " << static_cast<unsigned int>(ttensor->dim_num) << std::endl;  // unit8_t
//        int consumer_num = static_cast<unsigned int>(ttensor->consumer_num);
//        std::cout << "consumer" << std::endl;
//        for (int j = 0; j < consumer_num; ++j)
//        {
//            std::cout << ttensor->consumer[j] << std::endl;
//            std::cout << tensor_list[ttensor->consumer[j]]->name << std::endl;
//        }
//        std::cout << std::endl;
//        std::cout << ttensor->dims[0] << std::endl;
//        std::cout << std::endl;
//    }

    Json::Value NodeList;
    int index = 0;
    for (int i = 0; i < node_num; i++)
    {
        struct op op = node_list[i]->op;

        // For the Inception, because OP name != output name
        node_list[i]->name = tensor_list[node_list[i]->output_tensors[0]]->name;

        if (op.type == 14) // ignore OP_CONST
            continue;

        Json::Value Node;
        Node["index"] = index;
        index++;
        Node["name"] = node_list[i]->name;
        Node["operation"] = OP_TYPE[op.type];
        Node["bitwidth"] = 16;

        if (op.type == 15) // CONV
        {
            auto *param = static_cast<ConvParam *>(op.param_mem);
            Node["param"]["input_channel"] = param->input_channel;
            Node["param"]["output_channel"] = param->output_channel;
            Node["param"]["kernel_h"] = param->kernel_h;
            Node["param"]["kernel_w"] = param->kernel_w;
            Node["param"]["stride_h"] = param->stride_h;
            Node["param"]["stride_w"] = param->stride_w;
            Node["param"]["pad_h0"] = param->pad_h0;
            Node["param"]["pad_h1"] = param->pad_h1;
            Node["param"]["pad_w0"] = param->pad_w0;
            Node["param"]["pad_w1"] = param->pad_w1;
            Node["param"]["dilation_h"] = param->dilation_h;
            Node["param"]["dilation_w"] = param->dilation_w;
            Node["param"]["group"] = param->group;
        }
        else if (op.type == 49) // POOL
        {
            auto *param = static_cast<PoolParam *>(op.param_mem);
            Node["param"]["pool_method"] = param->pool_method;
            Node["param"]["kernel_h"] = param->kernel_h;
            Node["param"]["kernel_w"] = param->kernel_w;
            Node["param"]["stride_h"] = param->stride_h;
            Node["param"]["stride_w"] = param->stride_w;
            Node["param"]["pad_h0"] = param->pad_h0;
            Node["param"]["pad_h1"] = param->pad_h1;
            Node["param"]["pad_w0"] = param->pad_w0;
            Node["param"]["pad_w1"] = param->pad_w1;
        }
        else if (op.type == 26) // FC
        {
            auto *param = static_cast<FcParam *>(op.param_mem);
            Node["param"]["num_output"] = param->num_output;
        }
        else if (op.type == 27) // Flatten
        {
            auto *param = static_cast<FlattenParam *>(op.param_mem);
            Node["param"]["axis"] = param->axis;
            Node["param"]["end_axis"] = param->end_axis;
        }
        else if (op.type == 56) // ReLU
        {
            auto *param = static_cast<ReLUParam *>(op.param_mem);
            Node["param"]["negative_slope"] = param->negative_slope;
        }
        else if (op.type == 72) // Softmax
        {
            auto *param = static_cast<SoftmaxParam *>(op.param_mem);
            Node["param"]["axis"] = param->axis;
        }
        else if (op.type == 13) // CONCAT
        {
            auto *param = static_cast<ConcatParam *>(op.param_mem);
            Node["param"]["axis"] = param->axis;
        }
        else if (op.type == 22) // Elewise
        {
            auto *param = static_cast<ElewiseParam *>(op.param_mem);
            Node["param"]["eletype"] = param->type;
            Node["param"]["shift_float"] = param->shift;
            Node["param"]["power_float"] = param->power;
            Node["param"]["scale_float"] = param->scale;
        }
        else if (  op.type == 33 || op.type == 21 || op.type == 38)
            ;// Do Nothing: Input||    Dropout    ||      LRN
        else if(op.type == 59) // TODO: Reshape
        {
            std::cout << "*****************" <<std::endl;
            std::cout << "op type id " << op.type << std::endl;
            std::cout << "op type "<< OP_TYPE[op.type] << std::endl;
            std::cout << "op param size " << op.param_size << std::endl;
            std::cout << "node name " << node_list[i]->name << std::endl;
            auto *param = static_cast<ReshapeParam *>(op.param_mem);
            std::cout << param->dim_size<<std::endl;
            std::cout << param->re_shape[0] << std::endl;
            std::cout << param->re_shape[1] << std::endl;
            std::cout << std::endl;
        }
        else
        {
//            std::cout << "*****************" <<std::endl;
            std::cout << "op type id " << op.type << std::endl;
            std::cout << "op type "<< OP_TYPE[op.type] << std::endl;
            std::cout << "op param size " << op.param_size << std::endl;
            std::cout << "node name " << node_list[i]->name << std::endl;
            std::cout << std::endl;
        }

        for (int j = 0; j < tensor_num; ++j)
        {
            if ( strcmp(tensor_list[j]->name,node_list[i]->name)==0 )
            {
                ir_tensor_t* ttensor = tensor_list[j];
                // dim_info
                int dim_num =  static_cast<unsigned int>(ttensor->dim_num);
                Node["output_dim_num"] = dim_num;
                Node["output_dim"].resize(dim_num);
                for (int k = 0; k < dim_num; ++k)
                    Node["output_dim"][k] = ttensor->dims[k];
                // consumer_info
                int consumer_num = static_cast<unsigned int>(ttensor->consumer_num);
                Node["consumer_num"] = consumer_num;
                Node["consumer"].resize(consumer_num);
                for (int k = 0; k < consumer_num; ++k)
                    Node["consumer"][k] = tensor_list[ttensor->consumer[k]]->name;
            }
        }

        NodeList.append(Node);

        // Add ReLU node after every CONV node
        if (op.type == 15)
        {
            Json::Value Node_ReLU;
            Node_ReLU["index"] = index;
            Node_ReLU["bitwidth"] = 16;
            Node_ReLU["param"]["negative_slope"] = 0.0;
            index++;
            Node_ReLU["name"] = "ReLU_"+std::to_string(index-1);
            Node_ReLU["operation"] = OP_TYPE[56];
            Node_ReLU["output_dim_num"] = NodeList[index-2]["output_dim_num"].asInt();
            for (int k = 0; k < Node_ReLU["output_dim_num"].asInt(); ++k)
                Node_ReLU["output_dim"][k] = NodeList[index-2]["output_dim"][k].asInt();

            Node_ReLU["consumer_num"] = NodeList[index-2]["consumer_num"].asInt();
            NodeList[index-2]["consumer_num"] = 1;
//            Node_ReLU["provider"] = NodeList[index-2]["name"];
            Node_ReLU["consumer"] = NodeList[index-2]["consumer"];
            NodeList[index-2]["consumer"].resize(0);
            NodeList[index-2]["consumer"].append(Node_ReLU["name"]);
            NodeList.append(Node_ReLU);
        }
    }

    for (int i = 0; i < index; ++i)
    {
        int provider_num = 0;
        for (int j = 0; j < index; ++j)
        {
            if (j == i)
                continue;
            else
            {
                int consumer_num = NodeList[j]["consumer_num"].asInt();
                for (int k = 0; k < consumer_num; ++k)
                {
                    if (strcmp(NodeList[i]["name"].asCString(), NodeList[j]["consumer"][k].asCString()) ==0)
                    {
                        provider_num++;
                        NodeList[i]["provider"].append(NodeList[j]["name"].asCString());
                    }
                }
            }
        }
        NodeList[i]["provider_num"] = provider_num;
    }

    // Update 8/20 Add a wrapper for NodeList
    Json::Value Graph;
    Graph["node_list"] = NodeList;

    // Update 8/23 Add "num_input" for OP_FC
    std::map<std::string, int> name2index_map;
    std::map<std::string, int>::iterator  m_iter;
    for (int i = 0; i < index; ++i)
        name2index_map.insert(std::pair<std::string, int>(NodeList[i]["name"].asCString(),i));
    for (int i = 0; i < index; ++i)
    {
        Json::Value Node = NodeList[i];
        Json::Value Param = Node["param"];
        if (strcmp(Node["operation"].asCString(),"OP_FC") == 0)
        {
            int provider_index = name2index_map[Node["provider"][0].asCString()];
            int num_input = 0;
            for (int h = 0; h < NodeList[provider_index]["output_dim_num"].asInt(); ++h)
                if (NodeList[provider_index]["output_dim"][h].asInt() > num_input)
                    num_input = NodeList[provider_index]["output_dim"][h].asInt();
            int num_output = 1;
            for (int h = 0; h < NodeList[i]["output_dim_num"].asInt(); ++h)
                if (NodeList[i]["output_dim"][h].asInt() > num_output)
                    num_output = NodeList[i]["output_dim"][h].asInt();
            Graph["node_list"][i]["param"]["num_input"] = num_input;
            //// 实际上FC的param原本就有num_output这个参数
            Graph["node_list"][i]["param"]["num_output"] = num_output;
        }
    }


    std::string strJson = Graph.toStyledString();
    std::ofstream fob(json_name, std::ios::trunc | std::ios::out);
    if (fob.is_open())
    {
        fob.write(strJson.c_str(), strJson.length());
        fob.close();
    }
    std::cout << "Successfully Get Json" << std::endl;
    std::cout << "===============================" << std::endl << std::endl << std::endl;
}


int main()
{
//    std::cout << "*** Front End ***" << std::endl;
//    const char* vgg16_model = "./vgg16-12.tmfile";
//    const char* vgg16_json = "../../../vgg16-12.json";
//    GetJsonFile(vgg16_model, vgg16_json);
//    const char* inception_v1_model = "./inception-v1-12.tmfile";
//    const char* inception_v1_json = "../../../inception-v1.json";
//    GetJsonFile(inception_v1_model, inception_v1_json);
    const char* resnet18_model = "./resnet18.tmfile";
    const char* resnet18_json = "../../../resnet18.json";
    GetJsonFile(resnet18_model, resnet18_json);
//    const char* alexnet_model = "./alexnet.tmfile";
//    const char* alexnet_json = "../../../alexnet.json";
//    GetJsonFile(alexnet_model, alexnet_json);
}
