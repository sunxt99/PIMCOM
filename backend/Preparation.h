//
// Created by SXT on 2022/10/5.
//

#ifndef PIMCOM_PREPARATION_H
#define PIMCOM_PREPARATION_H

#include "../common.h"


void PreProcess(Json::Value & DNNInfo);
void GetStructNodeListFromJson(Json::Value & DNNInfo);
void CopyFromOriginNodeList();
void ShowModelInfo();
void GetConvPoolInputOutputInfo();
#endif //PIMCOM_PREPARATION_H
