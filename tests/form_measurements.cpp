#include "simulation/measurements.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace closet;
namespace {
bool check(Body body,bool male,const char* name) {
    const auto mesh=bodyMesh(body,male);
    const auto measured=measureBody(mesh,male);
    const float stature=100*(measured.crown-measured.sole),crotch=100*measured.crotch;
    const float heads=(measured.crown-measured.sole)/(measured.crown-measured.chin);
    std::set<std::array<int,3>> surface;
    for(Vec3 p:mesh.positions)
        surface.insert({int(std::lround(p.x*10000)),int(std::lround(p.y*10000)),int(std::lround(p.z*10000))});
    for(auto point:surface) if(!surface.count({-point[0],point[1],point[2]}))
        throw std::runtime_error("fitting form lost bilateral symmetry");
    std::cout<<(male?"masculine":"feminine")<<','<<name<<','<<stature<<','<<measured.chest.cm<<','
             <<measured.waist.cm<<','<<measured.hips.cm<<','<<measured.shoulderWidthCm<<','<<crotch<<'\n';
    return std::abs(measured.chest.cm-body.chest)<=.5f && std::abs(measured.waist.cm-body.waist)<=.5f &&
           std::abs(measured.hips.cm-body.hips)<=.5f && std::abs(measured.shoulderWidthCm-body.shoulder)<=.5f &&
           std::abs(stature-body.height)<.05f && std::abs(100*measured.sole)<.05f &&
           std::abs(crotch-body.height*.47f)<.5f && heads>=7.3f && heads<=7.7f;
}
}
int main() try {
    std::cout<<std::fixed<<std::setprecision(2)<<"form,configuration,height_cm,chest_cm,waist_cm,hip_cm,shoulder_cm,crotch_to_floor_cm\n";
    bool passed=true;
    for(bool male:{false,true}) {
        const auto& bodies=male?masculinePresets:presets;
        const char* names[]={"Classic","Compact","Tall"};
        for(int i=0;i<3;++i) passed=check(bodies[i],male,names[i]) && passed;
        passed=check({150,112,62,118,36},male,"broad chest") && passed;
        passed=check({188,82,95,84,48},male,"broad shoulders") && passed;
    }
    return passed?0:1;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 1;}
