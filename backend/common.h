// 123
// Created by SXT on 2022/8/18.
//

#ifndef _COMMON
#define _COMMON

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cmath>
#include "json/json.h"
#include <ctime>
#include <set>
#include <list>

static void ShowSingleInstruction(Json::Value Instruction, int inf)
{
    std::string Operation = Instruction["operation"].asCString();
    if (strcmp(Operation.c_str(), "RECV") == 0)
        std::cout << "    [" << Operation << "]"
                  << " stage:" << Instruction["stage"]
                  << " rd:" << Instruction["destination"]
                  <<  " from Core:" << Instruction["from_core"]
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "SEND") == 0)
        std::cout << "    [" << Operation << "]"
                  << " stage:" << Instruction["stage"]
                  << " rs:" << Instruction["source"]
                  <<  " to Core:" << Instruction["to_core"]
                  << " rlength:" << Instruction["relative_length"]
                  << " offset:" << Instruction["offset"] // the comm at the end of inference period has "offset" info but no "rlength" info
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
                  << " rd:" << Instruction["destination"]
                  << " rd_offset:" << Instruction["offset"]["value"]
                  << " rs:" << Instruction["source"]
                  << " node:" << Instruction["node_index"]
                  << " input:" << Instruction["input_cycle_index"]
                  << " input_element_num:" << Instruction["input_element_num"]
                  << " output_element_num:" << Instruction["output_element_num"] <<  std::endl;
    else if (strcmp(Operation.c_str(), "VADD") == 0)
        std::cout << "    [" << Operation << "]"
                  << " rs1:" << Instruction["source_1"]
                  << " rs2:" << Instruction["source_2"]
                  << " rd:" << Instruction["destination"]
                  << " offset:" << Instruction["offset"]["value"]
                  << " rlength:" << Instruction["relative_length"]
                  << " element_num:" << Instruction["element_num"] << std::endl;
    else if (strcmp(Operation.c_str(), "ReLU") == 0)
        std::cout << "    <" << Operation << ">"
                  << " rs:" << Instruction["source"]
                  << " rd:" << Instruction["destination"]
                  << " offset:" << Instruction["offset"]["value"]
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
    else if (strcmp(Operation.c_str(), "sep") != 0)
        std::cout << "      【" << Operation << "】"
                  << " rs:" << Instruction["source"]
                  << " copy_offset:" << Instruction["copy_offset_flag"]
                  << " element_num:" << Instruction["element_num"]
                  //                              << " node:" << Instruction["node_index"]
                  << std::endl;
    else
        std::cout << "    " << Operation << std::endl;
}

#endif