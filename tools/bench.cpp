#include "simulation/run.hpp"
#include "simulation/catalog.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <utility>
#include <string>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

using namespace closet;
using Clock=std::chrono::steady_clock;
static void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }

// Follow the sewn openings; matching coordinates alone would miss disconnected sleeves or legs.
static void checkOpenings(const Mesh& mesh,int expected) {
    std::map<std::pair<int,int>,int> uses;
    std::vector<std::vector<int>> links(mesh.positions.size()),boundary(mesh.positions.size());
    for(auto f:mesh.triangles) for(int i=0;i<3;++i) {
        int a=f[i],b=f[(i+1)%3];
        require(a!=b && ++uses[std::minmax(a,b)]<=2,"invalid garment seam");
        links[a].push_back(b); links[b].push_back(a);
    }
    for(auto [edge,count]:uses) if(count==1) {
        auto [a,b]=edge;
        boundary[a].push_back(b); boundary[b].push_back(a);
    }
    auto components=[](const auto& neighbors) {
        int count=0;
        std::vector<bool> seen(neighbors.size());
        for(size_t i=0;i<seen.size();++i) if(!seen[i] && !neighbors[i].empty()) {
            ++count;
            std::vector<int> stack{int(i)}; seen[i]=true;
            while(!stack.empty()) {
                int current=stack.back(); stack.pop_back();
                for(int j:neighbors[current]) if(!seen[j]) {seen[j]=true; stack.push_back(j);}
            }
        }
        return count;
    };
    for(const auto& e:boundary) require(e.empty() || e.size()==2,"open seam ends in the middle of the fabric");
    require(components(links)==1 && components(boundary)==expected,"wrong number of sewn pieces or garment openings");
}

static float rightLegWidthAt(const Mesh& mesh,float height) {
    float lo=1.f,hi=-1.f;
    for(auto face:mesh.triangles) for(int k=0;k<3;++k) {
        Vec3 a=mesh.positions[face[k]],b=mesh.positions[face[(k+1)%3]];
        if((a.y>height)==(b.y>height)) continue;
        float x=a.x+(b.x-a.x)*(height-a.y)/(b.y-a.y);
        if(x<0) continue;
        lo=std::min(lo,x); hi=std::max(hi,x);
    }
    require(hi>lo,"missing right trouser leg at the knee");
    return hi-lo;
}

static void checkSleeves(const std::vector<Vec3>& points,const GarmentSize& chart) {
    const auto panelsEnd=points.begin()+(Simulation::panelRows+1)*Simulation::ringSize;
    auto distance=[](Vec3 a,Vec3 b){return std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);};
    for(int side:{-1,1}) {
        std::vector<Vec3> shoulder;
        // The center of each front-to-back shoulder bridge follows the sewing line.
        for(auto it=panelsEnd;it!=points.end();++it)
            if(side*it->x>0 && std::abs(it->z)<1e-6f) shoulder.push_back(*it);
        require(shoulder.size()>=2,"missing shoulder seam");
        auto outwards=[side](Vec3 a,Vec3 b){return side*a.x<side*b.x;};
        std::sort(shoulder.begin(),shoulder.end(),outwards);
        float tape=std::abs(shoulder.front().x);
        for(size_t j=1;j<shoulder.size();++j) tape+=distance(shoulder[j-1],shoulder[j]);
        require(std::abs(200*tape-chart.shoulderCm)<.03f,"shoulder seam differs from chart");
        auto cuff=std::max_element(panelsEnd,points.end(),outwards);
        tape+=distance(shoulder.back(),*cuff);
        require(std::abs(100*tape-chart.sleeveFromBackCm)<.03f,"center back to cuff differs from chart");
    }
}

