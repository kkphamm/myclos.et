#include "catalog.hpp"
#include "cloth_math.hpp"
#include <algorithm>
#include <stdexcept>

namespace closet {
using namespace cloth_math;

void layerOutfit(Simulation& top,Simulation& bottom) {
    if(items[top.item].bottom() || !items[bottom.item].bottom())
        throw std::invalid_argument("Layering expects a top and a bottom");

    // A top panel bridges the bottom garment's fine folds. The support under its footprint
    // must include nearby crests, even when its own vertices fall between them.
    constexpr float footprint=.036f, falloff=.050f;
    constexpr int sectors=32;
    auto sector=[](Vec3 p){
        return int((std::atan2(p.z,p.x)+pi)*(sectors/(2*pi)))%sectors;
    };
    float hem=top.cloth.positions.front().y,waist=bottom.cloth.positions.back().y;
    for(Vec3 p:top.cloth.positions) hem=std::min(hem,p.y);
    std::array<std::vector<Vec3>,sectors> bands;
    bool bottomMoved=false;
    for(size_t i=0;i<bottom.cloth.positions.size();++i) {
        Vec3& p=bottom.cloth.positions[i];
        if(p.y<hem-footprint-falloff) continue;
        // A skirt can be seated around the entire pelvis. Trouser legs must keep their own openings.
        const float lateral=std::clamp((std::abs(p.x)-bottom.body.hips*.00105f)/.050f,0.f,1.f);
        const float outerPanel=items[bottom.item].pants() ? lateral*lateral*(3-2*lateral) : 1.f;
        if(outerPanel>0) {
            // Leave space for both layers on the torso side of the hanging arms.
            Vec3 before=p;
            Vec3 above={p.x,p.y+.05f,p.z};
            Vec3 limit=bottom.contact.dress(above,clearance,.022f); limit.y=p.y;
            Vec3 dressed=bottom.contact.dress(dot(limit,limit)<dot(p,p)?limit:p,clearance+.0025f,.022f);
            float weight=std::clamp((p.y-hem+footprint+falloff)/falloff,0.f,1.f);
            p=bottom.contact.project(p+(dressed-p)*(outerPanel*weight),clearance+.0025f);
            Vec3 correction=p-before;
            if(dot(correction,correction)>1e-12f) {
                bottom.previous[i]=bottom.previous[i]+correction;
                bottomMoved=true;
            }
        }
        bands[sector(p)].push_back(p);
        waist=std::max(waist,p.y);
    }
    if(bottomMoved) bottom.updateStretch();
    bool moved=false;
    for(size_t i=0;i<top.cloth.positions.size();++i) {
        Vec3& p=top.cloth.positions[i];
        if(p.y>waist+footprint+falloff) continue;
        float radius=std::hypot(p.x,p.z);
        if(radius<1e-5f) continue;
        Vec3 ray={p.x/radius,0,p.z/radius};
        int cell=sector(p);
        float target=radius;
        for(int k=-3;k<=3;++k) for(Vec3 v:bands[(cell+k+sectors)%sectors]) {
            float dy=std::abs(p.y-v.y);
            if(dy>=footprint+falloff) continue;
            float sideways=std::abs(ray.x*v.z-ray.z*v.x);
            float offset=std::max(0.f,dy-footprint)+std::max(0.f,sideways-footprint);
            target=std::max(target,dot(v,ray)+clearance-1.5f*offset);
        }
        if(target<=radius) continue;
        moved=true;
        const Vec3 before=p;
        p=top.contact.project(top.contact.dress(p+ray*(target-radius),clearance+.002f),clearance+.002f);
        Vec3 correction=p-before;
        top.previous[i]=top.previous[i]+correction;
    }
    if(moved) top.updateStretch();
}
} // namespace closet
