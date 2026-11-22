#include "measurements.hpp"
#include "cloth_math.hpp"
#include <limits>
#include <stdexcept>

namespace closet {
// Cauchy's formula measures a taut tape by averaging projected section widths.
float tapeCircumference(const Mesh& mesh,const AvatarRest& rest,float y) {
    std::vector<Vec3> slice;
    for(auto f:mesh.triangles) for(int k=0;k<3;++k) {
        int i=f[k],j=f[(k+1)%3];
        if(i>j || rest.limbWeights[i][0]+rest.limbWeights[j][0]>=1.1f) continue;
        Vec3 a=mesh.positions[i],b=mesh.positions[j];
        if((a.y>y)==(b.y>y)) continue;
        float t=(y-a.y)/(b.y-a.y);
        slice.push_back({a.x+t*(b.x-a.x),y,a.z+t*(b.z-a.z)});
    }
    if(slice.size()<3) return std::numeric_limits<float>::quiet_NaN();
    constexpr int directions=90;
    float sum=0;
    for(int k=0;k<directions;++k) {
        float dx=std::cos(cloth_math::pi*k/directions),dz=std::sin(cloth_math::pi*k/directions);
        float lo=1e3f,hi=-lo;
        for(Vec3 p:slice) {
            float projection=dx*p.x+dz*p.z;
            lo=std::min(lo,projection); hi=std::max(hi,projection);
        }
        sum+=hi-lo;
    }
    return 100*cloth_math::pi*sum/directions;
}

BodyMeasurements measureBody(const Mesh& mesh,bool masculine) {
    const auto& rest=avatarRest(masculine);
    auto extremes=std::minmax_element(mesh.positions.begin(),mesh.positions.end(),
                                      [](Vec3 a,Vec3 b){return a.y<b.y;});
    const float sole=extremes.first->y,crown=extremes.second->y,height=crown-sole;
    auto band=[&](float from,float to,bool narrowest=false) {
        Tape result{0,narrowest?1e3f:0.f};
        for(int i=0;i<=14;++i) {
            float y=sole+height*(from+(to-from)*i/14);
            float cm=tapeCircumference(mesh,rest,y);
            if(!std::isfinite(cm)) throw std::runtime_error("Missing body tape section");
            if(narrowest ? cm<result.cm : cm>result.cm) result={y,cm};
        }
        return result;
    };
    float crotch=crown;
    for(Vec3 p:mesh.positions) {
        if(std::abs(p.x)<1e-6f && p.y>sole+.30f*height) crotch=std::min(crotch,p.y);
    }
    const float chin=mesh.positions.at(chinVertex).y;
    const float shoulder=200*mesh.positions.at(shoulderVertex).x;
    const float chestY=sole+.72f*height;
    return {{chestY,tapeCircumference(mesh,rest,chestY)},band(.593f,.645f,true),band(.480f,.555f),
            sole,crown,chin,crotch,shoulder};
}
}
