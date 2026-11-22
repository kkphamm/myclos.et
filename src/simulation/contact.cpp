#include "contact.hpp"
#include "closet.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace closet {
namespace {
Vec3 sub(Vec3 a,Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 cross(Vec3 a,Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 offset(Vec3 p,Vec3 n,float d) {return {p.x+d*n.x,p.y+d*n.y,p.z+d*n.z};}
float mix(float a,float b,float t) {return a+t*(b-a);}
float crossXZ(Vec3 a,Vec3 b) {return a.x*b.z-a.z*b.x;}
constexpr int radialSteps=96;
constexpr float tau=6.28318530718f;

struct Section {
    Vec3 center{};
    std::array<float,radialSteps> radii{};
};

struct BoundsXZ {
    float minX=1e4f,maxX=-1e4f,minZ=1e4f,maxZ=-1e4f;
    void include(Vec3 p) {
        minX=std::min(minX,p.x); maxX=std::max(maxX,p.x);
        minZ=std::min(minZ,p.z); maxZ=std::max(maxZ,p.z);
    }
    Vec3 center(Vec3 fallback) const {
        return minX>maxX ? fallback : Vec3{(minX+maxX)*.5f,fallback.y,(minZ+maxZ)*.5f};
    }
};

void fitSection(Section& section,std::vector<Vec3>& points) {
    if(points.size()<3) return;
    std::sort(points.begin(),points.end(),[](Vec3 a,Vec3 b) {return a.x==b.x?a.z<b.z:a.x<b.x;});
    // A closed panel surrounds both legs and spans small concavities between the breasts.
    std::vector<Vec3> hull;
    for(int pass:{0,1}) {
        const size_t start=hull.size();
        for(size_t j=0;j<points.size();++j) {
            Vec3 p=points[pass?points.size()-1-j:j];
            while(hull.size()>=start+2 && crossXZ(sub(hull.back(),hull[hull.size()-2]),sub(p,hull.back()))<=0)
                hull.pop_back();
            hull.push_back(p);
        }
        hull.pop_back();
    }
    if(hull.size()<3) return;
    // The mean of the convex vertices lies inside even a narrow, tilted section.
    for(Vec3 p:hull){section.center.x+=p.x;section.center.z+=p.z;}
    section.center.x/=hull.size(); section.center.z/=hull.size();
    for(int j=0;j<radialSteps;++j) {
        Vec3 ray{std::cos(j*tau/radialSteps),0,std::sin(j*tau/radialSteps)};
        float radius=std::numeric_limits<float>::infinity();
        for(size_t i=0;i<hull.size();++i) {
            Vec3 a=sub(hull[i],section.center),edge=sub(hull[(i+1)%hull.size()],hull[i]);
            float outward=crossXZ(ray,edge);
            if(outward>0) radius=std::min(radius,crossXZ(a,edge)/outward);
        }
        section.radii[j]=radius;
    }
}

struct Triangle {
    Vec3 a,b,c,ab,bc,ca,normal;
    float invAB,invBC,invCA;

    Triangle(Vec3 a,Vec3 b,Vec3 c) : a(a),b(b),c(c),ab(sub(b,a)),bc(sub(c,b)),ca(sub(a,c)),
        normal(cross(ab,bc)),invAB(1.f/std::max(1e-20f,dot(ab,ab))),
        invBC(1.f/std::max(1e-20f,dot(bc,bc))),invCA(1.f/std::max(1e-20f,dot(ca,ca))) {
        float n=std::sqrt(dot(normal,normal));
        if(n>0) normal={normal.x/n,normal.y/n,normal.z/n};
    }
    float squaredDistance(Vec3 p) const {
        auto edgeDistance=[](Vec3 edge,Vec3 delta,float reciprocal) {
            Vec3 d=offset(delta,edge,-std::clamp(dot(delta,edge)*reciprocal,0.f,1.f));
            return dot(d,d);
        };
        Vec3 pa=sub(p,a),pb=sub(p,b),pc=sub(p,c);
        if(dot(cross(ab,pa),normal)<0 || dot(cross(bc,pb),normal)<0 || dot(cross(ca,pc),normal)<0)
            return std::min({edgeDistance(ab,pa,invAB),edgeDistance(bc,pb,invBC),edgeDistance(ca,pc,invCA)});
        float h=dot(pa,normal);
        return h*h;
    }
};
}

struct BodyContact::Data {
    Vec3 origin;
    float cell, invCell, waist;
    int nx,ny,nz;
    std::vector<float> distance;
    std::vector<float> instep;
    std::vector<Triangle> triangles;
    std::vector<Triangle> shoulderTriangles;
    std::vector<Section> sections;
    std::vector<std::array<BoundsXZ,4>> limbs;

    Data(const Mesh& body,const std::vector<std::array<float,2>>* weights,const Mesh* fittedSurface,float naturalWaist) {
        if(body.positions.empty()||body.triangles.empty()) throw std::invalid_argument("Empty fitting figure");
        if(weights && weights->size()!=body.positions.size()) throw std::invalid_argument("Invalid fitting figure weights");
        Vec3 lo=body.positions.front(),hi=lo;
        for(Vec3 v:body.positions) {
            lo={std::min(lo.x,v.x),std::min(lo.y,v.y),std::min(lo.z,v.z)};
            hi={std::max(hi.x,v.x),std::max(hi.y,v.y),std::max(hi.z,v.z)};
        }
        waist=naturalWaist>0?naturalWaist:mix(lo.y,hi.y,.61f);
        if(fittedSurface) for(auto f:fittedSurface->triangles) {
            const auto& p=fittedSurface->positions;
            const float y=(p[f[0]].y+p[f[1]].y+p[f[2]].y)/3;
            if(y>mix(lo.y,hi.y,.72f) && y<mix(lo.y,hi.y,.91f))
                shoulderTriangles.emplace_back(p[f[0]],p[f[1]],p[f[2]]);
        }
        cell=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,.01f})/192.f;
        invCell=1.f/cell;
        // An off-center lattice avoids ray coincidences at symmetric mesh vertices and poles.
        origin={lo.x-4.317f*cell,lo.y-4.583f*cell,lo.z-4.143f*cell};
        nx=int(std::ceil((hi.x-origin.x)*invCell))+6;
        ny=int(std::ceil((hi.y-origin.y)*invCell))+6;
        nz=int(std::ceil((hi.z-origin.z)*invCell))+6;
        distance.assign(size_t(nx)*ny*nz,std::numeric_limits<float>::max());
        instep.assign(nx*nz,0.f);
        std::vector<unsigned char> crossings(distance.size(),0);
        std::vector<std::vector<Vec3>> slices(ny);
        sections.resize(ny);
        limbs.resize(ny);
        triangles.reserve(body.triangles.size());
        for(auto f:body.triangles) {
            const Triangle t(body.positions.at(f[0]),body.positions.at(f[1]),body.positions.at(f[2]));
            if(dot(t.normal,t.normal)<.5f) continue;
            triangles.push_back(t);
            const int x0=int((std::min({t.a.x,t.b.x,t.c.x})-origin.x)*invCell);
            const int x1=int((std::max({t.a.x,t.b.x,t.c.x})-origin.x)*invCell);
            const int y0=int((std::min({t.a.y,t.b.y,t.c.y})-origin.y)*invCell);
            const int y1=int((std::max({t.a.y,t.b.y,t.c.y})-origin.y)*invCell);
            const int z0=int((std::min({t.a.z,t.b.z,t.c.z})-origin.z)*invCell);
            const int z1=int((std::max({t.a.z,t.b.z,t.c.z})-origin.z)*invCell);
            // A low cuff can rest on the instep. Rasterize the upper foot surface on the floor.
            const float ankle=lo.y+.073f*(hi.y-lo.y);
            if(t.normal.y>.01f && std::min({t.a.y,t.b.y,t.c.y})<ankle) {
                const float det=crossXZ(t.ab,sub(t.c,t.a));
                for(int z=z0;z<=z1+1;++z) for(int x=x0;x<=x1+1;++x) {
                    Vec3 q{origin.x+x*cell-t.a.x,0,origin.z+z*cell-t.a.z};
                    float u=crossXZ(q,sub(t.c,t.a))/det,v=crossXZ(t.ab,q)/det;
                    if(u<0 || v<0 || u+v>1) continue;
                    float h=t.a.y+u*t.ab.y-v*t.ca.y;
                    if(h<=ankle) instep[z*nx+x]=std::max(instep[z*nx+x],h);
                }
            }
            const bool arm=weights && ((*weights)[f[0]][0]+(*weights)[f[1]][0]+(*weights)[f[2]][0])>1.5f;
            const bool leg=weights && ((*weights)[f[0]][1]+(*weights)[f[1]][1]+(*weights)[f[2]][1])>1.5f;
            for(int iy=y0+1;iy<=y1;++iy) {
                float y=origin.y+iy*cell;
                const Vec3 corners[]={t.a,t.b,t.c};
                for(int i=0;i<3;++i) {
                    Vec3 a=corners[i],b=corners[(i+1)%3];
                    if((a.y<y)==(b.y<y)) continue;
                    Vec3 point=offset(a,sub(b,a),(y-a.y)/(b.y-a.y));
                    if(!arm) slices[iy].push_back(point);
                    if(arm || leg) limbs[iy][2*int(arm)+int(point.x>0)].include(point);
                }
            }
            // Exact point-to-triangle distance at lattice samples adjacent to the skin.
            for(int z=z0-1;z<=z1+2;++z) for(int y=y0-1;y<=y1+2;++y) {
                float* row=&distance[(z*ny+y)*nx];
                Vec3 p{0,origin.y+y*cell,origin.z+z*cell};
                for(int x=x0-1;x<=x1+2;++x) {
                    p.x=origin.x+x*cell;
                    row[x]=std::min(row[x],t.squaredDistance(p));
                }
            }
            // Signed crossings along Z distinguish arm/torso and inter-leg gaps.
            const double den=double(t.ab.x)*(-t.ca.y)-double(t.ab.y)*(-t.ca.x);
            if(std::abs(den)<1e-15) continue;
            for(int y=y0;y<=y1+1;++y) for(int x=x0;x<=x1+1;++x) {
                double px=double(origin.x)+x*double(cell)-t.a.x,py=double(origin.y)+y*double(cell)-t.a.y;
                double u=(px*(-t.ca.y)-py*(-t.ca.x))/den, v=(t.ab.x*py-t.ab.y*px)/den;
                if(u<0 || v<0 || u+v>1) continue;
                double hit=t.a.z+u*t.ab.z-v*t.ca.z;
                int z=int(std::ceil((hit-origin.z)/cell));
                crossings[(z*ny+y)*nx+x]^=1;
            }
        }
        for(int i=0;i<ny;++i) fitSection(sections[i],slices[i]);
        for(float& d:distance) d=std::sqrt(d);
        // Extend distances into the interior and empty space by two chamfer sweeps.
        const int plane=nx*ny;
        for(int dir:{1,-1}) {
            int begin=dir>0?0:int(distance.size())-1;
            for(int z=0;z<nz;++z) for(int y=0;y<ny;++y) for(int x=0;x<nx;++x) {
                int i=begin+dir*((z*ny+y)*nx+x);
                float d=distance[i];
                if(x) d=std::min(d,distance[i-dir]+cell);
                if(y) d=std::min(d,distance[i-dir*nx]+cell);
                if(z) d=std::min(d,distance[i-dir*plane]+cell);
                distance[i]=d;
            }
        }
        for(int xy=0;xy<plane;++xy) {
            bool inside=false;
            for(int i=xy;i<int(distance.size());i+=plane) {
                inside^=bool(crossings[i]);
                if(inside) distance[i]=-distance[i];
            }
        }
    }

