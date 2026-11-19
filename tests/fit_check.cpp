#include "simulation/catalog.hpp"
#include "simulation/run.hpp"
#include "ui/ui.hpp"
#include "simulation/avatar.hpp"
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace closet;
constexpr float bandHeight=.025f, tolerance=.000002f;

Vec3 subtract(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
int band(float y) { return int(std::floor(y/bandHeight)); }

struct Triangle {
    std::array<Vec3,3> p;
    Vec3 lo,hi,normal;
    Triangle(const Mesh& m,std::array<int,3> face)
        : p{m.positions.at(face[0]),m.positions.at(face[1]),m.positions.at(face[2])},lo(p[0]),hi(p[0]) {
        for(Vec3 v:p) {
            lo={std::min(lo.x,v.x),std::min(lo.y,v.y),std::min(lo.z,v.z)};
            hi={std::max(hi.x,v.x),std::max(hi.y,v.y),std::max(hi.z,v.z)};
        }
        Vec3 n=cross(subtract(p[1],p[0]),subtract(p[2],p[0]));
        float len=std::sqrt(dot(n,n));
        normal=len>0 ? Vec3{n.x/len,n.y/len,n.z/len} : Vec3{};
    }

    // Intersect this triangle with the other triangle's plane. Measure the cut
    // segment along the common line; boundary-only touches are valid contact.
    bool cut(const Triangle& other,Vec3 axis,float& lower,float& upper) const {
        std::array<float,3> d;
        for(int i=0;i<3;++i) d[i]=dot(other.normal,subtract(p[i],other.p[0]));
        auto [small,large]=std::minmax_element(d.begin(),d.end());
        if(*small>=-tolerance || *large<=tolerance) return false;
        lower=std::numeric_limits<float>::max(); upper=-lower;
        for(int i=0;i<3;++i) {
            int j=(i+1)%3;
            if((d[i]<0)==(d[j]<0)) continue;
            float t=d[i]/(d[i]-d[j]);
            float s=dot(axis,p[i])+t*dot(axis,subtract(p[j],p[i]));
            lower=std::min(lower,s); upper=std::max(upper,s);
        }
        return true;
    }
    bool crosses(const Triangle& b) const {
        if(hi.x<b.lo.x || lo.x>b.hi.x || hi.y<b.lo.y || lo.y>b.hi.y || hi.z<b.lo.z || lo.z>b.hi.z) return false;
        Vec3 axis=cross(normal,b.normal);
        float a0,a1,b0,b1;
        return cut(b,axis,a0,a1) && b.cut(*this,axis,b0,b1) &&
               std::min(a1,b1)-std::max(a0,b0)>tolerance*std::sqrt(dot(axis,axis));
    }
};

struct Counts {
    int clothFaces=0,bodyFaces=0,pairs=0,inside=0;
};

class BodyTriangles {
    std::vector<Triangle> triangles;
    int firstBand,lastBand;
    std::vector<std::vector<int>> bands;
public:
    explicit BodyTriangles(const Mesh& mesh) {
        triangles.reserve(mesh.triangles.size());
        for(auto face:mesh.triangles) triangles.emplace_back(mesh,face);
        firstBand=band(triangles.at(0).lo.y); lastBand=firstBand;
        for(const Triangle& t:triangles) {
            firstBand=std::min(firstBand,band(t.lo.y));
            lastBand=std::max(lastBand,band(t.hi.y));
        }
        bands.resize(lastBand-firstBand+1);
        for(int i=0;i<int(triangles.size());++i)
            for(int j=band(triangles[i].lo.y);j<=band(triangles[i].hi.y);++j) bands[j-firstBand].push_back(i);
    }

    bool contains(Vec3 p) const {
        // Offset the ray by less than a micron to avoid samples on a mesh symmetry plane.
        p.x+=.0000007f; p.y+=.0000003f;
        int j=band(p.y);
        if(j<firstBand || j>lastBand) return false;
        int hits=0;
        for(int i:bands[j-firstBand]) {
            const auto& t=triangles[i];
            if(p.x<t.lo.x || p.x>t.hi.x || p.y<t.lo.y || p.y>t.hi.y || p.z>t.hi.z+tolerance ||
               std::abs(t.normal.z)<1e-8f) continue;
            bool inFootprint=false;
            for(int k=0;k<3;++k) {
                Vec3 a=t.p[k],b=t.p[(k+1)%3];
                if((a.y>p.y)!=(b.y>p.y) && p.x<a.x+(p.y-a.y)*(b.x-a.x)/(b.y-a.y)) inFootprint=!inFootprint;
            }
            if(!inFootprint) continue;
            float distance=dot(t.normal,subtract(t.p[0],p))/t.normal.z;
            if(std::abs(distance)<=tolerance) return false;
            if(distance>0) ++hits;
        }
        return hits%2!=0;
    }

    Counts inspect(const Mesh& cloth,bool closed=true) const {
        Counts counts;
        if(closed) for(Vec3 p:cloth.positions) counts.inside+=contains(p);
        std::vector<bool> bodyHit(triangles.size());
        for(auto face:cloth.triangles) {
            Triangle t(cloth,face);
            const int begin=std::max(firstBand,band(t.lo.y)),end=std::min(lastBand,band(t.hi.y));
            bool any=false;
            for(int j=begin;j<=end;++j) for(int i:bands[j-firstBand]) {
                const auto& b=triangles[i];
                if(j!=std::max(begin,band(b.lo.y)) || !t.crosses(b)) continue;
                any=true; bodyHit[i]=true; ++counts.pairs;
            }
            counts.clothFaces+=any;
        }
        counts.bodyFaces=int(std::count(bodyHit.begin(),bodyHit.end(),true));
        return counts;
    }
};

void checkPredicates() {
    const Mesh tetra{{{0,0,0},{1,0,0},{0,1,0},{0,0,1}},{{0,2,1},{0,1,3},{0,3,2},{1,2,3}}};
    const Mesh slice{{{-.2f,.25f,.25f},{1.2f,.25f,.25f},{.5f,.25f,-.5f}},{{0,1,2}}};
    BodyTriangles fixture(tetra);
    auto hit=fixture.inspect(slice);
    if(hit.inside || hit.clothFaces!=1 || hit.bodyFaces!=3 || !fixture.contains({.1f,.1f,.1f}) ||
       fixture.contains({.9f,.9f,.9f}) || fixture.inspect(tetra).pairs)
        throw std::runtime_error("triangle intersection self-check failed");
}

bool checkOutfits() {
    Ui state;
    if(state.worn!=std::array<int,2>{0,2}) throw std::runtime_error("opening outfit missing a piece");
    state.setModel(true);
    if(!(state.body==masculinePresets[0])) throw std::runtime_error("wrong masculine preset");
    state.body.waist=85;
    state.setModel(false);
    if(state.body.waist!=85) throw std::runtime_error("model switch lost custom measurements");
    state.wear(8);
    state.wear(6);
    if(state.worn!=std::array<int,2>{6,8}) throw std::runtime_error("could not combine a tee with pants");
    state.garmentSizes[0]=0;
    state.wear(3);
    state.garmentSizes[state.slot]=2;
    state.wear(4);
    if(state.worn!=std::array<int,2>{4,3} || state.size()!=0)
        throw std::runtime_error("switching tops lost the skirt or the top size");
    state.remove(0);
    if(state.item()!=3 || state.size()!=2) throw std::runtime_error("removing top lost active skirt");
    state.remove(1);
    if(state.item()>=0 || state.worn[0]>=0) throw std::runtime_error("outfit did not empty");
    state.wear(1);
    if(state.item()!=1 || state.worn[1]>=0) throw std::runtime_error("could not dress empty figure");

    bool passed=true;
    std::cout<<"model,top,bottom,top_size,bottom_size,step,garment_intersections,body_intersections,inside_body_vertices\n";
    for(bool male:{false,true}) {
        const Body body=(male?masculinePresets:presets)[0];
        const auto avatar=bodyMesh(body,male);
        const auto contact=fittingContact(avatar,male);
        const BodyTriangles surface(avatar);
        Body edited=body;
        edited.waist+=6;
        const auto editedAvatar=bodyMesh(edited,male);
        const auto editedContact=fittingContact(editedAvatar,male);
        const BodyTriangles editedSurface(editedAvatar);
        std::vector<Simulation> drapes;
        for(int i=0;i<int(items.size());++i) {
            // Include mixed sizes as well as the opening M/M outfit.
            int size=(i==1 || i==4) ? 0 : (i==3 || i==5) ? 2 : 1;
            drapes.emplace_back(body,i,size,male,contact);
            bool settled;
            settle(drapes.back(),settled);
        }
        for(const auto& shirt:drapes) if(!items[shirt.item].bottom())
            for(const auto& lower:drapes) if(items[lower.item].bottom()) {
                auto top=shirt,bottom=lower;
                for(int step=0;step<=120;++step) {
                    if(step==61) {
                        top.updateBody(edited,editedContact);
                        bottom.updateBody(edited,editedContact);
                    }
                    if(step) {
                        top.step(stepDt,1.2f,.8f);
                        bottom.step(stepDt,1.2f,.8f);
                    }
                    layerOutfit(top,bottom);
                    if(step!=0 && step!=20 && step!=61 && step!=120) continue;
                    if(!finiteState(top) || !finiteState(bottom)) throw std::runtime_error("nonfinite outfit");
                    auto layers=BodyTriangles(bottom.cloth).inspect(top.cloth,false);
                    const auto& figure=step<61 ? surface : editedSurface;
                    const auto topHits=figure.inspect(top.cloth),bottomHits=figure.inspect(bottom.cloth);
                    const int bodyPairs=topHits.pairs+bottomHits.pairs,inside=topHits.inside+bottomHits.inside;
                    std::cout<<(male?"masculine":"feminine")<<','<<items[top.item].productId<<','
                             <<items[bottom.item].productId<<','<<"SML"[top.size]<<','<<"SML"[bottom.size]
                             <<','<<step<<','<<layers.pairs<<','<<bodyPairs<<','<<inside<<'\n';
                    passed=passed && !layers.pairs && !bodyPairs && !inside;
                }
            }
    }
    return passed;
}

bool checkEdits() {
    std::cout<<"model,product,waist_cm,chest_cm,step_after_edit,crossing_pairs,inside_cloth_vertices\n";
    bool passed=true;
    for(bool male:{false,true}) for(int item:{0,2,6,8,10}) {
        Body b=(male?masculinePresets:presets)[0];
        Simulation sim(b,item,1,male);
        bool settled;
        settle(sim,settled);
        for(int edit=0;edit<=10;++edit) {
            const Body& base=(male?masculinePresets:presets)[0];
            b.waist=base.waist+1.6f*edit;
            b.chest=base.chest+.8f*edit;
            if(items[item].pants()) {b.hips=base.hips+.9f*edit; b.height=base.height-.7f*edit;}
            sim.updateBody(b);
            BodyTriangles surface(bodyMesh(b,male));
            for(int frame=1;frame<=(edit==10?120:6);++frame) {
                sim.step(stepDt,.35f,.8f);
                if(frame!=1 && frame!=6 && frame!=120) continue;
                if(!finiteState(sim)) throw std::runtime_error("nonfinite cloth after body edit");
                Counts hit=surface.inspect(sim.cloth);
                std::cout<<(male?"masculine":"feminine")<<','<<items[item].productId<<','<<b.waist
                         <<','<<b.chest<<','<<frame<<','<<hit.pairs<<','<<hit.inside<<'\n';
                passed=passed && !hit.pairs && !hit.inside;
            }
        }
    }
    return passed;
}
}

