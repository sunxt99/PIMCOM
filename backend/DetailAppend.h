//
// Created by SXT on 2022/9/1.
//

#ifndef PIMCOM_DETAILAPPEND_H
#define PIMCOM_DETAILAPPEND_H

#include "common.h"
#include "../configure.h"

class DetailAppend
{
public:
    void AppendDetail(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
    void SaveDetailedInstructionFast(Json::Value & DNNInfo);
    void SaveDetailedInstructionSlow(Json::Value & DNNInfo);
private:
    int instruction_group_num;
    int core_num;
    void PreProcessFast(Json::Value & DNNInfo);
    void PreProcessSlow(Json::Value & DNNInfo);
    void PrepareForInputFast(Json::Value & DNNInfo);
    void PrepareForInputSlow(Json::Value & DNNInfo);
};


#endif //PIMCOM_DETAILAPPEND_H
