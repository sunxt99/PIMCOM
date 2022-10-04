// 123
// Created by SXT on 2022/8/18.
//

#ifndef _COMMON
#define _COMMON

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <set>
#include <list>
#include <any>
#include <cmath>
#include "json/json.h"
#include <ctime>


/////////////////////////////////////////////////////////  NODE  /////////////////////////////////////////////////////////
struct param
{
    //// FC
    int num_input;
    int num_output;
    //// CONV and POOL
    int kernel_h;
    int kernel_w;
    int stride_h;
    int stride_w;
    int pad_h0;
    int pad_h1;
    int pad_w0;
    int pad_w1;
    int dilation_h;
    int dilation_w;
    int input_channel;
    int output_channel;
    int group;
    int activation;
    int wino_off;
    int pool_method;
    //// FLATTEN
    int axis;
    int end_axis;
    //// ELTWISE
    int eletype;
    int caffe_flavor;
    float shift;
    float power;
    float scale;
};


struct PIMCOM_node
{
    int AG0_core_index;
    int AG0_index_in_total;
    int AG0_node_index;
    int AGP;
    int H;
    int W;
    int bitwidth;
    std::vector<int> consumer_index;
    int consumer_num;
    int copy_offset_flag;
    int effective_node_index;
    int index;
    int index_in_level;
    int input_cycle_in_total;
    std::vector<int> input_dim;
    int input_dim_num;
    int level_index;
    std::string name;
    std::string operation;
    std::vector<int> output_dim;
    struct param param;
    int output_dim_num;
    std::vector<int> provider_index;
    int provider_num;
    int replication_num;
    int replication_num_origin;
    int instruction_group_num = 0;
};

/////////////////////////////////////////////////////////  2  /////////////////////////////////////////////////////////
struct AG_list
{
    int AG_index;
    std::vector<int> virtual_crossbar_list;
    std::map<int, int> virtual_core_list;
};

struct replication
{
    std::vector<struct AG_list> AG_list;
    int agp_index;
    int input_cycle_this_end;
    int input_cycle_this_replication;
    int input_cycle_this_start;
    int instruction_group_num;
};

struct PIMCOM_2_AG_partition
{
    int AGP_num;
    int Height;
    int Width;
    int index;
    int input_cycle_in_total;
    std::string name;
    std::string operation;
    std::vector<struct replication> replication;
    int replication_num;
    int replication_num_origin;
};

struct PIMCOM_2_virtual_crossbar
{
    int index_in_weight;
    int virtual_index;
    int replication_index;
    int array_group_in_weight;
    int array_group_total;
    int height_start;
    int height_end;
    int width_start;
    int width_end;
    int weight_index;
    int node_index;
    int AG_num_per_replication;
    int input_cycle_this_replication;
    int input_cycle_this_replication_start;
    int input_cycle_this_replication_end;
    int agp_index;
    int agp_offset;
    // added in hierarchy mapping
    int vcore;            // 这两个是一个。这个出现在2_virtual_crossbar
    int vcore_index;      // 这个出现在3_hierarchy_map/whole
    int index_in_vcore;
    // added in element placement
    int physical_core;
    int physical_position_in_core;
    // added in schedule
    int instruction_group_num;
};

struct PIMCOM_2_resource_info
{
    int AGs;
    int RRAMS;
};

/////////////////////////////////////////////////////////  3  /////////////////////////////////////////////////////////
struct PIMCOM_3_hierarchy_map
{
    std::vector<std::vector<struct PIMCOM_2_virtual_crossbar>> whole;
//    std::vector<struct whole> whole;
    std::vector<int> whole_index;
    std::vector<std::vector<struct PIMCOM_2_virtual_crossbar>> split;
//    std::vector<struct whole> split;
    std::vector<int> split_index;
};

/////////////////////////////////////////////////////////  5  /////////////////////////////////////////////////////////

struct PIMCOM_5_pool_info
{
//    std::vector<int> input_index;
//    std::vector<int> output_index;
    std::vector<std::vector<int>> input_index;
    std::vector<std::vector<int>> output_index;
};

/////////////////////////////////////////////////////////  6  /////////////////////////////////////////////////////////
struct replication_list_schedule
{
    std::map<int, int> replication_list;
};

struct PIMCOM_6_first_AG_info
{
    std::map<int, struct replication_list_schedule> node_list;
};

struct AG_info_schedule
{
    int AGP;
    int AG_index_in_replication;
    int AG_index_in_total;
    int AG_num_per_replication;
    int agp_index;
    int agp_offset;
    int input_cycle_in_total;
    int input_cycle_this_replication;
    int input_cycle_this_replication_start;
    int input_cycle_this_replication_end;
    int level_index;
    int replication_index;
    int replication_num;
    int replication_num_origin;
};

struct core_schedule
{
    std::vector<struct AG_info_schedule> AG_list;
    std::vector<int> node_list;
};

struct PIMCOM_6_physical_core_AG_map
{
    std::map<int, struct core_schedule> core_list;
};

struct node_recv_info
{
    // For CONV
    int AG_index;
    int AG_index_in_replication;
    int core_index;
    int node_index;
    int replication_index;
    // For FC
    int recv_element;
    int recv_num;
    int start_offset_element;
    int start_offset_num;
};

struct PIMCOM_6_recv_info
{
    std::map<int, std::vector<struct node_recv_info>> node_list;
};


////////////////////////////////////////////////////  Instruction  ////////////////////////////////////////////////////
enum InstType {MVMUL, VEC1OP, VEC2OP, COMM, MEM};


