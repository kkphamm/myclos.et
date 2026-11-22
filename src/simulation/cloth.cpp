#include "avatar.hpp"
#include "catalog.hpp"
#include "cloth_math.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string_view>

namespace closet {
using namespace cloth_math;

Vec3 Simulation::projectCloth(Vec3 p,float gap) const {
    const auto& design=items[item];
    if(design.pants() && p.y<.15f) {
        const float ankle=design.cut==Cut::Joggers?.00032f*body.height:0.f;
        p.y=std::max(p.y,std::max(ankle,contact.floorHeight(p))+gap);
        p=contact.project(p,gap);
        p.y=std::max(p.y,std::max(ankle,contact.floorHeight(p))+gap);
        return p;
    }
    return contact.project(p,gap);
}

void Simulation::updateBody(Body b) {
    if(b==body) return;
    updateBody(b,fittingContact(bodyMesh(b,masculine),masculine));
}

float Simulation::supportOffset(Body form,const BodyContact& fitted) const {
    const auto& design=items[item];
    return design.elasticWaist()?fitted.waistHeight()-1.70f*design.supportFraction():
                                (form.height-170.f)*.01f*design.supportFraction();
}

void Simulation::updateBody(Body b,const BodyContact& fitted) {
    const float beforeOffset=supportOffset(body,contact);
    const float lift=supportOffset(b,fitted)-beforeOffset;
    const auto& design=items[item];
    for(const auto& band:limbBands) {
        Vec3 base=band.center; base.y+=beforeOffset;
        Vec3 before=contact.limbCenter(base.y,band.side,!design.pants(),base);
        Vec3 after=fitted.limbCenter(base.y+lift,band.side,!design.pants(),base);
        Vec3 delta{after.x-before.x,0,after.z-before.z};
        for(int j=band.first;j<band.first+band.count;++j) {
            cloth.positions[j]=cloth.positions[j]+delta;
            previous[j]=previous[j]+delta;
        }
    }
    body=b;
    contact=fitted;
    for(size_t i=0;i<cloth.positions.size();++i) if(!anchored[i]) {
        Vec3 before=cloth.positions[i],p=before;
        p.y+=lift;
        cloth.positions[i]=projectCloth(p,clearance);
        previous[i]=previous[i]+(cloth.positions[i]-before);
    }
    updateAnchors();
}

void Simulation::updateAnchors() {
    const auto& design=items[item];
    const float offset=supportOffset(body,contact);
    for(int i:anchorIndices) {
        if(!anchored[i]) continue;
        Vec3 p=restPositions[i];
        if(design.bottom()) {
            p.y+=offset;
            p=contact.dress(p,clearance,0,design.elasticWaist());
            p=contact.project(p,clearance);
        } else {
            if(design.tee()) p.x*=body.shoulder/40.f;
            else p.x+=std::copysign(1.f,p.x)*.002f*(body.shoulder-40.f);
            p.y=contact.shoulderHeight(p.x,p.z,.0089f*body.height,.0082f*body.height)+clearance;
        }
        cloth.positions[i]=previous[i]=p;
    }
}

void Simulation::selfContact() {
    constexpr float spacing=.006f;
    auto cell=[](float x){return int(std::floor(x/spacing));};
    auto bucket=[](int x,int y,int z){return (uint32_t(x)*73856093u ^ uint32_t(y)*19349663u ^ uint32_t(z)*83492791u)&4095u;};
    std::fill(gridHeads.begin(),gridHeads.end(),-1);
    auto& p=cloth.positions;
    for(int i=0;i<int(p.size());++i) {
        int x=cell(p[i].x),y=cell(p[i].y),z=cell(p[i].z);
        for(int dx=-1;dx<=1;++dx) for(int dy=-1;dy<=1;++dy) for(int dz=-1;dz<=1;++dz)
            for(int j=gridHeads[bucket(x+dx,y+dy,z+dz)];j>=0;j=gridNext[j]) {
                auto d=p[i]-p[j]; float d2=dot(d,d);
                if(d2>=spacing*spacing||d2<=1e-12f) continue;
                auto rest=restPositions[i]-restPositions[j];
                if(dot(rest,rest)<4*spacing*spacing) continue;
                float len=std::sqrt(d2);
                auto corr=d*((spacing-len)/len/(anchored[i]||anchored[j]?1.f:2.f));
                if(!anchored[i]) p[i]=p[i]+corr;
                if(!anchored[j]) p[j]=p[j]-corr;
            }
        int key=bucket(x,y,z); gridNext[i]=gridHeads[key]; gridHeads[key]=i;
    }
}

void Simulation::step(float dt,float wind,float direction) {
    if(!std::isfinite(dt)||dt<=0||dt>.034f) throw std::invalid_argument("Expected a positive simulation timestep <= 34 ms");
    if(dt>1.f/110.f) {
        int substeps=int(std::ceil(dt*120.f));
        for(int sub=0;sub<substeps;++sub) step(dt/substeps,wind,direction);
        return;
    }
    auto& p=cloth.positions;
    constexpr double supportRelease=.65;
    if(elapsed<supportRelease && elapsed+dt>=supportRelease)
        for(int i:temporarySupports) anchored[i]=0;
    std::fill(forces.begin(),forces.end(),Vec3{0,-9.81f,0});
    // Hooke acceleration for equal particle masses; k/m is 2100 s^-2 times each spring weight.
    for(const auto& s:springs) {
        Vec3 d=p[s.b]-p[s.a]; float len=length(d);
        if(len<1e-8f) continue;
        Vec3 acceleration=d*(2100.f*s.stiffness*(len-s.rest)/len);
        forces[s.a]=forces[s.a]+acceleration;
        forces[s.b]=forces[s.b]-acceleration;
    }
    if(wind!=0) {
        Vec3 flow={std::cos(direction),.12f,std::sin(direction)};
        float gust=(1.f+.24f*std::sin(3.5*elapsed)+.13f*std::sin(6.3*elapsed+.8))*wind;
        for(auto tri:cloth.triangles) {
            Vec3 normal=cross(p[tri[1]]-p[tri[0]],p[tri[2]]-p[tri[0]]);
            float area=length(normal);
            if(area<1e-9f) continue;
            float pressure=dot(normal,flow)/area;
            // Lump the triangle pressure across its vertices. Rest-area scaling approximates local mass.
            float restArea=length(cross(restPositions[tri[1]]-restPositions[tri[0]],restPositions[tri[2]]-restPositions[tri[0]]));
            Vec3 acceleration=normal*(gust*pressure*std::abs(pressure)*2.f/std::max(restArea,1e-8f));
            for(int i:tri) forces[i]=forces[i]+acceleration;
        }
    }
    const float damping=std::exp(-8.f*dt);
    for(int i=0;i<int(p.size());++i) {
        if(anchored[i]) continue;
        Vec3 before=p[i]; p[i]=p[i]+(p[i]-previous[i])*damping+forces[i]*(dt*dt); previous[i]=before;
    }
    const float calmRelaxation=std::string_view(items[item].productId)=="437410"?.20f:.15f;
    // Hooke springs act throughout the elastic range; five strain-limiting passes stabilize larger excursions.
    for(int pass=0;pass<5;++pass) {
        for(const auto& s:springs){
            Vec3 d=p[s.b]-p[s.a]; float len=length(d);
            if(len<1e-8f) continue;
            float error=len-s.rest;
            float band=s.rest*(s.surface?s.strainLimit:.04f);
            float excess=error-std::clamp(error,-band,band);
            auto corr=d*(s.stiffness*excess/len*(anchored[s.a]||anchored[s.b]?1.f:.5f));
            if(!anchored[s.a]) p[s.a]=p[s.a]+corr;
            if(!anchored[s.b]) p[s.b]=p[s.b]-corr;
        }
        for(auto tether:tethers) {
            Vec3 offset=p[tether.particle]-p[tether.anchor];
            float squared=dot(offset,offset);
            if(squared>tether.length*tether.length)
                p[tether.particle]=p[tether.anchor]+offset*(tether.length/std::sqrt(squared));
        }
        if(pass==3) selfContact();
        if(pass==4 && !wind) {
            // Dampen competing constraints before the final surface contact pass.
            const float relax=elapsed<.35?.62f:calmRelaxation;
            for(int i=0;i<int(p.size());++i) if(!anchored[i]) {
                Vec3 delta=p[i]-previous[i];
                const Vec3 touch=projectCloth(previous[i],clearance+.0005f)-previous[i];
                if(dot(delta,delta)<.0016f*.0016f && dot(touch,touch)>1e-10f) delta={};
                p[i]=previous[i]+delta*relax;
            }
        }
        if(pass>=3) {
            // Reserve interpolation error between the points sampled along a surface primitive.
            constexpr float surfaceGap=clearance+.0023f;
            // An edge can cut the shoulder cap even when both endpoints clear it.
            for(const auto& s:springs) if(s.surface) {
                // A small armhole can stretch over the axilla and needs samples along its worn length.
                const float span=std::max(s.rest,length(p[s.a]-p[s.b]));
                int intervals=std::clamp(int(std::ceil(span/.007f)),3,18);
                for(int k=1;k<intervals;++k) {
                    float t=float(k)/intervals;
                    Vec3 sample=p[s.a]*(1-t)+p[s.b]*t;
                    Vec3 correction=projectCloth(sample,surfaceGap)-sample;
                    float a=anchored[s.a]?0.f:1-t,b=anchored[s.b]?0.f:t;
                    const float distance=length(correction);
                    if(distance<1e-6f) continue;
                    correction=correction*std::min(1.f,.004f/distance);
                    // A sample next to a seat cannot move the free endpoint by a large lever arm.
                    const float weight=std::max(.25f,a*a+b*b);
                    p[s.a]=p[s.a]+correction*(a/weight);
                    p[s.b]=p[s.b]+correction*(b/weight);
                }
            }
            for(auto f:cloth.triangles) {
                Vec3 center=(p[f[0]]+p[f[1]]+p[f[2]])*(1.f/3.f);
                Vec3 correction=projectCloth(center,surfaceGap)-center;
                int free=3-anchored[f[0]]-anchored[f[1]]-anchored[f[2]];
                const float distance=length(correction);
                if(free && distance>1e-6f) {
                    const Vec3 push=correction*std::min(3.f/free,.006f/distance);
                    for(int i:f) if(!anchored[i]) p[i]=p[i]+push;
                }
            }
        }
        for(int i=0;i<int(p.size());++i) if(!anchored[i]) p[i]=projectCloth(p[i],clearance);
    }
    // Keep the upper panel below the shoulder when contact pushes from the armpit.
    const float support=.01f*(body.height-170.f)*items[item].supportFraction();
    float reseatMotion=0;
    for(int i:armholePanels) {
        if(anchored[i]) continue;
        Vec3 fit=restPositions[i];
        fit.y+=support;
        const float shoulder=.00845f*body.height-.55f*std::max(0.f,std::abs(fit.x)-.105f);
        const float ceiling=std::min(fit.y+.015f,shoulder);
        const float side=i%ringSize<ringSize/2?1.f:-1.f;
        const float depth=side*fit.z+.023f+.005f*(panelRows-i/ringSize);
        if(p[i].y<=ceiling && side*p[i].z<=depth) continue;
        Vec3 seated=p[i];
        seated.x=std::clamp(seated.x,fit.x-.024f,fit.x+.024f);
        seated.y=std::min(seated.y,ceiling);
        seated.z=side*std::min(side*seated.z,depth);
        seated.z=contact.panelDepth(seated,side,clearance+.0015f);
        p[i]=projectCloth(seated,clearance);
        const Vec3 displacement=p[i]-previous[i];
        reseatMotion+=dot(displacement,displacement);
        previous[i]=p[i];
    }
    const float priorSpeed=rmsSpeed;
    elapsed+=dt; rmsSpeed=reseatMotion; insideCount=0; maxPenetration=0;
    const float slip=std::exp(-84.f*dt);
    for(int i=0;i<int(p.size());++i) {
        Vec3 v=p[i]-previous[i];
        Vec3 touch=projectCloth(p[i],clearance+.0005f)-p[i];
        float gap=length(touch);
        if(gap>1e-5f) {
            // Friction damps sliding at the mannequin surface; free fabric retains its momentum.
            Vec3 n=touch*(1.f/gap);
            float vn=dot(v,n);
            previous[i]=p[i]-(v-n*vn)*slip-n*(std::max(vn,0.f)*.2f);
            float depth=length(contact.project(p[i],0)-p[i]);
            if(depth>1e-5f) { ++insideCount; maxPenetration=std::max(maxPenetration,depth); }
        }
        rmsSpeed+=dot(v,v);
    }
    rmsSpeed=std::sqrt(rmsSpeed/p.size())/dt;
    if(!wind && rmsSpeed<priorSpeed && priorSpeed>earlierSpeed) previous=p;
    earlierSpeed=priorSpeed;
    updateStretch();
}

void Simulation::updateStretch() {
    const auto& p=cloth.positions;
    std::fill(stretch.begin(),stretch.end(),0.f);
    for(const auto& s:springs) if(s.surface){
        float e=std::max(0.f,length(p[s.b]-p[s.a])/s.rest-1.f);
        stretch[s.a]+=e; stretch[s.b]+=e;
    }
    for(int i=0;i<int(p.size());++i) stretch[i]*=inverseEdgeCount[i];
}
} // namespace closet