static void checkTrouserLegs(const std::vector<Vec3>& points,const GarmentSize& chart) {
    // Each horizontal section below the fork measures two separate legs.
    auto inside=std::min_element(points.begin(),points.end(),[](Vec3 a,Vec3 b){
        return std::hypot(a.x,a.z)<std::hypot(b.x,b.z);
    });
    for(float side:{-1.f,1.f}) {
        std::map<float,std::vector<Vec3>,std::greater<float>> sections;
        for(auto p:points) if(p.y<=inside->y && (side*p.x>0 || std::hypot(p.x,p.z)<1e-5f))
            sections[p.y].push_back(p);
        float tape=0, thigh=0, cuff=0;
        Vec3 prev=*inside;
        for(auto& [y,ring]:sections) {
            require(ring.size()==Simulation::ringSize/2,"trouser leg missing a sewn section");
            auto inner=std::min_element(ring.begin(),ring.end(),[](Vec3 a,Vec3 b){return std::abs(a.x)<std::abs(b.x);});
            tape+=std::hypot(inner->x-prev.x,inner->y-prev.y,inner->z-prev.z); prev=*inner;
            float cx=0;
            for(auto p:ring) cx+=p.x/ring.size();
            std::sort(ring.begin(),ring.end(),[cx](Vec3 a,Vec3 b){return std::atan2(a.z,a.x-cx)<std::atan2(b.z,b.x-cx);});
            cuff=0;
            for(size_t i=0;i<ring.size();++i) {
                Vec3 a=ring[i],b=ring[(i+1)%ring.size()];
                cuff+=50*std::hypot(a.x-b.x,a.z-b.z);
            }
            if(y==inside->y) thigh=cuff;
        }
        require(std::abs(100*tape-chart.inseamCm)<.03f,"inside leg seam differs from chart");
        require(std::abs(thigh-chart.thighFlatCm)<.03f && std::abs(cuff-chart.hemFlatCm)<.03f,"flat leg width differs from chart");
    }
}

// Measure the material before putting it on a body.
static void checkChartPattern(int item,int size) {
    const Simulation sim(presets[0],item,size);
    const auto& pts=sim.restPositions;
    const auto& style=items[item];
    const auto& chart=style.sizes[size];
    constexpr int ring=Simulation::ringSize, top=Simulation::panelRows, landmark=Simulation::measureRow;
    checkOpenings(sim.cloth,style.pants()?3:(style.bottom()?2:4));
    auto girth=[&](int row){
        float metres=0;
        for(int i=0;i<ring;++i){
            Vec3 a=pts[row*ring+i], b=pts[row*ring+(i+1)%ring];
            metres+=std::hypot(a.x-b.x,a.z-b.z);
        }
        return metres*100;
    };
    if(style.pants()) {
        require(std::abs(girth(0)-chart.waistCircCm)<.03f && std::abs(girth(Simulation::trouserHipRow)-chart.hipCircCm)<.03f,"trouser waist or hips differ from chart");
        checkTrouserLegs(pts,chart);
        return;
    }
    require(pts.size()>=(top+1)*ring,"garment lacks chart landmarks");
    const int back=3*ring/4;
    float length=style.bottom()?100*(pts[top*ring+back].y-pts[back].y):sim.backLengthCm(pts);
    require(std::abs(length-chart.lengthCm)<.02f,"pattern length differs from size chart");
    if(style.bottom()) {
        require(std::abs(girth(top)-chart.waistCircCm)<.02f,"waist differs from size chart");
        require(std::abs(girth(landmark)-chart.hipCircCm)<.02f,"hip differs from size chart");
        if(chart.slitLengthCm>0) {
            size_t slitRows=pts.size()-(top+1)*ring;
            require(slitRows>0 && slitRows<landmark,"missing open back slit");
            float slit=100*(pts[slitRows*ring+30].y-pts[30].y);
            require(std::abs(slit-chart.slitLengthCm)<.02f,"back slit differs from size chart");
        }
    } else {
        require(std::abs(girth(landmark)*.5f-chart.chestFlatCm)<.02f,"chest differs from size chart");
        for(int k=0;k<ring;++k) {
            const Vec3 a=pts[top*ring+k],b=pts[top*ring+(ring+ring/2-k)%ring];
            require(std::hypot(a.x+b.x,a.y-b.y,a.z-b.z)<.00001f,"asymmetric neckline or armholes");
        }
        if(style.tee()) checkSleeves(pts,chart);
    }
}

