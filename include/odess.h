/*
 * @Author: Helix0503 834991203@qq.com
 * @Date: 2024-02-02 10:03:53
 * @LastEditors: Helix0503 834991203@qq.com
 * @LastEditTime: 2024-02-02 10:14:13
 * @FilePath: /JunWu/LocalDedupSim/include/localfin.h
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
#ifndef ODESS_H
#define ODESS_H

#include "absmethod.h"
#include "odess_similarity_detection.h"

using namespace std;

class Odess : public AbsMethod
{
private:
    string myName_ = "Odess";
    int PrevDedupChunkid = -1;
    int Version = 0;
    FeatureIndexTable table;

public:
    Odess();
    ~Odess();
    void ProcessTrace();
};
#endif