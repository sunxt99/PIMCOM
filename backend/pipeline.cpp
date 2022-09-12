//
// Created by SXT on 2022/8/25.
//


//    Json::Reader jsonReader;
//    Json::Value DNNInfo;
//    std::ifstream jsonFile("../models/JSON/"+model_name+".json");
//    if(!jsonReader.parse(jsonFile, DNNInfo, true))
//    {
//        std::cout << "error" << std::endl;
//        return -1;
//    }
//    Json::Value NodeList = DNNInfo["node_list"];
//    int node_num = NodeList.size();
//    std::cout << "#Nodes in total: " << node_num << std::endl;
//    for (int i = 0; i < node_num; ++i)
//    {
//        Json::Value Node = NodeList[i];
//        if (strcmp(Node["operation"].asCString(),"OP_CONV") == 0)
//        {
//            std::cout << " H: " << DNNInfo["node_list"][i]["output_dim"][2]
//                      << " W: " << DNNInfo["node_list"][i]["output_dim"][3] ;
//            std::cout << " kh: " << DNNInfo["node_list"][i]["param"]["kernel_h"]
//                      << " kw: " << DNNInfo["node_list"][i]["param"]["kernel_w"]
//                      << " pad: "      << DNNInfo["node_list"][i]["param"]["pad_h0"] << std::endl;
//            float W = DNNInfo["node_list"][i]["output_dim"][3].asFloat();
//            float H = DNNInfo["node_list"][i]["output_dim"][2].asFloat();
//            float kh = DNNInfo["node_list"][i]["param"]["kernel_h"].asFloat();
//            float kw = DNNInfo["node_list"][i]["param"]["kernel_w"].asFloat();
//            float p = 1.0;
////            Not Consider the Padding
////            std::cout << W*(kh-1)+kw+(kw-1)*(H-kh+1) << std::endl;
////            std::cout << ((W*(kh-1)+kw+(kw-1)*(H-kh+1))/(W*H)) << "%" <<std::endl;
////            Consider the Padding
//            std::cout << H*W-(H-kh+p+1)*(W-kw+p+1) << std::endl;
//            std::cout << (H*W-(H-kh+p+1)*(W-kw+p+1))/(W*H) << std::endl;
//        }
//    }