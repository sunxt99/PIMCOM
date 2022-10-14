//
// Created by SXT on 2022/10/5.
//

#ifndef PIMCOM_PIMCOMVARIABLE_H
#define PIMCOM_PIMCOMVARIABLE_H

#include "../common.h"

extern std::map<int, struct PIMCOM_node> PIMCOM_node_list_origin;
extern std::map<int, struct PIMCOM_node> PIMCOM_node_list;
extern std::vector<struct PIMCOM_conv_pool_input_output_info> PIMCOM_conv_pool_input_output_info;
extern std::vector<struct PIMCOM_2_AG_partition> PIMCOM_2_AG_partition;
extern std::vector<struct PIMCOM_2_virtual_crossbar> PIMCOM_2_virtual_crossbar;
extern struct PIMCOM_2_resource_info PIMCOM_2_resource_info;
extern std::vector<int> PIMCOM_2_effective_node;
extern struct PIMCOM_3_hierarchy_map PIMCOM_3_hierarchy_map;
extern std::map<int, std::vector<int>> PIMCOM_3_virtual_core_crossbar_map;
extern std::map<int, int> PIMCOM_4_AG_instruction_group_num;
extern struct PIMCOM_4_first_AG_info PIMCOM_4_first_AG_info;
extern struct PIMCOM_4_virtual_core_AG_map PIMCOM_4_virtual_core_AG_map;
extern struct PIMCOM_4_recv_info PIMCOM_4_recv_info;
extern std::vector<struct PIMCOM_4_instruction_ir> PIMCOM_4_base_instruction_ir;
extern std::vector<std::vector<int>> PIMCOM_4_input_cycle_record;
extern std::map<int, struct PIMCOM_4_instruction_ir> PIMCOM_4_post_instruction_ir;
extern std::map<int, struct PIMCOM_4_instruction_ir> PIMCOM_4_post_multi_core_instruction_ir;
extern struct PIMCOM_5_reload_info PIMCOM_5_reload_info;
extern std::vector<struct PIMCOM_4_instruction_ir> PIMCOM_5_base_instruction_with_reload;
extern struct PIMCOM_6_detailed_instruction_ir PIMCOM_6_detailed_instruction_ir;

extern std::set<int> PIMCOM_4_unique_instruction_group_index;
extern std::vector<int> PIMCOM_4_evaluation_instruction_group_index;
extern std::map<int, int> PIMCOM_4_core_instruction_group_num;

extern std::vector<std::pair<struct MapSortStruct, int>> PIMCOM_3_compute_crossbar_ratio;
extern std::vector<std::vector<struct AGMapStruct>> PIMCOM_3_mapping_result;

extern std::map<int, std::set<int>> PIMCOM_4_effective_provider_consumer_relation;
extern std::map<int, std::set<int>> PIMCOM_4_effective_consumer_provider_relation;
extern std::map<int, std::set<int>> PIMCOM_4_provider_consumer_relation_with_pool;
extern std::map<int, std::set<int>> PIMCOM_4_consumer_provider_relation_with_pool;
#endif //PIMCOM_PIMCOMVARIABLE_H