static void checkAvatar(bool masculine) {
    auto mesh=bodyMesh(presets[2],masculine);
    require(!mesh.positions.empty() && !mesh.triangles.empty(),"missing avatar");
    const auto top=std::max_element(mesh.positions.begin(),mesh.positions.end(),
                                    [](Vec3 a,Vec3 b){return a.y<b.y;});
    require(std::abs(100.f*top->y-presets[2].height)<2.f,"avatar height differs from chosen stature");
    for(auto p:mesh.positions) require(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z),
                                       "nonfinite avatar deformation");
    std::map<std::pair<int,int>,int> edgeUses;
    for(auto face:mesh.triangles) {
        for(int i:face) require(i>=0 && size_t(i)<mesh.positions.size(),"invalid avatar triangle");
        for(int i=0;i<3;++i) ++edgeUses[std::minmax(face[i],face[(i+1)%3])];
    }
    for(const auto& edge:edgeUses) require(edge.second==2,"open or overlapping seam in fitting figure");
}

static void check() {
    for(bool masculine:{false,true}) checkAvatar(masculine);
    for(int item=0;item<int(items.size());++item) {
        for(int size=0;size<3;++size) checkChartPattern(item,size);
        Simulation sim(presets[0],item,1);
        const auto springs=sim.springs;
        for(int i=0;i<100;++i) sim.step(stepDt);
        const auto before=sim.cloth.positions, history=sim.previous;
        sim.updateBody(presets[2]);
        require(before.size()==sim.cloth.positions.size(),"morph changed cloth topology");
        size_t retained=0;
        for(size_t i=0;i<before.size();++i){
            auto a=before[i],b=sim.cloth.positions[i],old=history[i],now=sim.previous[i];
            retained+=std::abs((b.x-now.x)-(a.x-old.x))+
                      std::abs((b.y-now.y)-(a.y-old.y))+
                      std::abs((b.z-now.z)-(a.z-old.z))<1e-6f;
        }
        require(retained>before.size()*.90,"body edit discarded cloth velocity");
        for(int i=0;i<200;++i) sim.step(stepDt,.8f,.5f);
        require(finiteState(sim),"nonfinite state through morph and wind");
        require(sim.springs.size()==springs.size(),"morph changed spring count");
        for(size_t i=0;i<springs.size();++i)
            require(sim.springs[i].rest==springs[i].rest,"body edit resized garment");
        require(sim.stretch.size()==before.size(),"missing vertex stretch");
        for(float e:sim.stretch) require(std::isfinite(e) && e>=0,"invalid stretch value");
    }
    struct ShirtFit { float width{},girth{},cuffHeight{}; };
    ShirtFit shirts[2][3]{};
    for(int i=0;i<int(items.size());++i) if(!items[i].bottom()) for(int size=0;size<3;++size) {
        const Item& style=items[i];
        Simulation sim((style.mens?masculinePresets:presets)[0],i,size,style.mens);
        bool settled=false;
        settle(sim,settled);
        const auto& pts=sim.cloth.positions;
        const float back=sim.backLengthCm(pts);
        if(std::abs(back-style.sizes[size].lengthCm)>=2.f)
            throw std::runtime_error(std::string(style.name)+" "+"SML"[size]+" draped HPS length: "+
                                     std::to_string(back)+" cm, chart: "+std::to_string(style.sizes[size].lengthCm)+" cm");
        if(!style.tee()) continue;
        auto& fit=shirts[style.cut==Cut::OversizedTee][size];
        constexpr int n=Simulation::ringSize,start=n*Simulation::measureRow;
        float left=1,right=-1;
        for(int k=0;k<n;++k) {
            Vec3 a=pts[start+k],b=pts[start+(k+1)%n];
            left=std::min(left,a.x); right=std::max(right,a.x);
            fit.girth+=std::hypot(a.x-b.x,a.y-b.y,a.z-b.z);
        }
        fit.width=right-left;
        fit.cuffHeight=std::min_element(pts.begin()+(Simulation::panelRows+1)*n,pts.end(),
                                       [](Vec3 a,Vec3 b){return a.y<b.y;})->y;
    }
    for(int size=0;size<3;++size) {
        auto regular=shirts[0][size],oversized=shirts[1][size];
        require(oversized.width>regular.width+.025f && oversized.girth>regular.girth+.07f &&
                oversized.cuffHeight<regular.cuffHeight-.045f,"oversized tee lost its boxy silhouette or half sleeve");
    }
    float tailoredKnee=0,wideKnee=0,tailoredWaist=0,wideWaist=0;
    const auto model=masculinePresets[0];
    for(int i=0;i<int(items.size());++i) {
        const auto cut=items[i].cut;
        if(cut!=Cut::Trousers && cut!=Cut::WideTrousers) continue;
        Simulation sim(model,i,1,true);
        bool settled=false; settle(sim,settled);
        float knee=rightLegWidthAt(sim.cloth,.00285f*model.height);
        float waist=sim.cloth.positions.front().y;
        if(cut==Cut::Trousers) {tailoredKnee=knee; tailoredWaist=waist;}
        else {wideKnee=knee; wideWaist=waist;}
    }
    require(wideKnee>tailoredKnee+.02f && wideWaist>tailoredWaist+.015f,
            "wide trousers lost their knee volume or high rise");
    const DrapeKey original{presets[0],1};
    require(original==DrapeKey{presets[0],1},"identical drape should be reused");
    require(!(original==DrapeKey{presets[0],2}),"size change reused stale drape");
    require(!(original==DrapeKey{presets[0],1,true}),"model change reused stale drape");
    for(auto field:{&Body::height,&Body::chest,&Body::waist,&Body::hips,&Body::shoulder}){
        auto edited=original; edited.body.*field+=1;
        require(!(original==edited),"measurement change reused stale drape");
    }
    std::cout<<"Passed all "<<3*items.size()<<" chart-to-pattern comparisons, morphology, cloth state, stretch and cache-key checks.\n";
}