struct INST
{
    InstType type;
    std::string operation;
    std::string stage;
    std::string conv_or_fc;
    std::string note;
    int AGP;
    int AG_index_in_replication;
    int AG_index_in_total;
    int AG_num_per_replication;
    int agp_index;
    int destination;
    int input_cycle_in_total;
    int input_cycle_index;
    int input_cycle_this_replication_end;
    int input_cycle_this_replication_start;
    int input_element_num;
    int instruction_group_index;
    int level_index;
    int node_index;
    int output_element_num;
    int replication_index;
    int replication_num;
    int source;
    int rs_offset;
    int rd_offset;
    //// Added VEC_1OP
    int element_num;
    int level_diff;
    int relative_length;
    //// Added VEC_2op
    int source_1;
    int source_2;
    int rs1_offset;
    int rs2_offset;
    //// Added COMM
    int comm_index;
    int instruction_index_in_core;
    int from_core; // RECV
    int to_core;  // SEND
    ////
    int copy_offset_flag;
    //// Load Input
    int rs_offset_between_inference;
    int rs_offset_in_inference;
    //// Store Output
    int rd_offset_between_inference;
    //// CONCAT
    int input_cycle;
    //// CACHE
    int rd_offset_unit;
    int rs_offset_unit;
    //// POOL
    int input_channel_index;
    int output_channel_index;
    // Prepare For Input
    int rs_offset_in_channel;
    int rs_offset_between_channel;
};

struct INST_MVMUL
{
    InstType type;
    std::string operation;
    std::string stage;
    std::string conv_or_fc;
    std::string note;
    int AGP;
    int AG_index_in_replication;
    int AG_index_in_total;
    int AG_num_per_replication;
    int agp_index;
    int destination;
    int input_cycle_in_total;
    int input_cycle_index;
    int input_cycle_this_replication_end;
    int input_cycle_this_replication_start;
    int input_element_num;
    int instruction_group_index;
    int level_index;
    int node_index;
    int output_element_num;
    int replication_index;
    int replication_num;
    int source;
    int rs_offset;
    int rd_offset;
};

struct INST_VEC_1OP
{
    InstType type;
    std::string operation;
    std::string stage;
    std::string conv_or_fc;
    std::string note;
    int destination;
    int element_num;
    int instruction_group_index;
    int level_index;
    int level_diff;
    int relative_length;
    int source;
    int rd_offset;
    int rs_offset;
    int copy_offset_flag;
};

struct INST_VEC_2OP
{
    InstType type;
    std::string operation;
    std::string stage;
    std::string conv_or_fc;
    std::string note;
    int AGP;
    int agp_index;
    int destination;
    int element_num;
    int instruction_group_index;
    int level_index;
    int level_diff;
    int relative_length;
    int source_1;
    int source_2;
    int rs1_offset;
    int rs2_offset;
    int rd_offset;
    int copy_offset_flag;
};

struct INST_COMM
{
    InstType type;
    std::string operation;
    std::string stage;
    std::string conv_or_fc;
    std::string note;
    int comm_index;
    int element_num;
    int relative_length;
    int instruction_group_index;
    int instruction_index_in_core;
    int level_index;
    int level_diff;
    int AGP;
    int agp_index;
    // RECV
    int destination;
    int from_core;
    // SEND
    int source;
    int to_core;
    int copy_offset_flag;
};

struct INST_MEMORY
{
    InstType type;
    std::string operation;
    std::string stage;
    std::string conv_or_fc;
    std::string note;
    int destination;
    int element_num;
    int instruction_group_index;
    int level_index;
    int level_diff;
    int relative_length;
    int source;
    int rd_offset;
    int rs_offset;
    int copy_offset_flag;
    int rd_offset_between_inference;
};

struct instruction_ir_list
{
    std::vector<struct INST> instruction_ir_list;
};

struct PIMCOM_6_instruction_ir
{
    std::map<int, struct instruction_ir_list> core_list;
//    std::vector<struct instruction_ir_list> core_list;
};

////////////////////////////////////////////////////  7  ////////////////////////////////////////////////////
struct reload_start_and_end
{
    int start;
    int end;
};

struct AG_reload_info
{
    // For CONV
    int AG_index;
    int AG_index_in_replication;
    int core_index;
    int node_index;
    int replication_index;
//    std::map<int, struct reload_start_and_end> reload;
    std::vector<struct reload_start_and_end> reload;
    // For FC
    int recv_element;
    int recv_num;
    int start_offset_element;
    int start_offset_num;
};

struct AG_reload_list
{
    std::map<int, struct AG_reload_info> AG_list;
};

struct PIMCOM_7_reload_info
{
    std::map<int, struct AG_reload_list> core_list;
};




////////////////////////////////////////////////////  8  ////////////////////////////////////////////////////
struct detailed_instruction_ir_list
{
    std::vector<struct INST> instruction_ir_list;
};
struct detailed_instruction_group
{
    std::map<int, struct detailed_instruction_ir_list> core_list;
};
struct detailed_instruction_group_list
{
    std::vector<struct detailed_instruction_group> instruction_group;
};
struct PIMCOM_8_detailed_instruction_ir
{
    struct detailed_instruction_group_list detailed_1_prepare_for_input;
    struct detailed_instruction_group_list detailed_2_quantization_operation;
};

















