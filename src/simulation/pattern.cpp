#include "avatar.hpp"
#include "catalog.hpp"
#include "cloth_math.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string_view>

namespace closet {
using namespace cloth_math;
namespace {
constexpr int perimeter=Simulation::ringSize, halfCols=perimeter/2, rows=Simulation::panelRows;
int id(int row,int col) { return row*perimeter+(col+perimeter)%perimeter; }
float tankTop(float sideAngle,float neckline,float underarm,int outer,int inner) {
    // sideAngle runs from the side seam to the center of one pattern panel.
    const float outerAngle=2.f*outer/halfCols,innerAngle=2.f*inner/halfCols;
    constexpr float outerY=.823f, innerY=.832f;
    if(sideAngle<outerAngle) {
        float t=sideAngle/outerAngle;
        return underarm+(outerY-underarm)*t*t*(3-2*t);
    }
    if(sideAngle<=innerAngle) return outerY+(innerY-outerY)*(sideAngle-outerAngle)/(innerAngle-outerAngle);
    float t=std::cos(sideAngle*pi*.5f)/std::cos(innerAngle*pi*.5f);
    const float scoop=1.f-std::sqrt(std::max(0.f,1.f-t*t));
    return neckline+(innerY-neckline)*scoop;
}
}

Simulation::Simulation(Body b,int itemIndex,int sizeIndex,bool male)
    : Simulation(b,itemIndex,sizeIndex,male,fittingContact(bodyMesh(b,male),male)) {}

Simulation::Simulation(Body b,int itemIndex,int sizeIndex,bool male,const BodyContact& fitted)
    : body(b),item(itemIndex),size(sizeIndex),masculine(male),contact(fitted) {
    if(item<0||item>=int(items.size())||size<0||size>2) throw std::invalid_argument("Invalid garment or size");
    if(items[item].pants()) draftTrousers();
    else draftPanels();
    auto braces=std::move(springs);
    std::set<std::pair<int,int>> edges;
    auto spring=[&](int a,int b,float k,bool surface) {
        if(a>b) std::swap(a,b);
        if(!edges.emplace(a,b).second) return;
        springs.push_back({a,b,length(cloth.positions[b]-cloth.positions[a]),k,surface});
    };
    for(auto tri:cloth.triangles) for(int k=0;k<3;++k) spring(tri[k],tri[(k+1)%3],.80f,true);
    for(auto s:braces) spring(s.a,s.b,s.stiffness,false);
    restPositions=cloth.positions;
    const auto& design=items[item];
    const float offset=(b.height-170.f)*.01f*design.supportFraction();
    std::vector<bool> onLimb(cloth.positions.size());
    const float hemY=design.pants()?limbBands.back().center.y+offset:0;
    for(const auto& band:limbBands) {
        Vec3 base=band.center; base.y+=offset;
        Vec3 delta{};
        if(design.pants()) {
            // Seat an overlong leg above the sole, distributing the break up the shin.
            delta.y=std::max(0.f,.052f-hemY)*std::clamp((.37f-base.y)/(.37f-hemY),0.f,1.f);
        }
        const float sampleY=design.pants()?std::max(.082f,base.y+delta.y):base.y;
        Vec3 center=contact.limbCenter(sampleY,band.side,!design.pants(),base);
        delta.x=center.x-base.x; delta.z=center.z-base.z;
        for(int j=band.first;j<band.first+band.count;++j) {
            cloth.positions[j]=cloth.positions[j]+delta;
            onLimb[j]=true;
        }
    }
    for(size_t j=0;j<cloth.positions.size();++j) {
        Vec3& p=cloth.positions[j]; p.y+=offset;
        if(!onLimb[j] && (design.bottom() || p.y<body.height*.00790f) &&
           (!design.pants() || p.y>body.height*.00505f)) p=contact.dress(p,clearance);
        p=projectCloth(p,clearance);
    }
    previous=cloth.positions;
    anchored.resize(previous.size());
    for(int i:anchorIndices) anchored[i]=1;
    // Long-range, tension-only constraints carry the weight to the straps or waistband.
    // Transverse slack lets the panel wrap around the body without stretching its length.
    for(int i=0;i<int(previous.size());++i) if(!anchored[i]) {
        for(int side=0;side<(design.bottom()?1:2);++side) {
            int anchor=-1;
            float limit=1e5f;
            for(int j:anchorIndices) {
                if(!design.bottom() && (restPositions[j].x>0)!=(side==1)) continue;
                Vec3 d=restPositions[i]-restPositions[j];
                const float slack=design.mens?.035f:.12f;
                float bound=length(d)*(design.mens?1.004f:1.015f)+slack*std::hypot(d.x,d.z)+.001f;
                if(bound<limit) {anchor=j; limit=bound;}
            }
            if(anchor>=0) tethers.push_back({i,anchor,limit});
        }
    }
    stretch.resize(previous.size()); forces.resize(previous.size());
    inverseEdgeCount.resize(previous.size());
    for(const auto& s:springs) if(s.surface){ ++inverseEdgeCount[s.a]; ++inverseEdgeCount[s.b]; }
    for(auto& count:inverseEdgeCount) count=1.f/std::max(1.f,count);
    gridHeads.resize(4096); gridNext.resize(previous.size());
    updateAnchors();
}

void Simulation::addQuad(int a,int b,int c,int d) {
    cloth.triangles.push_back({a,b,d});
    cloth.triangles.push_back({b,c,d});
}

void Simulation::draftPanels() {
    constexpr float patternHeight=1.70f;
    const Item& design=items[item];
    const auto& dims=design.sizes[size];
    const float lengthM=.01f*dims.lengthCm;
    constexpr int bustRow=measureRow, hipRow=measureRow;
    const bool tee=design.tee(), oversized=design.cut==Cut::OversizedTee;
    const float bustHeight=design.mens && design.cut==Cut::Tank?.661f:.682f;
    constexpr float waistHeight=.610f;
    const float panelDepth=oversized?.59f:(tee?.66f:.72f);
    const float ellipse=ellipsePerimeter(perimeter,panelDepth);
    const float neckHalf=oversized?.071f:.079f;
    constexpr float neckY=1.438f,innerCap=.009f,outerCap=.032f;
    float shoulderX=0,shoulderY=0;
    if(tee) {
        // The shoulder tape crosses the neck and follows the sloped seam out to each sleeve.
        const float halfSpan=.005f*dims.shoulderCm, drop=halfSpan-.205f;
        const float run=.114f+.40f*drop, fall=.066f+.9165f*drop;
        const float scale=(halfSpan-neckHalf)/std::hypot(run,fall);
        shoulderX=neckHalf+run*scale;
        shoulderY=neckY+innerCap-fall*scale-outerCap;
    }
    const bool ribbed=std::string_view(design.productId)=="427391";
    const bool racerback=std::string_view(design.productId)=="433735";
    const bool dryTank=std::string_view(design.productId)=="437410";
    const int outer=ribbed || dryTank ? 4 : 5;
    const int inner=tee?11:8;
    // Opening and strap widths are fractions of flat chest width measured in the product photographs.
    const float outerWidth=ribbed?.89f:(racerback?.61f:(dryTank?.78f:.71f));
    const float neckWidth=ribbed?.67f:(racerback?.43f:.53f);
    const float chestToHeight=.01f*dims.chestFlatCm/patternHeight;
    const float frontTop=ribbed?.742f:.838f-(design.mens && !dryTank?.43f:.34f)*chestToHeight;
    const float backTop=ribbed?.786f:.838f-(dryTank?.05f:(racerback?.13f:.17f))*chestToHeight;
    const float underarm=design.mens && !tee?.670f:.687f;
    const float hem=(tee?.840f:backTop)-lengthM/patternHeight;
    const float waistM=dims.waistCircCm*.01f;
    const float hipM=dims.hipCircCm*.01f;
    const float hipDepth=std::min(.18f,lengthM*.5f);
    const float slitM=.01f*dims.slitLengthCm;
    const int slitTop=slitM>0 ? std::clamp(int(std::lround(hipRow*slitM/(lengthM-hipDepth))),1,hipRow-1) : 0;
    constexpr int backColumn=perimeter*3/4;
    for(int row=0;row<=rows;++row) for(int c=0;c<perimeter;++c){
        float a=2*pi*c/perimeter-pi/2;
        float sx=std::sin(a), cz=std::cos(a); Vec3 pos;
        if(design.bottom()) {
            float depth, girth, fold=0;
            if(row>=hipRow) {
                float t=float(rows-row)/(rows-hipRow);
                depth=t*hipDepth;
                girth=waistM+(hipM-waistM)*t*(2-t);
            } else {
                if(slitTop && row<slitTop)
                    depth=lengthM-slitM*float(row)/slitTop;
                else
                    depth=hipDepth+(lengthM-hipDepth-slitM)*float(hipRow-row)/(hipRow-slitTop);
                float t=(depth-hipDepth)/(lengthM-hipDepth);
                girth=hipM*(1.f+.022f*t);
                if(design.flaredHem) {
                    float flare=std::max(0.f,(t-.29f)/.71f);
                    girth+=.71f*lengthM*flare*flare;
                    fold=flare*(.009f*std::cos(10*a+.3f)+.004f*std::cos(7*a));
                }
            }
            float radius=girth/ellipse;
            pos={sx*(radius+fold),patternHeight*waistHeight-depth,cz*(.72f*radius+fold)};
        } else {
            // One level ring just below the armholes measures exactly twice the published flat width.
            float sideAngle=2.f*std::min(c%halfCols,halfCols-c%halfCols)/halfCols;
            float top=tankTop(sideAngle,cz>0?frontTop:backTop,underarm,outer,inner);
            // The ribbed tank has a modest shirttail hem that rises at each side seam.
            const float hemRise=ribbed?.028f:(racerback?.019f:0.f);
            float bottom=hem+hemRise*std::pow(std::abs(sx),6.f);
            float y=row<=bustRow ? bottom+(bustHeight-bottom)*float(row)/bustRow
                                 : bustHeight+(top-bustHeight)*float(row-bustRow)/(rows-bustRow);
            const float centerZ=-.012f+.014f*smoothBetween(.53f,.62f,y)+.007f*smoothBetween(.65f,.715f,y)-.014f*smoothBetween(.736f,.806f,y);
            float waist=std::clamp((bustHeight-y)/.10f,0.f,1.f);
            float hip=std::clamp((waistHeight-y)/.10f,0.f,1.f);
            float taper=tee ? 1.f+(oversized?.035f:.050f)*std::clamp((bustHeight-y)/(bustHeight-hem),0.f,1.f)
                            : 1.f-(ribbed?.047f:.021f)*waist+(ribbed?.055f:.066f)*hip;
            if(dryTank) taper=1.f-.008f*waist+.010f*hip;
            float radius=.02f*dims.chestFlatCm*taper/ellipse;
            float ripple=(.003f*std::cos(5*a+8*y)+.002f*std::sin(8*a-5*y))*hip;
            float backInset=1;
            if(racerback && cz<0 && row>bustRow)
                backInset-=.31f*std::abs(cz)*float(row-bustRow)/(rows-bustRow);
            // The front and back panels turn toward each other as they reach the shoulder seam.
            float depthScale=1.f-.75f*smoothBetween(.728f,.850f,y);
            pos={sx*(radius+ripple)*backInset,patternHeight*y,centerZ+cz*(panelDepth*radius+ripple)*depthScale};
            if(!tee && row>bustRow) {
                const float k=sideAngle*halfCols*.5f, flat=.005f*dims.chestFlatCm;
                float rim=flat*neckWidth*std::abs(sx)/std::cos(pi*inner/halfCols);
                if(k<inner) rim=flat*blend(outerWidth,neckWidth,(k-outer)/(inner-outer));
                if(k<outer) rim=blend(radius,flat*outerWidth,k/outer);
                if(racerback && cz<0) rim*=backInset;
                pos.x=std::copysign(blend(std::abs(pos.x),rim,smoothBetween(bustRow,rows,float(row))),sx);
            }
            if(tee && row<bustRow) {
                // Excess panel width hangs in long folds to either side of the flatter front.
                float hang=1.f-float(row)/bustRow;
                float across=std::abs(sx),crease=across-.66f-.09f*hang;
                float folds=std::exp(-85.f*crease*crease)-.80f*std::exp(-120.f*(crease+.14f)*(crease+.14f));
                pos.z+=cz*(oversized?.021f:.010f)*folds*smoothBetween(0,.7f,hang);
            }
        }
        if(tee && row>bustRow) {
            // Grade the armhole over the shoulder. Extra shoulder width falls down the upper arm.
            const float side= c<halfCols/2 || c>perimeter-halfCols/2 ? -1.f : 1.f;
            const float k=std::min(c%halfCols,halfCols-c%halfCols);
            const float front=c<=halfCols?1.f:-1.f;
            Vec3 rim;
            if(k<=outer) {
                float t=k/outer;
                rim={side*(.171f+(shoulderX-.171f)*t),1.219f+(shoulderY-1.219f)*t,
                     front*(.065f*std::sin(t*pi*.75f)+.004f*t)};
            } else if(k<=inner) {
                float t=(k-outer)/(inner-outer);
                rim={side*(shoulderX+(neckHalf-shoulderX)*t),shoulderY+(neckY-shoulderY)*t,
                     front*blend(.050f,.032f,t)};
            } else {
                float t=(k-inner)/(halfCols*.5f-inner);
                const float arc=std::sqrt(std::max(0.f,1.f-(1-t)*(1-t)));
                rim={side*neckHalf*(1-t),neckY-(front>0?(oversized?.037f:.052f):.008f)*arc,
                     front*(.032f+(front>0?.070f:.034f)*arc)};
            }
            float ratio=float(row-bustRow)/(rows-bustRow);
            Vec3 chest=cloth.positions[id(bustRow,c)];
            pos=chest*(1-ratio)+rim*ratio;
        }
        cloth.positions.push_back(pos);
    }
    if(!design.bottom()) {
        // Measure the tape along the shaped center back. Its vertical projection is shorter.
        float low=-.08f, high=.14f;
        auto hemWeight=[](int row){return std::max(0.f,1.f-float(row)/bustRow);};
        for(int pass=0;pass<20;++pass) {
            float lift=(low+high)*.5f, tape=0;
            for(int r=1;r<=rows;++r) {
                Vec3 d=cloth.positions[id(r,backColumn)]-cloth.positions[id(r-1,backColumn)];
                d.y+=lift*(hemWeight(r)-hemWeight(r-1));
                tape+=length(d);
            }
            if(tape>lengthM) low=lift; else high=lift;
        }
        for(int r=0;r<bustRow;++r) for(int c=0;c<perimeter;++c)
            cloth.positions[id(r,c)].y+=high*hemWeight(r);
    }
    // The two lips of the walking slit share a single vertex at its apex.
    const int slitBase=int(cloth.positions.size());
    for(int row=0;row<slitTop;++row) {
        Vec3& original=cloth.positions[id(row,backColumn)];
        float gap=.004f*(1-float(row)/slitTop);
        Vec3 other=original;
        original.x+=gap;
        other.x-=gap;
        cloth.positions.push_back(other);
    }
    auto panelVertex=[&](int row,int column,int sector) {
        if(row<slitTop && column==backColumn && sector>=backColumn) return slitBase+row;
        return id(row,column);
    };
    // Both half-panels share their side-edge particles: a permanently sewn side seam.
    for(int r=0;r<rows;++r) for(int c=0;c<perimeter;++c)
        addQuad(panelVertex(r,c,c),panelVertex(r,c+1,c),panelVertex(r+1,c+1,c),panelVertex(r+1,c,c));
    if(design.bottom()) {
        for(int c=0;c<perimeter;++c) anchorIndices.push_back(id(rows,c));
    } else {
        // Bridge each shoulder between front and back. The unbridged edges form neck and armholes.
        const int across=tee?8:4;
        const int columns=inner-outer+1;
        for(int side: {0,1}) {
            int bridge[9][12];
            for(int j=0;j<=across;++j) for(int k=0;k<columns;++k) {
                int front= side==0 ? outer+k : halfCols-inner+k;
                int back=(perimeter-front)%perimeter;
                Vec3 a=cloth.positions[id(rows,front)],bpos=cloth.positions[id(rows,back)];
                if(j==0) bridge[j][k]=id(rows,front);
                else if(j==across) bridge[j][k]=id(rows,back);
                else {
                    float t=float(j)/across;
                    Vec3 p=a*(1-t)+bpos*t;
                    Vec3 mid=(a+bpos)*.5f;
                    const float outwards=float(side?k:columns-1-k)/(columns-1);
                    const float crown=tee ? mid.y+innerCap+(outerCap-innerCap)*outwards
                                          : patternHeight*(.846f-.023f*std::abs(mid.x)/.18f);
                    p.y+=(crown-mid.y)*std::sin(pi*t);
                    bridge[j][k]=int(cloth.positions.size()); cloth.positions.push_back(p);
                    if(j==across/2 && (!tee || std::abs(p.x)<.165f)) anchorIndices.push_back(bridge[j][k]);
                }
            }
            for(int j=0;j<across;++j) for(int k=0;k+1<columns;++k)
                addQuad(bridge[j][k],bridge[j][k+1],bridge[j+1][k+1],bridge[j+1][k]);
            if(tee) {
                std::vector<int> hole;
                const int sideColumn=side?halfCols:0, towardsFront=side?-1:1;
                for(int k=0;k<=outer;++k) hole.push_back(id(rows,sideColumn+towardsFront*k));
                for(int j=1;j<=across;++j) hole.push_back(bridge[j][side?columns-1:0]);
                for(int k=outer-1;k>0;--k) hole.push_back(id(rows,sideColumn-towardsFront*k));
                addSleeve(std::move(hole),side?1:-1);
            }
        }
    }
    auto brace=[&](int a,int b,float k) { springs.push_back({a,b,0,k,false}); };
    for(int r=0;r<=rows;++r) for(int c=0;c<perimeter;++c) {
        if(r<rows) brace(panelVertex(r,c,c),panelVertex(r+1,c+1,c),.40f);
        if(r+2<=rows) {
            brace(id(r,c),id(r+2,c),.045f);
            if(r<slitTop && c==backColumn)
                brace(panelVertex(r,c,c),panelVertex(r+2,c,c),.045f);
        }
        if(r>=slitTop || c+1!=backColumn)
            brace(panelVertex(r,c,c),panelVertex(r,c+2,c),.045f);
    }
}
} // namespace closet
