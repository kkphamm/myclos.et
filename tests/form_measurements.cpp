#include "simulation/avatar.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace closet;
namespace {
// A taut tape around a horizontal mesh section follows its convex perimeter.
// Cauchy's formula measures that perimeter by averaging its projected widths.
float tape(const Mesh& mesh,const AvatarRest& rest,float y) {
    std::vector<Vec3> slice;
    for(auto f:mesh.triangles) for(int k=0;k<3;++k) {
        int i=f[k],j=f[(k+1)%3];
        if(i>j || rest.limbWeights[i][0]+rest.limbWeights[j][0]>=1.1f) continue;
        Vec3 a=mesh.positions[i],b=mesh.positions[j];
        if((a.y>y)==(b.y>y)) continue;
        float t=(y-a.y)/(b.y-a.y);
        slice.push_back({a.x+t*(b.x-a.x),y,a.z+t*(b.z-a.z)});
    }
    if(slice.size()<3) throw std::runtime_error("empty tape measurement");
    constexpr int directions=90;
    constexpr float pi=3.14159265f;
    float sum=0;
    for(int k=0;k<directions;++k) {
        float dx=std::cos(pi*k/directions),dz=std::sin(pi*k/directions);
        float lo=1e3f,hi=-lo;
        for(Vec3 p:slice) {
            float projection=dx*p.x+dz*p.z;
            lo=std::min(lo,projection); hi=std::max(hi,projection);
        }
        sum+=hi-lo;
    }
    return 100*pi*sum/directions;
}

bool check(Body body,bool male,const char* name) {
    const auto mesh=bodyMesh(body,male);
    const auto& rest=avatarRest(male);
    auto band=[&](float from,float to,bool narrowest=false) {
        float result=narrowest?1e3f:0.f;
        for(int i=0;i<=14;++i) {
            float y=.01f*body.height*(from+(to-from)*i/14);
            float cm=tape(mesh,rest,y);
            result=narrowest?std::min(result,cm):std::max(result,cm);
        }
        return result;
    };
    float chest=band(.681f,.749f),waist=band(.593f,.645f,true),hips=band(.480f,.555f);
    float shoulder=0,closest=1e3f,crotch=1e3f,stature=0,sole=1e3f;
    std::set<std::array<int,3>> surface;
    for(size_t i=0;i<mesh.positions.size();++i) {
        Vec3 p=mesh.positions[i],r=rest.mesh.positions[i];
        surface.insert({int(std::lround(p.x*10000)),int(std::lround(p.y*10000)),int(std::lround(p.z*10000))});
        if(std::abs(p.x)<1e-6f && p.y>.003f*body.height) crotch=std::min(crotch,100*p.y);
        stature=std::max(stature,100*p.y); sole=std::min(sole,100*p.y);
        // Follow the same shoulder landmark through independent chest and shoulder edits.
        float distance=std::abs(r.x-.20f);
        if(r.y>1.375f && r.y<1.403f && std::abs(r.z)<.055f && distance<closest) {
            closest=distance; shoulder=200*p.x;
        }
    }
    for(auto point:surface) if(!surface.count({-point[0],point[1],point[2]}))
        throw std::runtime_error("fitting form lost bilateral symmetry");
    std::cout<<(male?"masculine":"feminine")<<','<<name<<','<<stature<<','<<chest<<','<<waist<<','<<hips<<','<<shoulder<<','<<crotch<<'\n';
    return std::abs(chest-body.chest)<1 && std::abs(waist-body.waist)<1 && std::abs(hips-body.hips)<1 &&
           std::abs(shoulder-body.shoulder)<1 && std::abs(stature-body.height)<.05f && std::abs(sole)<.05f &&
           std::abs(crotch-body.height*(male?81.25f/177.75f:77.5f/166.25f))<.15f;
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