//////////////////////////////////////////////////////// SHOW INSTRUCTION ////////////////////////////////////////////////////////////////
static void SaveSingleInstructionSlow(std::ofstream & OutFile, Json::Value Instruction, int inf)
{
    std::string Operation = Instruction["operation"].asCString();
    if (strcmp(Operation.c_str(), "RECV") == 0)
        OutFile << "    [" << Operation << "]"
                  //                  << " stage:" << Instruction["stage"]
                  << " rd:" << Instruction["destination"]
                  <<  " from Core:" << Instruction["from_core"]
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "SEND") == 0)
        OutFile << "    [" << Operation << "]"
                  //                  << " stage:" << Instruction["stage"]
                  << " rs:" << Instruction["source"]
                  <<  " to Core:" << Instruction["to_core"]
                  << " rlength:" << Instruction["relative_length"]
                  //                  << " offset:" << Instruction["offset"] // the comm at the end of inference period has "offset" info but no "rlength" info
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "LD") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset_num:" << (inf-Instruction["level_index"].asInt()) % (Instruction["level_diff"].asInt())
                  << " rs_offset_unit:" << Instruction["offset"]["rs_offset_unit"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ST") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset_num:" << (inf-Instruction["level_index"].asInt()) % (Instruction["level_diff"].asInt())
                  << " rd_offset_unit:" << Instruction["offset"]["rd_offset_unit"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "RELOAD-LD") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "RELOAD-ST") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "POST-LD") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "POST-ST") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "POST-VM") == 0)
        OutFile << "      【" << Operation  << "】"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POST-POOL") == 0)
    {
        if (strcmp(Instruction["operation_type"].asCString(), "VM") == 0)
        {
            OutFile << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                    << " rs:" << Instruction["source"]
                    << " rd:" << Instruction["destination"]
                    << " rs_offset:" << Instruction["rs_offset"]
                    << " rd_offset:" << Instruction["rd_offset"]
                    << " element_num:" << Instruction["element_num"]
                    << std::endl;
        }
        else if (strcmp(Instruction["operation_type"].asCString(), "VVMAX") == 0)
        {
            OutFile << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                    << " rs1:" << Instruction["source_1"]
                    << " rs2:" << Instruction["source_2"]
                    << " rd:" << Instruction["destination"]
                    << " rs1_offset:" << Instruction["rs1_offset"]
                    << " rs2_offset:" << Instruction["rs2_offset"]
                    << " rd_offset:" << Instruction["rd_offset"]
                    << " element_num:" << Instruction["element_num"]
                    << std::endl;
        }
    }
    else if (strcmp(Operation.c_str(), "POST-VECTOR") == 0)
        OutFile << "      【" << Operation << "】"
                  << " rs1:" << Instruction["source_1"]
                  << " rs2:" << Instruction["source_2"]
                  << " rd:" << Instruction["destination"]
                  << " rs1_offset:" << Instruction["offset"]["rs1_offset"]
                  << " rs2_offset:" << Instruction["offset"]["rs2_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POST-RELU") == 0)
        OutFile << "    <" << Operation << ">"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "LD-INPUT") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset_in_inference:" << Instruction["offset"]["rs_offset_in_inference"]
                  << " rs_offset_between_inference:" << Instruction["offset"]["rs_offset_between_inference"]
                  // total_rs_offset = inf * rs_offset_between_inference + rs_offset_in_inference
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ST-OUTPUT") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset_between_inference:" << Instruction["offset"]["rd_offset_between_inference"]
                  // total_rd_offset = inf * rd_offset_between_inference
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "WB") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rep_index:" << Instruction["replication_index"]
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "MVMUL") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rs_offset:" << Instruction["offset"]["rs"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["offset"]["rd"].asInt() * Instruction["offset"]["value"].asInt()
                  << " node:" << Instruction["node_index"]
                  << " input:" << Instruction["input_cycle_index"]
                  << " input_element_num:" << Instruction["input_element_num"]
                  << " output_element_num:" << Instruction["output_element_num"] <<  std::endl;
    else if (strcmp(Operation.c_str(), "VADD") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs1:" << Instruction["source_1"]
                  << " rs2:" << Instruction["source_2"]
                  << " rd:" << Instruction["destination"]
                  << " rs1_offset:" << Instruction["offset"]["rs1"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rs2_offset:" << Instruction["offset"]["rs2"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rd_offset:" << Instruction["offset"]["rd"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "OP_RELU") == 0)
        OutFile << "    <" << Operation << ">"
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["offset"]["rd"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rs:" << Instruction["source"]
                  << " rs_offset:" << Instruction["offset"]["rs"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ELTWISE") == 0)
        OutFile << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                  << " rs1:" << Instruction["source_1"]
                  << " rs2:" << Instruction["source_2"]
                  << " rd:" << Instruction["destination"]
                  << " copy_offset:" << Instruction["copy_offset_flag"]
                  << " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "CONCAT") == 0)
        OutFile << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["rs_offset"]
                  << " rd_offset:" << Instruction["rd_offset"]
                  << " input_cycle:" << Instruction["input_cycle"]
                  << " copy_offset:" << Instruction["copy_offset_flag"]
                  << " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VVMAX") == 0)
        OutFile << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                  << std::setw(5) << " rs1:" << Instruction["source_1"]
                  << std::setw(5)<< " rs2:" << Instruction["source_2"]
                  << std::setw(5)<< " rd:" << Instruction["destination"]
                  << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
                  << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
                  << std::setw(12)<< " rs1_offset:" << std::setw(8) << Instruction["rs1_offset"]
                  << std::setw(12)<< " rs2_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) <<  Instruction["rs2_offset_in_output"]
                  << std::setw(12)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rs2_offset_in_output"]
                  << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
                  << std::setw(12)<< " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VM") == 0)
        OutFile << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                  << std::setw(8) << " rs:" << Instruction["source"]
                  << std::setw(11)<< " rd:" << Instruction["destination"]
                  << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
                  << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
                  << std::setw(12)<< " rs_offset:" << std::setw(8) << Instruction["rs_offset"]
                  << std::setw(41)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rd_offset_in_output"]
                  << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
                  << std::setw(12)<< " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "vm_conv") == 0)
        OutFile << "    [" << Operation << "]"
                  << " input_index:" << Instruction["input_cycle_index"]
                  << " rs:" << Instruction["source"]
                  //                              << " rs_offset:" << Instruction["source_offset"]
                  << " rs_offset_w:" << Instruction["source_offset_w"]
                  << " rs_offset_h:" << Instruction["source_offset_h"]
                  << " rs_offset_plus:" << Instruction["source_offset_plus"]
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["destination_offset"]
                  << " element_num:" << Instruction["element_num"]
                  << " N" << Instruction["node_index"]
                  << " R" << Instruction["replication_index"]
                  << " AG" << Instruction["AG_index_in_replication"] << std::endl;
    else if (strcmp(Operation.c_str(), "vm_fc") == 0)
        OutFile << "    [" << Operation << "]"
                  << " input_index:" << Instruction["input_cycle_index"]
                  << " rs:" << Instruction["source"]
                  << " rs_offset:" << Instruction["source_offset"]
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["destination_offset"]
                  << " element_num:" << Instruction["element_num"]
                  << " N" << Instruction["node_index"]
                  << " R" << Instruction["replication_index"]
                  << " AG" << Instruction["AG_index_in_replication"] << std::endl;
    else if (strcmp(Operation.c_str(), "sep") != 0)
        OutFile << "      【" << Operation << "】"
                  << " rs:" << Instruction["source"]
                  << " copy_offset:" << Instruction["copy_offset_flag"]
                  << " element_num:" << Instruction["element_num"]
                  //                              << " node:" << Instruction["node_index"]
                  << std::endl;
    else
        OutFile << "    " << Operation << std::endl;
}

