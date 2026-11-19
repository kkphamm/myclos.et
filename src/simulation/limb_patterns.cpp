#include "catalog.hpp"
#include "cloth_math.hpp"
#include <algorithm>

namespace closet {
using namespace cloth_math;

// Sew a tube directly to its opening. The topological seam shares particles with the body panel.
void Simulation::extendBand(std::vector<int>& edge,const std::vector<Vec3>& points,int side,Vec3 center) {
    const int n=int(edge.size()),first=int(cloth.positions.size());
    const bool hasBend=limbBands.size()>=2 && limbBands[limbBands.size()-2].side==side;
    cloth.positions.insert(cloth.positions.end(),points.begin(),points.end());
    for(int k=0;k<n;++k) {
        int j=(k+1)%n;
        if(side>0) {
            addQuad(first+k,first+j,edge[j],edge[k]);
            springs.push_back({first+k,edge[j],0,.40f,false});
        } else {
            addQuad(edge[k],edge[j],first+j,first+k);
            springs.push_back({edge[k],first+j,0,.40f,false});
        }
        springs.push_back({first+k,first+(k+2)%n,0,.06f,false});
        if(hasBend)
            springs.push_back({limbBands[limbBands.size()-2].first+k,first+k,0,.06f,false});
    }
    for(int k=0;k<n;++k) edge[k]=first+k;
    limbBands.push_back({first,n,side,center});
}

void Simulation::addSleeve(std::vector<int> hole,int side) {
    const auto& d=items[item].sizes[size];
    std::vector<Vec3> root;
    Vec3 origin{};
    for(int i:hole) { root.push_back(cloth.positions[i]); origin=origin+cloth.positions[i]; }
    const int n=int(hole.size());
    origin=origin*(1.f/n);
    const Vec3 shoulder=root[n/2];
    // UNIQLO measures from the center back, across half the shoulder, then down the outer sleeve.
    const float sleeve=.01f*(d.sleeveFromBackCm-d.shoulderCm*.5f);
    const float cuffDepth=.85f;
    const float cuffFlat=.01f*d.chestFlatCm*(items[item].cut==Cut::OversizedTee?.42f:.40f);
    const float radius=2*cuffFlat/ellipsePerimeter(n,cuffDepth);
    Vec3 cuff{side*.242f,0,-.010f};
    float dx=cuff.x+side*radius-shoulder.x, dz=cuff.z-shoulder.z;
    constexpr float tilt=.014f;
    cuff.y=shoulder.y-std::sqrt(std::max(0.f,sleeve*sleeve-dx*dx-dz*dz))-tilt;
    for(int r=1;r<=9;++r) {
        float t=r/9.f;
        Vec3 center=origin*(1-t)+cuff*t;
        std::vector<Vec3> ring;
        for(int k=0;k<n;++k) {
            float a=2*pi*k/n;
            Vec3 end=cuff+Vec3{-side*radius*std::cos(a),-tilt*std::cos(a),radius*cuffDepth*std::sin(a)};
            ring.push_back(root[k]*(1-t)+end*t);
        }
        extendBand(hole,ring,side,center);
    }
}

void Simulation::draftTrousers() {
    const auto& design=items[item];
    const auto& d=design.sizes[size];
    constexpr int n=ringSize,legCols=n/2, hipRow=trouserHipRow, forkRow=11, legRows=27;
    const float waist=.01f*d.waistCircCm, hip=.01f*d.hipCircCm;
    const float thigh=.02f*d.thighFlatCm, cuff=.02f*d.hemFlatCm, inseam=.01f*d.inseamCm;
    const float waistY=1.70f*design.supportFraction();
    // These charts omit rise. Draft it from the hip ease and hold it fixed when the wearer changes.
    const bool wide=design.cut==Cut::WideTrousers, jogger=design.cut==Cut::Joggers;
    const float rise=(wide?.251f:(jogger?.218f:.227f))+.14f*(hip-waist);
    const float crotchY=waistY-rise;
    const float legDepth=wide?.86f:(jogger?.98f:1.03f);
    constexpr float hipDepth=.74f;
    const float hipUnit=ellipsePerimeter(n,hipDepth);
    std::array<Vec3,legCols> profile{};
    for(int k=0;k<legCols;++k) {
        float a=2*pi*k/legCols, x=-std::cos(a);
        // The pressed trouser leg has distinct front and back creases along the fabric grain.
        float crease=design.cut==Cut::Trousers ? 1.f-.22f*std::abs(x) : 1.f;
        profile[k]={x,0,legDepth*std::sin(a)*crease};
    }
    float legUnit=0;
    for(int k=0;k<legCols;++k) legUnit+=length(profile[k]-profile[(k+1)%legCols]);
    const float thighRadius=thigh/legUnit;
    auto fork=[&](int column){
        int side=(column>=n/4 && column<=3*n/4)?1:-1;
        int k=(side>0?column-n/4:n+n/4-column)%legCols;
        return Vec3{side*thighRadius*(1+profile[k].x),crotchY,thighRadius*profile[k].z};
    };
    std::array<int,n> previous{},ring{};
    for(int row=0;row<=forkRow;++row) {
        for(int c=0;c<n;++c) {
            // Front and back converge to one crotch vertex, splitting the seat into two leg openings.
            if(row==forkRow && c==3*n/4) {ring[c]=ring[n/4]; continue;}
            float a=2*pi*c/n;
            float t=std::min(1.f,float(row)/hipRow);
            float radius=(waist+(hip-waist)*t*(2-t))/hipUnit;
            Vec3 p{-radius*std::cos(a),waistY-.13f*t,hipDepth*radius*std::sin(a)};
            if(row>hipRow) {
                float s=float(row-hipRow)/(forkRow-hipRow);
                p=p*(1-s)+fork(c)*s;
            }
            ring[c]=int(cloth.positions.size());
            cloth.positions.push_back(p);
            if(!row) anchorIndices.push_back(ring[c]);
        }
        if(row) for(int c=0;c<n;++c) {
            int j=(c+1)%n;
            addQuad(ring[c],ring[j],previous[j],previous[c]);
            springs.push_back({ring[c],previous[j],0,.4f,false});
        }
        previous=ring;
    }
    for(int side:{-1,1}) {
        std::vector<int> opening(legCols);
        for(int k=0;k<legCols;++k) opening[k]=ring[(n+n/4+side*k)%n];
        Vec3 inner{0,crotchY,0};
        for(int row=1;row<=legRows;++row) {
            const float t=float(row)/legRows;
            float girth=cuff+(thigh-cuff)*std::pow(1-t,wide?.76f:(jogger?.69f:1.21f));
            if(jogger) {
                float ankle=std::clamp((1-t)/.095f,0.f,1.f);
                girth+=.085f*t*ankle*ankle*(3-2*ankle);
            }
            float rx=girth/legUnit;
            Vec3 center{side*(thighRadius*(1-t)+.116f*t),0,0};
            float innerX=center.x-side*rx, step=inseam/legRows;
            // Walk the actual inside seam. Its diagonal sections consume material length too.
            center.y=inner.y-std::sqrt(std::max(0.f,step*step-(innerX-inner.x)*(innerX-inner.x)));
            inner={innerX,center.y,0};
            std::vector<Vec3> points;
            for(Vec3 v:profile) points.push_back(center+Vec3{side*rx*v.x,0,rx*v.z});
            extendBand(opening,points,side,center);
        }
    }
}
} // namespace closet