    float sample(Vec3 p,Vec3& gradient) const {
        Vec3 q{(p.x-origin.x)*invCell,(p.y-origin.y)*invCell,(p.z-origin.z)*invCell};
        if(q.x<0||q.y<0||q.z<0||q.x>=nx-1||q.y>=ny-1||q.z>=nz-1)
            return std::numeric_limits<float>::infinity();
        int x=int(q.x),y=int(q.y),z=int(q.z);
        float u=q.x-x,v=q.y-y,w=q.z-z;
        const float* a=&distance[(z*ny+y)*nx+x];
        const float* b=a+nx*ny;
        float a0=mix(a[0],a[1],u),a1=mix(a[nx],a[nx+1],u);
        float b0=mix(b[0],b[1],u),b1=mix(b[nx],b[nx+1],u);
        float front=mix(a0,a1,v),back=mix(b0,b1,v);
        gradient={invCell*mix(mix(a[1]-a[0],a[nx+1]-a[nx],v),mix(b[1]-b[0],b[nx+1]-b[nx],v),w),
                  invCell*mix(a1-a0,b1-b0,w),invCell*(back-front)};
        return mix(front,back,w);
    }
};

BodyContact::BodyContact(const Mesh& body,const std::vector<std::array<float,2>>* weights,const Mesh* fittedSurface,
                         float naturalWaist)
    : data(std::make_shared<Data>(body,weights,fittedSurface,naturalWaist)) {}

