#include "avatar.hpp"
#include "catalog.hpp"
#include "cloth_math.hpp"
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace closet {
using namespace cloth_math;

Vec3 Simulation::projectCloth(Vec3 p,float gap) const {
    if(items[item].pants() && p.y<.15f) {
        p.y=std::max(p.y,contact.floorHeight(p)+gap);
        p=contact.project(p,gap);
        p.y=std::max(p.y,contact.floorHeight(p)+gap);
        return p;
    }
    return contact.project(p,gap);
}

void Simulation::updateBody(Body b) {
    if(b==body) return;
    updateBody(b,fittingContact(bodyMesh(b,masculine),masculine));
}

void Simulation::updateBody(Body b,const BodyContact& fitted) {
    float lift=(b.height-body.height)*.01f*items[item].supportFraction();
    const auto& design=items[item];
    for(const auto& band:limbBands) {
        Vec3 base=band.center; base.y+=(body.height-170.f)*.01f*design.supportFraction();
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
    for(int i:anchorIndices) {
        Vec3 p=restPositions[i];
        if(design.bottom()) {
            p.y+=.01f*design.supportFraction()*(body.height-170.f);
            p=contact.dress(p,clearance);
        } else {
            p.x*=body.shoulder/40.f;
            p.y=contact.shoulderHeight(p.x,p.z,.0089f*body.height,.0082f*body.height)+clearance;
        }
        cloth.positions[i]=previous[i]=contact.project(p,clearance);
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
    float damping=std::exp(-4.f*dt);
    for(int i=0;i<int(p.size());++i) {
        if(anchored[i]) continue;
        Vec3 before=p[i]; p[i]=p[i]+(p[i]-previous[i])*damping+forces[i]*(dt*dt); previous[i]=before;
    }
    // Hooke springs act throughout the elastic range; five strain-limiting passes stabilize larger excursions.
    for(int pass=0;pass<5;++pass) {
        for(const auto& s:springs){
            Vec3 d=p[s.b]-p[s.a]; float len=length(d);
            if(len<1e-8f) continue;
            float error=len-s.rest;
            float band=s.rest*(s.surface?.015f:.04f);
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
        if(pass==4) {
            // An edge can cut the shoulder cap even when both endpoints clear it.
            for(const auto& s:springs) if(s.surface) {
                // A small armhole can stretch over the axilla and needs samples along its worn length.
                const float span=std::max(s.rest,length(p[s.a]-p[s.b]));
                int intervals=std::clamp(int(std::ceil(span/.009f)),3,12);
                for(int k=1;k<intervals;++k) {
                    float t=float(k)/intervals;
                    Vec3 sample=p[s.a]*(1-t)+p[s.b]*t;
                    Vec3 correction=projectCloth(sample,clearance)-sample;
                    float a=anchored[s.a]?0.f:1-t,b=anchored[s.b]?0.f:t;
                    float weight=a*a+b*b;
                    if(weight==0 || dot(correction,correction)<1e-12f) continue;
                    p[s.a]=p[s.a]+correction*(a/weight);
                    p[s.b]=p[s.b]+correction*(b/weight);
                }
            }
            for(auto f:cloth.triangles) {
                Vec3 center=(p[f[0]]+p[f[1]]+p[f[2]])*(1.f/3.f);
                Vec3 correction=projectCloth(center,clearance)-center;
                int free=3-anchored[f[0]]-anchored[f[1]]-anchored[f[2]];
                if(free && dot(correction,correction)>1e-12f)
                    for(int i:f) if(!anchored[i]) p[i]=p[i]+correction*(3.f/free);
            }
        }
        for(int i=0;i<int(p.size());++i) if(!anchored[i]) p[i]=projectCloth(p[i],clearance);
    }
    elapsed+=dt; rmsSpeed=0; insideCount=0; maxPenetration=0;
    const float slip=std::exp(-42.f*dt);
    for(int i=0;i<int(p.size());++i) {
        Vec3 v=p[i]-previous[i]; rmsSpeed+=dot(v,v);
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
    }
    rmsSpeed=std::sqrt(rmsSpeed/p.size())/dt;
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