static float p95Percent(std::vector<float> values) {
    require(!values.empty(),"no samples for stretch percentile");
    const size_t i=static_cast<size_t>(std::ceil(.95*values.size()))-1;
    std::nth_element(values.begin(),values.begin()+i,values.end());
    return 100*values[i];
}

static std::string cpu() {
#ifdef __APPLE__
    char name[256]{}; size_t n=sizeof(name);
    if(sysctlbyname("machdep.cpu.brand_string",name,&n,nullptr,0)==0) return name;
#endif
    return "unreported";
}

int main(int argc,char** argv) try {
    if(argc==2 && std::string(argv[1])=="--check"){ check(); return 0; }
    if(argc!=2){std::cerr<<"Usage: mycloset_bench OUTPUT.csv | --check\n"; return 2;}
    std::ofstream out(argv[1]);
    out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<"case,body,size,phase,vertices,steps,dt_s,simulated_s,mean_ms_per_step,status,rms_speed_m_s,p95_stretch_percent,torso_p95_stretch_percent,inside_proxy,max_penetration_mm,cpu,build,seed\n";
    const auto machine=cpu();
    for(int p=0;p<3;++p) for(int size=0;size<3;++size) {
        Simulation sim(presets[p],0,size);
        auto log=[&](const char* experiment,const char* phase,int steps,double ms,const char* status){
            require(finiteState(sim)&&!sim.stretch.empty(),"invalid benchmark state");
            std::vector<float> torso;
            for(size_t i=0;i<sim.stretch.size();++i) {
                float y=sim.cloth.positions[i].y/(.01f*sim.body.height);
                if(y>.55f && y<.74f) torso.push_back(sim.stretch[i]);
            }
            out << std::setprecision(8) << experiment << ',' << p << ',' << "SML"[size]
                << ',' << phase << ',' << sim.stretch.size() << ',' << steps << ',' << stepDt
                << ',' << sim.elapsed << ',' << ms/steps << ',' << status << ',' << sim.rmsSpeed
                << ',' << p95Percent(sim.stretch) << ',' << p95Percent(std::move(torso))
                << ',' << sim.insideCount << ',' << 1000*sim.maxPenetration
                << ",\"" << machine << "\"," << MYCLOSET_BUILD_TYPE << ",none\n";
        };
        bool settled;
        auto start=Clock::now();
        int n=settle(sim,settled);
        log("fit_grid","static",n,std::chrono::duration<double,std::milli>(Clock::now()-start).count(),settled?"settled":"timeout");
        if(p==0 && size==1){
            for(int block=0;block<24;++block) {
                double ms=0;
                for(int i=0;i<10;++i){
                    auto body=presets[0]; body.waist+=16*std::min(1.f,(block*10+i+1)/120.f);
                    start=Clock::now();
                    sim.updateBody(body); sim.step(stepDt);
                    ms+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();
                }
                log("waist_edit",block<12?"ramp":"hold",10,ms,"sample");
            }
        }
    }
    out.close(); std::cout<<"Wrote "<<argv[1]<<"\n";
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