float BodyContact::waistHeight() const { return data->waist; }

Vec3 BodyContact::limbCenter(float y,int side,bool arm,Vec3 fallback) const {
    const float row=(y-data->origin.y)*data->invCell;
    if(row<0 || row>=data->ny-1) return fallback;
    const int i=int(row), limb=2*int(arm)+int(side>0);
    Vec3 a=data->limbs[i][limb].center(fallback), b=data->limbs[i+1][limb].center(fallback);
    return {mix(a.x,b.x,row-i),y,mix(a.z,b.z,row-i)};
}

float BodyContact::armGirth(float y,int side) const {
    const int row=std::clamp(int(std::lround((y-data->origin.y)*data->invCell)),0,data->ny-1);
    const auto& arm=data->limbs[row][2+int(side>0)];
    if(arm.minX>=arm.maxX || arm.minZ>=arm.maxZ) return 0;
    const float a=.433f*(arm.maxX-arm.minX),b=.5f*(arm.maxZ-arm.minZ);
    return .5f*tau*(3*(a+b)-std::sqrt((3*a+b)*(a+3*b)));
}

float BodyContact::floorHeight(Vec3 p) const {
    float u=(p.x-data->origin.x)*data->invCell,v=(p.z-data->origin.z)*data->invCell;
    if(u<0 || v<0 || u>=data->nx-1 || v>=data->nz-1) return 0;
    int x=int(u),z=int(v);
    const float* h=&data->instep[z*data->nx+x];
    return mix(mix(h[0],h[1],u-x),mix(h[data->nx],h[data->nx+1],u-x),v-z);
}