/*
static void SaveSingleInstructionFast(std::ofstream & OutFile, struct INST Instruction, int inf)
{
    std::string Operation = Instruction.operation;
    if (strcmp(Operation.c_str(), "RECV") == 0)
        OutFile << "    [" << Operation << "]"
                  //                  << " stage:" << Instruction.stage
                  << " rd:" << Instruction.destination
                  <<  " from Core:" << Instruction.from_core
                  << " rlength:" << Instruction.relative_length
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "SEND") == 0)
        OutFile << "    [" << Operation << "]"
                  //                  << " stage:" << Instruction.stage
                  << " rs:" << Instruction.source
                  <<  " to Core:" << Instruction.to_core
                  << " rlength:" << Instruction.relative_length
                  //                  << " offset:" << Instruction.offset // the comm at the end of inference period has "offset" info but no "rlength" info
                  << " element_num:" << Instruction.element_num << std::endl;
//    else if (strcmp(Operation.c_str(), "LD") == 0)
//        OutFile << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset_num:" << (inf-Instruction["level_index"].asInt()) % (Instruction["level_diff"].asInt())
//                  << " rs_offset_unit:" << Instruction["rs_offset_unit"]
//                  << " rd_offset:" << Instruction["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "ST") == 0)
//        OutFile << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["rs_offset"]
//                  << " rd_offset_num:" << (inf-Instruction["level_index"].asInt()) % (Instruction["level_diff"].asInt())
//                  << " rd_offset_unit:" << Instruction["rd_offset_unit"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "RELOAD-LD") == 0)
//        OutFile << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["rs_offset"]
//                  << " rd_offset:" << Instruction["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "RELOAD-ST") == 0)
//        OutFile << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["rs_offset"]
//                  << " rd_offset:" << Instruction["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "POST-LD") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction.source
                  << " rd:" << Instruction.destination
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "POST-ST") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction.source
                  << " rd:" << Instruction.destination
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "POST-VM") == 0)
        OutFile << "      【" << Operation  << "】"
                  << " rs:" << Instruction.source
                  << " rd:" << Instruction.destination
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " element_num:" << Instruction.element_num
                  << std::endl;
//    else if (strcmp(Operation.c_str(), "POST-POOL") == 0)
//    {
//        if (Instruction.type == "VM")
//        {
//            OutFile << "      【" << Operation << "-" << Instruction.type << "】"
//                    << " rs:" << Instruction.source
//                    << " rd:" << Instruction.destination
//                    << " rs_offset:" << Instruction.rs_offset
//                    << " rd_offset:" << Instruction.rd_offset
//                    << " element_num:" << Instruction.element_num
//                    << std::endl;
//        }
//        else if (Instruction.type == "VVMAX")
//        {
//            OutFile << "      【" << Operation << "-" << Instruction.type << "】"
//                    << " rs1:" << Instruction.source_1
//                    << " rs2:" << Instruction.source_2
//                    << " rd:" << Instruction.destination
//                    << " rs1_offset:" << Instruction.rs1_offset
//                    << " rs2_offset:" << Instruction.rs2_offset
//                    << " rd_offset:" << Instruction.rd_offset
//                    << " element_num:" << Instruction.element_num
//                    << std::endl;
//        }
//    }
    else if (strcmp(Operation.c_str(), "POST-VECTOR") == 0)
        OutFile << "      【" << Operation << "】"
                  << " rs1:" << Instruction.source_1
                  << " rs2:" << Instruction.source_2
                  << " rd:" << Instruction.destination
                  << " rs1_offset:" << Instruction.rs1_offset
                  << " rs2_offset:" << Instruction.rs2_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " element_num:" << Instruction.element_num
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POST-RELU") == 0)
        OutFile << "    <" << Operation << ">"
                  << " rs:" << Instruction.source
                  << " rd:" << Instruction.destination
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " element_num:" << Instruction.element_num << std::endl;
//    else if (strcmp(Operation.c_str(), "LD-INPUT") == 0)
//        OutFile << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset_in_inference:" << Instruction["rs_offset_in_inference"]
//                  << " rs_offset_between_inference:" << Instruction["rs_offset_between_inference"]
//                  // total_rs_offset = inf * rs_offset_between_inference + rs_offset_in_inference
//                  << " rd_offset:" << Instruction["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ST-OUTPUT") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction.source
                  << " rd:" << Instruction.destination
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd_offset_between_inference:" << Instruction.rd_offset_between_inference
                  // total_rd_offset = inf * rd_offset_between_inference
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "WB") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction.source
                  << " rep_index:" << Instruction.replication_index
                  << " rlength:" << Instruction.relative_length
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "MVMUL") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs:" << Instruction.source
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd:" << Instruction.destination
                  << " rd_offset:" << Instruction.rd_offset
                  << " node:" << Instruction.node_index
                  << " input:" << Instruction.input_cycle_index
                  << " input_element_num:" << Instruction.input_element_num
                  << " output_element_num:" << Instruction.output_element_num <<  std::endl;
    else if (strcmp(Operation.c_str(), "VADD") == 0)
        OutFile << "    [" << Operation << "]"
                  << " rs1:" << Instruction.source_1
                  << " rs2:" << Instruction.source_2
                  << " rd:" << Instruction.destination
                  << " rs1_offset:" << Instruction.rs1_offset
                  << " rs2_offset:" << Instruction.rs2_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " rlength:" << Instruction.relative_length
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "OP_RELU") == 0)
        OutFile << "    <" << Operation << ">"
                  << " rd:" << Instruction.destination
                  << " rd_offset:" << Instruction.rd_offset
                  << " rs:" << Instruction.source
                  << " rs_offset:" << Instruction.rs_offset
                  << " rlength:" << Instruction.relative_length
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "ELTWISE") == 0)
        OutFile << "      【" << Operation << "-" << Instruction.type << "】"
                  << " rs1:" << Instruction.source_1
                  << " rs2:" << Instruction.source_2
                  << " rd:" << Instruction.destination
                  << " copy_offset:" << Instruction.copy_offset_flag
                  << " element_num:" << Instruction.element_num
                  << std::endl;
    else if (strcmp(Operation.c_str(), "CONCAT") == 0)
        OutFile << "      【" << Operation << "-" << Instruction.type << "】"
                  << " rs:" << Instruction.source
                  << " rd:" << Instruction.destination
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " input_cycle:" << Instruction.input_cycle
                  << " copy_offset:" << Instruction.copy_offset_flag
                  << " element_num:" << Instruction.element_num
                  << std::endl;
//    else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VVMAX") == 0)
//        OutFile << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
//                  << std::setw(5) << " rs1:" << Instruction["source_1"]
//                  << std::setw(5)<< " rs2:" << Instruction["source_2"]
//                  << std::setw(5)<< " rd:" << Instruction["destination"]
//                  << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
//                  << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
//                  << std::setw(12)<< " rs1_offset:" << std::setw(8) << Instruction["rs1_offset"]
//                  << std::setw(12)<< " rs2_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) <<  Instruction["rs2_offset_in_output"]
//                  << std::setw(12)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rs2_offset_in_output"]
//                  << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
//                  << std::setw(12)<< " element_num:" << Instruction["element_num"]
//                  << std::endl;
//    else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VM") == 0)
//        OutFile << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
//                  << std::setw(8) << " rs:" << Instruction["source"]
//                  << std::setw(11)<< " rd:" << Instruction["destination"]
//                  << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
//                  << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
//                  << std::setw(12)<< " rs_offset:" << std::setw(8) << Instruction["rs_offset"]
//                  << std::setw(41)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rd_offset_in_output"]
//                  << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
//                  << std::setw(12)<< " element_num:" << Instruction["element_num"]
//                  << std::endl;
//    else if (strcmp(Operation.c_str(), "sep") != 0)
//        OutFile << "      【" << Operation << "】"
//                  << " rs:" << Instruction["source"]
//                  << " copy_offset:" << Instruction["copy_offset_flag"]
//                  << " element_num:" << Instruction["element_num"]
//                  //                              << " node:" << Instruction["node_index"]
//                  << std::endl;
    else
    {
        OutFile << "    " << Operation << std::endl;
    }
}
 */
