#pragma once
#include "common/common.hpp"
#include <algorithm>

namespace client {
// The first extended punch pose in the Businessman sprite sheets.
inline std::size_t meleeImpactFrame(common::AttackKind kind) {
    return kind==common::AttackKind::Jab?6:4;
}
template<class Frames>
float meleeFrameDuration(common::AttackKind kind,const Frames& frames,std::size_t index) {
    const auto impact=meleeImpactFrame(kind);
    if(index>=impact || frames.size()<=impact)return frames[index].durationSeconds;
    const float windup=common::attackDescription(kind).startupTicks*common::TICK_DT;
    // Preserve the opening pose where possible; distribute the remaining windup
    // proportionally across the anticipation poses before the punch extends.
    const float opening=std::min(frames[0].durationSeconds,windup/float(impact));
    if(index==0)return opening;
    float middle=0;
    for(std::size_t i=1;i<impact;++i)middle+=frames[i].durationSeconds;
    return (windup-opening)*frames[index].durationSeconds/middle;
}
}
