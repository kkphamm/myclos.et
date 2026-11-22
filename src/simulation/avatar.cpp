#include "avatar.hpp"
#include "cloth_math.hpp"
#include "measurements.hpp"
#include <algorithm>
#include <cstdint>

namespace closet {
namespace {
// Packed positions are in tenths of a millimeter. The two forms share one closed surface.
struct Sample {
    std::int16_t x,y,z;
    std::uint8_t arm,leg;
};
constexpr Sample female[]={
#include "../../assets/female-vertices.inc"
};
constexpr Sample male[]={
#include "../../assets/male-vertices.inc"
};
constexpr std::array<int,3> anatomy[]={
#include "../../assets/body-triangles.inc"
};
struct Offset { std::int16_t x,y,z; };
using MacroTargets=Offset[8];
constexpr MacroTargets femaleMacros[]={
#include "../../assets/female-hm08-macros.inc"
};
constexpr MacroTargets maleMacros[]={
#include "../../assets/male-hm08-macros.inc"
};
struct DisplayOffset {
    std::uint16_t anchor,partner;
    std::uint8_t blend;
    std::int16_t x,y,z;
};
constexpr DisplayOffset femaleDisplay[]={
#include "../../assets/female-display-offsets.inc"
};
constexpr DisplayOffset maleDisplay[]={
#include "../../assets/male-display-offsets.inc"
};
constexpr std::array<int,3> displayFaces[]={
#include "../../assets/hm08-display-triangles.inc"
};
constexpr int proxyMap[]={
#include "../../assets/hm08-proxy-map.inc"
};
constexpr std::array<int,3> proxyFaces[]={
#include "../../assets/hm08-proxy-triangles.inc"
};
static_assert(std::size(female)==std::size(male) && std::size(female)==std::size(proxyMap));
static_assert(std::size(femaleDisplay)==std::size(maleDisplay));

AvatarRest unpack(const Sample (&samples)[std::size(female)]) {
    AvatarRest rest;
    rest.mesh.positions.reserve(std::size(samples));
    rest.limbWeights.reserve(std::size(samples));
    for(auto v:samples) {
        rest.mesh.positions.push_back({v.x*.0001f,v.y*.0001f,v.z*.0001f});
        rest.limbWeights.push_back({v.arm/255.f,v.leg/255.f});
    }
    rest.mesh.triangles.assign(std::begin(anatomy),std::end(anatomy));
    return rest;
}
}

const AvatarRest& avatarRest(bool masculine) {
    if(masculine){static const auto model=unpack(male); return model;}
    static const auto model=unpack(female); return model;
}

Mesh bodyMesh(Body body,bool masculine) {
    using namespace cloth_math;
    const auto& rest=avatarRest(masculine);
    Mesh mesh=rest.mesh;
    const auto& targets=masculine?maleMacros:femaleMacros;
    const Body reference=masculine?masculinePresets[0]:presets[0];
    const float stature=body.height/reference.height;
    const float waist=body.waist/reference.waist/stature;
    const float chest=body.chest/reference.chest/stature;
    const std::array<float,4> macro={
        (body.height-170.f)/36.f,
        .9f*(waist-1.f),
        .7f*(chest-waist),
        .9f*(body.shoulder/reference.shoulder-body.hips/reference.hips)
    };
    for(size_t i=0;i<mesh.positions.size();++i) {
        for(size_t m=0;m<macro.size();++m) {
            const auto target=targets[i][2*m+(macro[m]>0)];
            const float amount=.0001f*std::min(1.f,std::abs(macro[m]));
            mesh.positions[i]=mesh.positions[i]+Vec3{target.x*amount,target.y*amount,target.z*amount};
        }
    }
    auto bounds=std::minmax_element(mesh.positions.begin(),mesh.positions.end(),[](Vec3 a,Vec3 b){return a.y<b.y;});
    const float sole=bounds.first->y,crown=bounds.second->y,height=body.height*.01f;
    float crotch=crown;
    for(Vec3 p:mesh.positions) if(p.x==0 && p.y>sole+.30f*(crown-sole)) crotch=std::min(crotch,p.y);
    const std::array<float,5> source={sole,crotch,mesh.positions[shoulderVertex].y,mesh.positions[chinVertex].y,crown};
    const std::array<float,5> landmark={0,.47f,.818f,1-1.f/7.5f,1};
    for(auto& p:mesh.positions) {
        auto end=std::lower_bound(source.begin()+1,source.end()-1,p.y);
        size_t i=end-source.begin();
        p.y=height*blend(landmark[i-1],landmark[i],(p.y-source[i-1])/(source[i]-source[i-1]));
    }
    for(int iteration=0;iteration<8;++iteration) {
        auto m=measureBody(mesh,masculine);
        if(std::abs(m.chest.cm-body.chest)<=.20f && std::abs(m.waist.cm-body.waist)<=.20f &&
           std::abs(m.hips.cm-body.hips)<=.20f && std::abs(m.shoulderWidthCm-body.shoulder)<=.20f) break;
        // A horizontal tape is linear in local girth scale, so its measured ratio gives a secant update.
        const float hips=body.hips/m.hips.cm,waistRatio=body.waist/m.waist.cm,chestRatio=body.chest/m.chest.cm;
        const float shoulders=body.shoulder/m.shoulderWidthCm;
        const float shoulderShift=.005f*(body.shoulder-m.shoulderWidthCm);
        for(size_t i=0;i<mesh.positions.size();++i) {
            Vec3& p=mesh.positions[i];
            const float y=p.y/height,arm=rest.limbWeights[i][0];
            float girth=blend(1.f,hips,smoothBetween(.29f,.48f,y));
            girth=blend(girth,waistRatio,smoothBetween(.555f,.593f,y));
            girth=blend(girth,chestRatio,smoothBetween(.645f,.681f,y));
            const float shoulderBand=smoothBetween(.751f,.800f,y);
            const float neck=smoothBetween(.825f,.863f,y);
            const float width=blend(blend(girth,shoulders,shoulderBand),1.f,neck);
            const float depth=blend(girth,1.f,std::max(shoulderBand,neck));
            const float freeArm=smoothBetween(.30f,.95f,arm)*(1-shoulderBand);
            p.x=blend(p.x*width,p.x+float((p.x>0)-(p.x<0))*shoulderShift,freeArm);
            p.z*=blend(depth,1.f,freeArm);
        }
    }
    return mesh;
}

Mesh displayBodyMesh(const Mesh& mesh,bool masculine,float heightCm) {
    using namespace cloth_math;
    const auto& offsets=masculine?maleDisplay:femaleDisplay;
    const float stature=heightCm/170.f;
    Mesh display;
    display.positions.reserve(std::size(offsets));
    for(auto d:offsets) {
        const Vec3 a=mesh.positions[d.anchor],b=mesh.positions[d.partner];
        Vec3 p=a+(b-a)*(d.blend/255.f)+Vec3{.0001f*d.x,.0001f*d.y*stature,.0001f*d.z};
        if(p.y<0) p.y=0;
        display.positions.push_back(p);
    }
    display.triangles.assign(std::begin(displayFaces),std::end(displayFaces));
    return display;
}

BodyContact fittingContact(const Mesh& mesh,bool masculine) {
    using namespace cloth_math;
    constexpr int count=1+*std::max_element(std::begin(proxyMap),std::end(proxyMap));
    const auto& weights=avatarRest(masculine).limbWeights;
    Mesh proxy;
    proxy.positions.resize(count);
    proxy.triangles.assign(std::begin(proxyFaces),std::end(proxyFaces));
    std::vector<std::array<float,2>> limbWeights(count);
    std::vector<int> samples(count);
    for(size_t i=0;i<mesh.positions.size();++i) {
        const int j=proxyMap[i];
        proxy.positions[j]=proxy.positions[j]+mesh.positions[i];
        limbWeights[j][0]+=weights[i][0];
        limbWeights[j][1]+=weights[i][1];
        ++samples[j];
    }
    for(int i=0;i<count;++i) {
        const float inv=1.f/samples[i];
        proxy.positions[i]=proxy.positions[i]*inv;
        for(float& w:limbWeights[i]) w*=inv;
    }
    return BodyContact(proxy,&limbWeights,&mesh,measureBody(mesh,masculine).waist.y);
}
} // namespace closet