static void SaveSingleInstructionFast(std::ofstream & OutFile, struct INST Instruction, int inf)
{
    std::string Operation = Instruction.operation;
    switch (Instruction.type)
    {
        case MVMUL:
        {
            OutFile << "    [" << Operation << "]"
                    << " rs:" << Instruction.source
                    << " rs_offset:" << Instruction.rs_offset
                    << " rd:" << Instruction.destination
                    << " rd_offset:" << Instruction.rd_offset
                    << " node:" << Instruction.node_index
                    << " input:" << Instruction.input_cycle_index
                    << " input_element_num:" << Instruction.input_element_num
                    << " output_element_num:" << Instruction.output_element_num <<  std::endl;
            break;
        }
        case VEC1OP:
        {
            OutFile << "    [" << Operation << "]"
                    << " rs:" << Instruction.source
                    << " rd:" << Instruction.destination
                    << " rs_offset:" << Instruction.rs_offset
                    << " rd_offset:" << Instruction.rd_offset
                    << " element_num:" << Instruction.element_num << std::endl;
            break;
        }
        case VEC2OP:
        {
            OutFile << "    [" << Operation << "]"
                    << " rs1:" << Instruction.source_1
                    << " rs2:" << Instruction.source_2
                    << " rd:" << Instruction.destination
                    << " rs1_offset:" << Instruction.rs1_offset
                    << " rs2_offset:" << Instruction.rs2_offset
                    << " rd_offset:" << Instruction.rd_offset
                    << " rlength:" << Instruction.relative_length
                    << " element_num:" << Instruction.element_num << std::endl;
            break;
        }
        case COMM:
        {
            if (Operation == "RECV")
                OutFile << "    [" << Operation << "]"
                        //                  << " stage:" << Instruction.stage
                        << " rd:" << Instruction.destination
                        <<  " from Core:" << Instruction.from_core
                        << " rlength:" << Instruction.relative_length
                        << " element_num:" << Instruction.element_num << std::endl;
            else if (Operation == "SEND")
                OutFile << "    [" << Operation << "]"
                        //                  << " stage:" << Instruction.stage
                        << " rs:" << Instruction.source
                        <<  " to Core:" << Instruction.to_core
                        << " rlength:" << Instruction.relative_length
                        //                  << " offset:" << Instruction.offset // the comm at the end of inference period has "offset" info but no "rlength" info
                        << " element_num:" << Instruction.element_num << std::endl;
            break;
        }
        case MEM:
        {

            OutFile << "    [" << Operation << "]"
                    << " node:" << Instruction.node_index
                    << " rs:" << Instruction.source
                    << " rd:" << Instruction.destination
                    << " rs_offset:" << Instruction.rs_offset
                    << " rd_offset:" << Instruction.rd_offset
                    << " element_num:" << Instruction.element_num << std::endl;
            break;
        }
    }
}