Vec3 BodyContact::dress(Vec3 p,float clearance,float armGap,bool snug) const {
    float y=(p.y-data->origin.y)*data->invCell;
    if(y<0 || y>=data->ny-1) return p;
    int row=int(y);
    const auto &a=data->sections[row],&b=data->sections[row+1];
    if(a.radii[0]<=0 || b.radii[0]<=0) return p;
    Vec3 center{mix(a.center.x,b.center.x,y-row),p.y,mix(a.center.z,b.center.z,y-row)};
    Vec3 ray=sub(p,center);
    float r=std::hypot(ray.x,ray.z);
    if(r<1e-7f) ray={0,0,1}; else {ray.x/=r;ray.z/=r;}
    float angle=std::atan2(ray.z,ray.x)*(radialSteps/tau);
    if(angle<0) angle+=radialSteps;
    int i=int(angle)%radialSteps,next=(i+1)%radialSteps;
    auto radius=[&](const Section& s){return mix(s.radii[i],s.radii[next],angle-std::floor(angle));};
    float skin=mix(radius(a),radius(b),y-row);
    // Use the actual gap between the arm and the torso when threading each side panel.
    if(std::abs(ray.x)>.25f) {
        const int arm=2+int(ray.x>0);
        const auto& lower=data->limbs[row][arm];
        const auto& upper=data->limbs[row+1][arm];
        if(lower.minX<=lower.maxX && upper.minX<=upper.maxX) {
            const float t=y-row;
            const float inner=ray.x>0?mix(lower.minX,upper.minX,t):mix(lower.maxX,upper.maxX,t);
            const float radius=(inner-center.x-std::copysign(armGap,ray.x))/ray.x, z=center.z+radius*ray.z;
            const float gap=std::max(mix(lower.minZ,upper.minZ,t)-z,z-mix(lower.maxZ,upper.maxZ,t));
            const float weight=std::clamp((.024f+armGap-gap)/.018f,0.f,1.f);
            if(radius>0) r=mix(r,std::min(r,radius-2*clearance),weight*weight*(3-2*weight));
        }
    }
    return offset(center,ray,snug?skin+clearance:std::max(r,skin+clearance));
}

