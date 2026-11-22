#include "measurements.hpp"
#include "cloth_math.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace closet {
namespace {
// Tape edges binned by height, so each horizontal section only visits nearby edges.
class TapeSections {
public:
    TapeSections(const Mesh& mesh,const AvatarRest& rest) : positions(mesh.positions) {
        if(positions.empty()) return;
        auto [low,high]=std::minmax_element(positions.begin(),positions.end(),
                                            [](Vec3 a,Vec3 b){return a.y<b.y;});
        base=low->y;
        scale=high->y>low->y ? bins/(high->y-low->y) : 0.f;
        for(auto f:mesh.triangles) for(int k=0;k<3;++k) {
            int i=f[k],j=f[(k+1)%3];
            if(i>j || rest.limbWeights[i][0]+rest.limbWeights[j][0]>=1.1f) continue;
            float a=positions[i].y,b=positions[j].y;
            for(int bin=bucket(std::min(a,b));bin<=bucket(std::max(a,b));++bin) edges[bin].push_back({i,j});
        }
    }

    // Cauchy's formula measures a taut tape by averaging projected section widths.
    float circumference(float y) {
        slice.clear();
        if(!positions.empty()) for(auto [i,j]:edges[bucket(y)]) {
            Vec3 a=positions[i],b=positions[j];
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

private:
    static constexpr int bins=256;
    int bucket(float y) const {return std::clamp(int((y-base)*scale),0,bins-1);}
    const std::vector<Vec3>& positions;
    float base=0,scale=0;
    std::array<std::vector<std::array<int,2>>,bins> edges;
    std::vector<Vec3> slice;
};
}

float tapeCircumference(const Mesh& mesh,const AvatarRest& rest,float y) {
    return TapeSections(mesh,rest).circumference(y);
}

BodyMeasurements measureBody(const Mesh& mesh,bool masculine) {
    TapeSections tapes(mesh,avatarRest(masculine));
    auto extremes=std::minmax_element(mesh.positions.begin(),mesh.positions.end(),
                                      [](Vec3 a,Vec3 b){return a.y<b.y;});
    const float sole=extremes.first->y,crown=extremes.second->y,height=crown-sole;
    auto band=[&](float from,float to,bool narrowest=false) {
        Tape result{0,narrowest?1e3f:0.f};
        for(int i=0;i<=14;++i) {
            float y=sole+height*(from+(to-from)*i/14);
            float cm=tapes.circumference(y);
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
    return {{chestY,tapes.circumference(chestY)},band(.593f,.645f,true),band(.480f,.555f),
            sole,crown,chin,crotch,shoulder};
}
}