static void ShowSingleInstructionSlow(Json::Value Instruction, int inf)
{
    std::string Operation = Instruction["operation"].asCString();
    if (strcmp(Operation.c_str(), "RECV") == 0)
        std::cout << "    [" << Operation << "]"
//                  << " stage:" << Instruction["stage"]
                  << " rd:" << Instruction["destination"]
                  <<  " from Core:" << Instruction["from_core"]
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "SEND") == 0)
        std::cout << "    [" << Operation << "]"
//                  << " stage:" << Instruction["stage"]
                  << " rs:" << Instruction["source"]
                  <<  " to Core:" << Instruction["to_core"]
                  << " rlength:" << Instruction["relative_length"]
//                  << " offset:" << Instruction["offset"] // the comm at the end of inference period has "offset" info but no "rlength" info
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "LD") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset_num:" << (inf-Instruction["level_index"].asInt()) % (Instruction["level_diff"].asInt())
                  << " rs_offset_unit:" << Instruction["offset"]["rs_offset_unit"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ST") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset_num:" << (inf-Instruction["level_index"].asInt()) % (Instruction["level_diff"].asInt())
                  << " rd_offset_unit:" << Instruction["offset"]["rd_offset_unit"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "RELOAD-LD") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "RELOAD-ST") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "POST-LD") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "POST-ST") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "POST-VM") == 0)
        std::cout << "      【" << Operation  << "】"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POST-VECTOR") == 0)
        std::cout << "      【" << Operation << "】"
                  << " rs1:" << Instruction["source_1"]
                  << " rs2:" << Instruction["source_2"]
                  << " rd:" << Instruction["destination"]
                  << " rs1_offset:" << Instruction["offset"]["rs1_offset"]
                  << " rs2_offset:" << Instruction["offset"]["rs2_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POST-RELU") == 0)
        std::cout << "    <" << Operation << ">"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "LD-INPUT") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset_in_inference:" << Instruction["offset"]["rs_offset_in_inference"]
                  << " rs_offset_between_inference:" << Instruction["offset"]["rs_offset_between_inference"]
                  // total_rs_offset = inf * rs_offset_between_inference + rs_offset_in_inference
                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ST-OUTPUT") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
                  << " rd_offset_between_inference:" << Instruction["offset"]["rd_offset_between_inference"]
                  // total_rd_offset = inf * rd_offset_between_inference
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "WB") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rep_index:" << Instruction["replication_index"]
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "MVMUL") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction["source"]
                  << " rs_offset:" << Instruction["offset"]["rs"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["offset"]["rd"].asInt() * Instruction["offset"]["value"].asInt()
                  << " node:" << Instruction["node_index"]
                  << " input:" << Instruction["input_cycle_index"]
                  << " input_element_num:" << Instruction["input_element_num"]
                  << " output_element_num:" << Instruction["output_element_num"] <<  std::endl;
    else if (strcmp(Operation.c_str(), "VADD") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs1:" << Instruction["source_1"]
                  << " rs2:" << Instruction["source_2"]
                  << " rd:" << Instruction["destination"]
                  << " rs1_offset:" << Instruction["offset"]["rs1"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rs2_offset:" << Instruction["offset"]["rs2"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rd_offset:" << Instruction["offset"]["rd"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ReLU") == 0)
        std::cout << "    <" << Operation << ">"
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["offset"]["rd"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rs:" << Instruction["source"]
                  << " rs_offset:" << Instruction["offset"]["rs"].asInt() * Instruction["offset"]["value"].asInt()
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ELTWISE") == 0)
        std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                  << " rs1:" << Instruction["source_1"]
                  << " rs2:" << Instruction["source_2"]
                  << " rd:" << Instruction["destination"]
                  << " copy_offset:" << Instruction["copy_offset_flag"]
                  << " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "CONCAT") == 0)
        std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " rs_offset:" << Instruction["rs_offset"]
                  << " rd_offset:" << Instruction["rd_offset"]
                  << " input_cycle:" << Instruction["input_cycle"]
                  << " copy_offset:" << Instruction["copy_offset_flag"]
                  << " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VVMAX") == 0)
        std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                  << std::setw(5) << " rs1:" << Instruction["source_1"]
                  << std::setw(5)<< " rs2:" << Instruction["source_2"]
                  << std::setw(5)<< " rd:" << Instruction["destination"]
                  << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
                  << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
                  << std::setw(12)<< " rs1_offset:" << std::setw(8) << Instruction["rs1_offset"]
                  << std::setw(12)<< " rs2_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) <<  Instruction["rs2_offset_in_output"]
                  << std::setw(12)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rs2_offset_in_output"]
                  << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
                  << std::setw(12)<< " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VM") == 0)
        std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
                  << std::setw(8) << " rs:" << Instruction["source"]
                  << std::setw(11)<< " rd:" << Instruction["destination"]
                  << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
                  << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
                  << std::setw(12)<< " rs_offset:" << std::setw(8) << Instruction["rs_offset"]
                  << std::setw(41)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rd_offset_in_output"]
                  << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
                  << std::setw(12)<< " element_num:" << Instruction["element_num"]
                  << std::endl;
    else if (strcmp(Operation.c_str(), "vm_conv") == 0)
        std::cout << "    [" << Operation << "]"
                  << " input_index:" << Instruction["input_cycle_index"]
                  << " rs:" << Instruction["source"]
                  //                              << " rs_offset:" << Instruction["source_offset"]
                  << " rs_offset_w:" << Instruction["source_offset_w"]
                  << " rs_offset_h:" << Instruction["source_offset_h"]
                  << " rs_offset_plus:" << Instruction["source_offset_plus"]
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["destination_offset"]
                  << " element_num:" << Instruction["element_num"]
                  << " N" << Instruction["node_index"]
                  << " R" << Instruction["replication_index"]
                  << " AG" << Instruction["AG_index_in_replication"] << std::endl;
    else if (strcmp(Operation.c_str(), "vm_fc") == 0)
        std::cout << "    [" << Operation << "]"
                  << " input_index:" << Instruction["input_cycle_index"]
                  << " rs:" << Instruction["source"]
                  << " rs_offset:" << Instruction["source_offset"]
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["destination_offset"]
                  << " element_num:" << Instruction["element_num"]
                  << " N" << Instruction["node_index"]
                  << " R" << Instruction["replication_index"]
                  << " AG" << Instruction["AG_index_in_replication"] << std::endl;
//    else if (strcmp(Operation.c_str(), "sep") != 0)
//        std::cout << "      【" << Operation << "】"
//                  << " rs:" << Instruction["source"]
//                  << " copy_offset:" << Instruction["copy_offset_flag"]
//                  << " element_num:" << Instruction["element_num"]
//                  //                              << " node:" << Instruction["node_index"]
//                  << std::endl;
//    else
//        std::cout << "    " << Operation << std::endl;
}