Vec3 BodyContact::project(Vec3 point,float clearance) const {
    // Newton projection on the trilinear field also handles deep intersections after a body edit.
    for(int pass=0;pass<5;++pass) {
        Vec3 n{};
        float d=data->sample(point,n);
        if(d>=clearance-.000005f) return point;
        float n2=dot(n,n);
        if(n2<.025f) {n={0,0,1};n2=1;}
        point=offset(point,n,(clearance-d)/std::max(n2,.5f));
    }
    // Opposing surfaces in a narrow armpit can leave no feasible offset between them.
    // Escape the medial ridge along the first axis with space for the cloth thickness.
    constexpr Vec3 axes[]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    Vec3 gradient{};
    for(float span=0;;span+=data->cell) {
        for(Vec3 direction:axes) {
            if(data->sample(offset(point,direction,span),gradient)<clearance) continue;
            float lo=std::max(0.f,span-data->cell),hi=span;
            for(int i=0;i<7;++i) {
                float mid=.5f*(lo+hi);
                if(data->sample(offset(point,direction,mid),gradient)<clearance) lo=mid;
                else hi=mid;
            }
            return offset(point,direction,hi);
        }
    }
}

float BodyContact::panelDepth(Vec3 point,float direction,float clearance) const {
    Vec3 unused{};
    for(int i=0;i<32;++i) {
        const float distance=data->sample(point,unused);
        if(distance>=clearance) break;
        point.z+=direction*std::clamp(clearance-distance,.002f,.025f);
    }
    return point.z;
}

float BodyContact::shoulderHeight(float x,float z,float upperY,float fallbackY) const {
    float best=-std::numeric_limits<float>::infinity();
    const auto& surface=data->shoulderTriangles.empty()?data->triangles:data->shoulderTriangles;
    for(const auto& t:surface) {
        if(t.normal.y<=.0001f || x<std::min({t.a.x,t.b.x,t.c.x}) || x>std::max({t.a.x,t.b.x,t.c.x}) ||
           z<std::min({t.a.z,t.b.z,t.c.z}) || z>std::max({t.a.z,t.b.z,t.c.z})) continue;
        double den=double(t.ab.x)*(-t.ca.z)-double(t.ab.z)*(-t.ca.x);
        if(std::abs(den)<1e-15) continue;
        double px=x-t.a.x,pz=z-t.a.z;
        double u=(px*(-t.ca.z)-pz*(-t.ca.x))/den,v=(t.ab.x*pz-t.ab.z*px)/den;
        if(u<0||v<0||u+v>1) continue;
        float y=float(t.a.y+u*t.ab.y-v*t.ca.y);
        if(y<=upperY) best=std::max(best,y);
    }
    return std::isfinite(best)?best:fallbackY;
}

float BodyContact::minVertexDistance(const Mesh& cloth) const {
    float limit=std::numeric_limits<float>::infinity();
    for(Vec3 p:cloth.positions) for(const auto& t:data->triangles) {
        if(p.y-limit>std::max({t.a.y,t.b.y,t.c.y}) || p.y+limit<std::min({t.a.y,t.b.y,t.c.y}) ||
           p.x-limit>std::max({t.a.x,t.b.x,t.c.x}) || p.x+limit<std::min({t.a.x,t.b.x,t.c.x}) ||
           p.z-limit>std::max({t.a.z,t.b.z,t.c.z}) || p.z+limit<std::min({t.a.z,t.b.z,t.c.z})) continue;
        const float d=std::sqrt(t.squaredDistance(p));
        limit=std::min(limit,d);
    }
    return limit;
}
}
