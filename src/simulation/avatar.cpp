#include "avatar.hpp"
#include "cloth_math.hpp"
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
static_assert(std::size(female)==std::size(male));

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
    const float stature=body.height/170.f, chest=body.chest/92.f, waist=body.waist/74.f;
    const float hip=body.hips/98.f, shoulder=body.shoulder/40.f;
    for(size_t i=0;i<mesh.positions.size();++i) {
        const Vec3 p=rest.mesh.positions[i];
        const float y=p.y/1.70f, side=float((p.x>0)-(p.x<0));
        // Keep the entire tape band at its specified girth, including the seat below the hip bones.
        float torso=blend(hip,waist,smoothBetween(.562f,.592f,y));
        torso=blend(torso,chest,smoothBetween(.643f,.682f,y));
        float shoulderBand=smoothBetween(.753f,.808f,y);
        float width=blend(torso,shoulder,shoulderBand),depth=blend(torso,.65f*chest+.35f*stature,shoulderBand);
        const float neck=smoothBetween(.828f,.864f,y);
        Vec3 out{p.x*blend(width,stature,neck),p.y*stature,p.z*blend(depth,stature,neck)};
        auto weights=rest.limbWeights[i];
        const float leg=weights[1]*(1-smoothBetween(.436f,.487f,y));
        if(leg>0) {
            const float thigh=smoothBetween(.19f,.445f,y);
            const float girth=blend(.8f*stature+.2f*hip,hip,thigh);
            const float axis=side*(masculine?.104f:.099f);
            const float stance=blend(stature,hip,thigh);
            out.x+=leg*(axis*stance+(p.x-axis)*girth-out.x);
            out.z+=leg*(p.z*girth-out.z);
        }
        // Shoulder width moves the deltoid. Preserve the chest tape on the ribcage side of the axilla.
        const float arm=blend(smoothBetween(.50f,.98f,weights[0]),weights[0],smoothBetween(.751f,.82f,y));
        if(arm>0) {
            const float upper=smoothBetween(.49f,.68f,y),cap=smoothBetween(.748f,.821f,y);
            const float girth=blend(.78f*stature+.22f*chest,chest,upper);
            const float axis=side*blend(masculine?.258f:.246f,.171f,smoothBetween(.58f,.815f,y));
            const float shoulderShift=.005f*(body.shoulder-40.f);
            const float torsoShift=std::max(.158f*(torso-1),.026f*(stature-1));
            const float shift=blend(std::max(shoulderShift,torsoShift),shoulderShift,cap);
            const float limbX=blend(axis+(p.x-axis)*girth+side*shift,p.x*shoulder,cap);
            out.x+=arm*(limbX-out.x);
            out.z+=arm*(p.z*girth-out.z);
        }
        mesh.positions[i]=out;
    }
    return mesh;
}
} // namespace closet
