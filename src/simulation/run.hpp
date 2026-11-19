#pragma once
#include "closet.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace closet {
constexpr float stepDt=1.f/120;
constexpr float quietSpeed=.01f;
constexpr int quietSteps=60;
constexpr int maxSettleSteps=720;

struct DrapeKey {
    Body body;
    int size;
    bool masculine=false;
    bool operator==(const DrapeKey& other) const {
        return body==other.body && size==other.size && masculine==other.masculine;
    }
};

inline bool finiteState(const Simulation& sim) {
    return !sim.cloth.positions.empty() && std::isfinite(sim.rmsSpeed) &&
        std::all_of(sim.cloth.positions.begin(),sim.cloth.positions.end(),[](Vec3 v){
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        });
}

// Wind stays off for cached previews and static experimental runs.
inline int settle(Simulation& sim, bool& settled) {
    int quiet=0;
    settled=false;
    for(int n=1;n<=maxSettleSteps;++n){
        sim.step(stepDt);
        if(!std::isfinite(sim.rmsSpeed)) throw std::runtime_error("Nonfinite cloth velocity");
        quiet=sim.rmsSpeed<quietSpeed ? quiet+1 : 0;
        if(quiet>=quietSteps){ settled=true; return n; }
    }
    return maxSettleSteps;
}
}