int main(int argc,char** argv) try {
    using namespace closet;
    const bool all=argc==2 && std::string_view(argv[1])=="--all";
    checkPredicates();
    if(argc==2 && std::string_view(argv[1])=="--outfits") return checkOutfits()?0:1;
    if(argc==2 && std::string_view(argv[1])=="--edits") return checkEdits()?0:1;
    if(argc!=1 && !all) {std::cerr<<"Usage: mycloset_fit_check [--all|--edits|--outfits]\n"; return 2;}
    std::cout<<"preset,model,product,size,settling,crossing_cloth_faces,crossing_body_faces,crossing_pairs,inside_cloth_vertices\n";
    bool failed=false;
    for(int p=0;p<(all?int(presets.size()):1);++p) for(bool masculine:{false,true}) {
        const Body& body=(masculine?masculinePresets:presets)[p];
        BodyTriangles surface(bodyMesh(body,masculine));
        for(int item=0;item<int(items.size());++item) for(int size=all?0:1;size<=(all?2:1);++size) {
            Simulation sim(body,item,size,masculine);
            bool settled;
            settle(sim,settled);
            if(!finiteState(sim)) throw std::runtime_error("nonfinite cloth");
            const Counts hit=surface.inspect(sim.cloth);
            std::cout<<p<<','<<(masculine?"masculine":"feminine")<<','<<items[item].productId<<','<<"SML"[size]
                     <<','<<(settled?"settled":"timeout")<<','<<hit.clothFaces<<','<<hit.bodyFaces<<','<<hit.pairs<<','<<hit.inside<<std::endl;
            failed|=hit.pairs>0 || hit.inside>0;
        }
    }
    return failed?1:0;
} catch(const std::exception& e) {
    std::cerr<<e.what()<<'\n'; return 1;
}
