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
    void AppendDetail();
    void SaveInstruction();
//    void SaveJsonIR(Json::Value & DNNInfo, std::string ModelName);
private:
    int instruction_group_num;
    int core_num;
    void PreProcess();
    void PrepareForInput();
};


#endif //PIMCOM_DETAILAPPEND_H
