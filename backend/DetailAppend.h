//
// Created by SXT on 2022/9/1.
//

#ifndef PIMCOM_DETAILAPPEND_H
#define PIMCOM_DETAILAPPEND_H

#include "configure.h"
#include "common.h"

class DetailAppend
{
public:
    void AppendDetail(Json::Value & DNNInfo);
    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
    void ShowDetailedInstruction(Json::Value & DNNInfo);
private:
    int instruction_group_num;
    int core_num;
    void PreProcess(Json::Value & DNNInfo);
    void PrepareForInput(Json::Value & DNNInfo);
};


#endif //PIMCOM_DETAILAPPEND_H