static void ShowSingleInstructionFast(struct INST Instruction, int inf)
{
    std::string Operation = Instruction.operation;
    if (strcmp(Operation.c_str(), "RECV") == 0)
        std::cout << "    [" << Operation << "]"
//                  << " stage:" << Instruction.stage
                  << " rd:" << Instruction.destination
                  <<  " from Core:" << Instruction.from_core
                  << " rlength:" << Instruction.relative_length
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "SEND") == 0)
        std::cout << "    [" << Operation << "]"
//                  << " stage:" << Instruction.stage
                  << " rs:" << Instruction.source
                  <<  " to Core:" << Instruction.to_core
                  << " rlength:" << Instruction.relative_length
//                  << " offset:" << Instruction.offset // the comm at the end of inference period has "offset" info but no "rlength" info
                  << " element_num:" << Instruction.element_num << std::endl;
//    else if (strcmp(Operation.c_str(), "LD") == 0)
//        std::cout << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset_num:" << (inf-Instruction["level_index"].asInt()) % (Instruction["level_diff"].asInt())
//                  << " rs_offset_unit:" << Instruction["offset"]["rs_offset_unit"]
//                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "ST") == 0)
//        std::cout << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
//                  << " rd_offset_num:" << (inf-Instruction["level_index"].asInt()) % (Instruction["level_diff"].asInt())
//                  << " rd_offset_unit:" << Instruction["offset"]["rd_offset_unit"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "RELOAD-LD") == 0)
//        std::cout << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
//                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "RELOAD-ST") == 0)
//        std::cout << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
//                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "POST-LD") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction.source
                  << " rd:" << Instruction.destination
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "POST-ST") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction.source
                  << " rd:" << Instruction.destination
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " element_num:" << Instruction.element_num << std::endl;
//    else if (strcmp(Operation.c_str(), "POST-VM") == 0)
//        std::cout << "      【" << Operation  << "】"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
//                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
//                  << " element_num:" << Instruction["element_num"]
//                  << std::endl;
//    else if (strcmp(Operation.c_str(), "POST-VECTOR") == 0)
//        std::cout << "      【" << Operation << "】"
//                  << " rs1:" << Instruction["source_1"]
//                  << " rs2:" << Instruction["source_2"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs1_offset:" << Instruction["offset"]["rs1_offset"]
//                  << " rs2_offset:" << Instruction["offset"]["rs2_offset"]
//                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
//                  << " element_num:" << Instruction["element_num"]
//                  << std::endl;
//    else if (strcmp(Operation.c_str(), "POST-RELU") == 0)
//        std::cout << "    <" << Operation << ">"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
//                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "LD-INPUT") == 0)
//        std::cout << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset_in_inference:" << Instruction["offset"]["rs_offset_in_inference"]
//                  << " rs_offset_between_inference:" << Instruction["offset"]["rs_offset_between_inference"]
//                  // total_rs_offset = inf * rs_offset_between_inference + rs_offset_in_inference
//                  << " rd_offset:" << Instruction["offset"]["rd_offset"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "ST-OUTPUT") == 0)
//        std::cout << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["offset"]["rs_offset"]
//                  << " rd_offset_between_inference:" << Instruction["offset"]["rd_offset_between_inference"]
//                  // total_rd_offset = inf * rd_offset_between_inference
//                  << " element_num:" << Instruction["element_num"] << std::endl;
//    else if (strcmp(Operation.c_str(), "WB") == 0)
//        std::cout << "    [" << Operation << "]"
//                  << " rs:" << Instruction["source"]
//                  << " rep_index:" << Instruction["replication_index"]
//                  << " rlength:" << Instruction["relative_length"]
//                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "MVMUL") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs:" << Instruction.source
                  << " rs_offset:" << Instruction.rs_offset
                  << " rd:" << Instruction.destination
                  << " rd_offset:" << Instruction.rd_offset
                  << " node:" << Instruction.node_index
                  << " input:" << Instruction.input_cycle_index
                  << " input_element_num:" << Instruction.input_element_num
                  << " output_element_num:" << Instruction.output_element_num <<  std::endl;
    else if (strcmp(Operation.c_str(), "VADD") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs1:" << Instruction.source_1
                  << " rs2:" << Instruction.source_2
                  << " rd:" << Instruction.destination
                  << " rs1_offset:" << Instruction.rs1_offset
                  << " rs2_offset:" << Instruction.rs2_offset
                  << " rd_offset:" << Instruction.rd_offset
                  << " rlength:" << Instruction.relative_length
                  << " element_num:" << Instruction.element_num << std::endl;
    else if (strcmp(Operation.c_str(), "ReLU") == 0)
        std::cout << "    <" << Operation << ">"
                  << " rd:" << Instruction.destination
                  << " rd_offset:" << Instruction.rd_offset
                  << " rs:" << Instruction.source
                  << " rs_offset:" << Instruction.rs_offset
                  << " rlength:" << Instruction.relative_length
                  << " element_num:" << Instruction.element_num << std::endl;
//    else if (strcmp(Operation.c_str(), "ELTWISE") == 0)
//        std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
//                  << " rs1:" << Instruction["source_1"]
//                  << " rs2:" << Instruction["source_2"]
//                  << " rd:" << Instruction["destination"]
//                  << " copy_offset:" << Instruction["copy_offset_flag"]
//                  << " element_num:" << Instruction["element_num"]
//                  << std::endl;
//    else if (strcmp(Operation.c_str(), "CONCAT") == 0)
//        std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
//                  << " rs:" << Instruction["source"]
//                  << " rd:" << Instruction["destination"]
//                  << " rs_offset:" << Instruction["rs_offset"]
//                  << " rd_offset:" << Instruction["rd_offset"]
//                  << " input_cycle:" << Instruction["input_cycle"]
//                  << " copy_offset:" << Instruction["copy_offset_flag"]
//                  << " element_num:" << Instruction["element_num"]
//                  << std::endl;
//    else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VVMAX") == 0)
//        std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
//                  << std::setw(5) << " rs1:" << Instruction["source_1"]
//                  << std::setw(5)<< " rs2:" << Instruction["source_2"]
//                  << std::setw(5)<< " rd:" << Instruction["destination"]
//                  << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
//                  << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
//                  << std::setw(12)<< " rs1_offset:" << std::setw(8) << Instruction["rs1_offset"]
//                  << std::setw(12)<< " rs2_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) <<  Instruction["rs2_offset_in_output"]
//                  << std::setw(12)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rs2_offset_in_output"]
//                  << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
//                  << std::setw(12)<< " element_num:" << Instruction["element_num"]
//                  << std::endl;
//    else if (strcmp(Operation.c_str(), "POOL") == 0 && strcmp(Instruction["operation_type"].asCString(), "VM") == 0)
//        std::cout << "      【" << Operation << "-" << Instruction["operation_type"].asCString() << "】"
//                  << std::setw(8) << " rs:" << Instruction["source"]
//                  << std::setw(11)<< " rd:" << Instruction["destination"]
//                  << std::setw(12)<< " input_index:" << std::setw(5) << Instruction["input_index"]
//                  << std::setw(12)<< " output_index:" << std::setw(5) << Instruction["output_index"]
//                  << std::setw(12)<< " rs_offset:" << std::setw(8) << Instruction["rs_offset"]
//                  << std::setw(41)<< " rd_offset:" << std::setw(8) << Instruction["input_element_in_total"] << "+" << std::setw(8) << Instruction["rd_offset_in_output"]
//                  << std::setw(12)<< " copy_offset:" << Instruction["copy_offset_flag"]
//                  << std::setw(12)<< " element_num:" << Instruction["element_num"]
//                  << std::endl;
//    else if (strcmp(Operation.c_str(), "sep") != 0)
//        std::cout << "      【" << Operation << "】"
//                  << " rs:" << Instruction["source"]
//                  << " copy_offset:" << Instruction["copy_offset_flag"]
//                  << " element_num:" << Instruction["element_num"]
//                  //                              << " node:" << Instruction["node_index"]
//                  << std::endl;
//    else
//        std::cout << "    " << Operation << std::endl;
}

#endif