#include "capture.hpp"
#include "renderer.hpp"
#include "simulation/cloth_math.hpp"
#include "simulation/measurements.hpp"
#include "simulation/run.hpp"
#include "ui/ui.hpp"
#include <OpenGL/gl3.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <limits>
#include <optional>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace closet {
namespace {
void saveFrame(const std::filesystem::path& file,int w,int h,GLuint framebuffer=0) {
    std::vector<unsigned char> pixels(size_t(w)*h*4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,framebuffer);
    glReadBuffer(framebuffer ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    if(framebuffer && glGetError()!=GL_NO_ERROR) throw std::runtime_error("Could not read the fit render target");
    const size_t stride=size_t(w)*4;
    for(int y=0;y<h/2;++y)
        std::swap_ranges(pixels.data()+stride*y,pixels.data()+stride*(y+1),pixels.data()+stride*(h-1-y));
    CGColorSpaceRef space=CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGDataProviderRef provider=CGDataProviderCreateWithData(nullptr,pixels.data(),pixels.size(),nullptr);
    CGImageRef image=CGImageCreate(w,h,8,32,stride,space,kCGImageAlphaNoneSkipLast,
                                   provider,nullptr,false,kCGRenderingIntentDefault);
    auto path=file.string();
    CFURLRef url=CFURLCreateFromFileSystemRepresentation(nullptr,
                    reinterpret_cast<const UInt8*>(path.data()),path.size(),false);
    CGImageDestinationRef dest=url?CGImageDestinationCreateWithURL(url,CFSTR("public.png"),1,nullptr):nullptr;
    bool ok=false;
    if(dest && image){CGImageDestinationAddImage(dest,image,nullptr);ok=CGImageDestinationFinalize(dest);}
    if(dest) CFRelease(dest);
    if(url) CFRelease(url);
    if(image) CGImageRelease(image);
    CGDataProviderRelease(provider); CGColorSpaceRelease(space);
    if(!ok) throw std::runtime_error("Could not write "+path);
}

void reportCloth(const Simulation& sim,const Mesh& mesh,const BodyMeasurements& form,
                 const BodyContact& surface,const char* preset,const char* outfit,int steps,bool settled) {
    using namespace cloth_math;
    const auto& points=sim.cloth.positions;
    const auto& item=items.at(sim.item);
    constexpr int ring=Simulation::ringSize;
    auto circumference=[&](int row) {
        float cm=0;
        for(int i=0;i<ring;++i) cm+=100*length(points[row*ring+i]-points[row*ring+(i+1)%ring]);
        return cm;
    };
    float hem=0,lowest=points.front().y;
    for(int i=0;i<ring;++i) {
        hem+=points[i].y/ring;
        lowest=std::min(lowest,points[i].y);
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const float height=form.crown-form.sole,highHip=(form.hips.y+form.waist.y)*.5f;
    const float hemTape=tapeCircumference(mesh,avatarRest(sim.masculine),hem);
    const float bustOrHip=circumference(Simulation::measureRow);
    const float hemCm=circumference(0);
    const float waistCm=item.bottom()?circumference(Simulation::panelRows):nan;
    float waistbandY=nan;
    if(item.bottom()) {
        waistbandY=0;
        for(int i=0;i<ring;++i) waistbandY+=points[Simulation::panelRows*ring+i].y/ring;
    }
    float shoulderGap=nan;
    int missingShoulders=0;
    if(!item.bottom()) for(int i:sim.supports()) {
        Vec3 p=points[i];
        float skin=surface.shoulderHeight(p.x,p.z,form.sole+.90f*height,nan);
        if(std::isfinite(skin)) shoulderGap=std::fmax(shoulderGap,100*(p.y-skin));
        else ++missingShoulders;
    }
    const Body b=sim.body;
    std::cout<<(sim.masculine?"masculine":"feminine")<<','<<preset<<','<<outfit<<','<<item.productId<<",M,"
             <<100*height<<','<<(form.crown-form.chin)/height<<','<<height/(form.crown-form.chin)<<','
             <<b.chest<<','<<form.chest.cm<<','<<100*form.chest.y<<','<<b.waist<<','<<form.waist.cm<<','<<100*form.waist.y<<','
             <<b.hips<<','<<form.hips.cm<<','<<100*form.hips.y<<','<<b.shoulder<<','<<form.shoulderWidthCm<<','
             <<100*form.crotch<<','<<100*highHip<<','<<(hem-form.sole)/height<<','<<100*(lowest-form.sole)<<','
             <<100*(hem-form.crotch)<<','<<100*(hem-highHip)<<','<<100*(hem-form.waist.y)<<','
             <<(item.bottom()?nan:bustOrHip)<<','<<(item.bottom()?nan:bustOrHip-form.chest.cm)<<','
             <<(item.bottom()?bustOrHip:nan)<<','<<(item.bottom()?bustOrHip-form.hips.cm:nan)<<','
             <<waistCm<<','<<waistCm-form.waist.cm<<','<<hemCm<<','<<hemCm-hemTape<<','<<shoulderGap<<','
             <<missingShoulders<<','<<1000*surface.minVertexDistance(sim.cloth)<<','<<steps*stepDt<<','<<settled<<','
             <<(item.bottom()?nan:item.sizes[sim.size].lengthCm)<<','
             <<(item.bottom()?nan:sim.backLengthCm(sim.restPositions))<<','
             <<(item.bottom()?nan:sim.backLengthCm(points))<<','<<100*waistbandY<<','
             <<100*(waistbandY-form.waist.y)<<','<<(item.bottom()?item.sizes[sim.size].waistCircCm:nan)<<'\n';
}
} // namespace

CaptureReplay::CaptureReplay(const std::filesystem::path& directory) : directory(directory) {
    if(!directory.empty()) std::filesystem::create_directories(directory);
}

void CaptureReplay::applyControls(Ui& ui) const {
    // Capture uses the same state transitions, renderer and cache as the controls.
    if(directory.empty() || renderedFrames!=2) return;
    if(stage==1){ui.body.waist=88; ui.yaw=.49f;}
    if(stage==2){ui.garmentSizes[0]=2; ui.heatmap=true; ui.wind=true;}
    if(stage==3){ui.store=true;}
    if(stage==4){ui.wear(3); ui.store=false; ui.heatmap=false;}
    if(stage==5){ui.store=true;}
    if(stage==6){ui.mensCollection=true; ui.setModel(true); ui.body=ui.modelPresets()[0]; ui.garmentSizes[0]=1;}
    if(stage==7){ui.wear(8); ui.wear(6); ui.store=false; ui.wind=false;}
    if(stage==8){ui.remove(0);}
    if(stage==9){ui.remove(1);}
    if(stage==10){ui.wear(9); ui.wear(7); ui.yaw=-.47f;}
    if(stage==11){ui.wear(10); ui.yaw=2.75f;}
    if(stage==12){ui.wear(3); ui.remove(0); ui.yaw=.30f;}
}

bool CaptureReplay::saveIfReady(bool previewsReady,int width,int height) {
    if(directory.empty() || ++renderedFrames<14 || !previewsReady) return false;
    const char* names[]={"try-on","waist-edit","stretch","store","skirt","store-warm","mens-closet","mens-try-on","pants-only","empty-outfit","oversized-outfit","jogger-outfit","skirt-only"};
    const auto path=directory/(std::string(names[stage])+".png");
    saveFrame(path,width,height);
    std::cout<<"Captured "<<path.string()<<" ("<<width<<"x"<<height<<")\n";
    renderedFrames=0;
    return ++stage==int(std::size(names));
}

void captureFitReport(const std::filesystem::path& directory,bool heroes) {
    constexpr const char* presetsNames[]={"classic","compact","tall"};
    constexpr const char* views[]={"front","side","back"};
    struct Scenario { const char* name; bool male; int preset,top,bottom=-1; };
    std::vector<Scenario> scenarios;
    if(heroes) scenarios={{"dry-ex-tank-and-narrow",true,0,5,2},
                         {"dry-ex-tee",true,0,6},
                         {"ribbed-tank-and-mermaid",false,0,0,3},
                         {"airism-tank",false,2,1}};
    else for(bool male:{false,true}) for(int preset=0;preset<3;++preset) {
        scenarios.push_back({"dry-ex-tank",male,preset,5});
        scenarios.push_back({"dry-ex-tee",male,preset,6});
        scenarios.push_back({"airism-and-mermaid",male,preset,1,3});
    }
    constexpr int medium=1;
    constexpr int width=600,height=800;
    std::filesystem::create_directories(directory);
    Renderer renderer;
    Target target;
    std::cout<<std::fixed<<std::setprecision(4)
             <<"model,preset,outfit,product,size,height_cm,head_fraction,heads_tall,requested_chest_cm,chest_cm,chest_y_cm,"
               "requested_waist_cm,waist_cm,waist_y_cm,requested_hips_cm,hips_cm,hips_y_cm,requested_shoulder_cm,shoulder_cm,"
               "crotch_y_cm,high_hip_y_cm,hem_fraction,lowest_hem_cm,hem_above_crotch_cm,hem_above_high_hip_cm,hem_above_waist_cm,"
               "cloth_chest_cm,chest_ease_cm,cloth_hip_cm,hip_ease_cm,cloth_waist_cm,waist_ease_cm,cloth_hem_cm,hem_ease_cm,"
               "max_shoulder_gap_cm,shoulder_points_off_body,min_cloth_vertex_to_body_mm,simulated_seconds,settled,"
               "chart_back_length_cm,rest_hps_to_hem_cm,draped_hps_to_hem_cm,waistband_y_cm,"
               "waistband_above_waist_cm,chart_waist_cm\n";
    for(const auto& [outfit,male,preset,topItem,bottomItem]:scenarios) {
        const Body body=(male?masculinePresets:presets)[preset];
        const auto avatar=bodyMesh(body,male);
        const auto display=displayBodyMesh(avatar,male,body.height);
        const auto form=measureBody(avatar,male);
        const auto contact=fittingContact(avatar,male);
        const BodyContact surface(avatar,&avatarRest(male).limbWeights);
        Simulation top(body,topItem,medium,male,contact);
        std::optional<Simulation> bottom;
        if(bottomItem>=0) bottom.emplace(body,bottomItem,medium,male,contact);
        bool settled=false;
        int steps=0;
        if(!bottom) steps=settle(top,settled);
        else {
            layerOutfit(top,*bottom);
            auto topBefore=top.cloth.positions,bottomBefore=bottom->cloth.positions;
            auto speed=[](const Simulation& sim,std::vector<Vec3>& before) {
                using namespace cloth_math;
                float sum=0;
                for(size_t i=0;i<before.size();++i) {
                    const Vec3 p=sim.cloth.positions[i],v=p-before[i];
                    sum+=dot(v,v);
                    before[i]=p;
                }
                return std::sqrt(sum/before.size())/stepDt;
            };
            for(int quiet=0;steps<maxSettleSteps && !settled;++steps) {
                top.step(stepDt);
                bottom->step(stepDt);
                layerOutfit(top,*bottom);
                quiet=std::max(speed(top,topBefore),speed(*bottom,bottomBefore))<quietSpeed ? quiet+1 : 0;
                settled=quiet>=quietSteps;
            }
        }
        if(!finiteState(top) || (bottom && !finiteState(*bottom)))
            throw std::runtime_error("Invalid fit report simulation");
        const auto stem=std::string(male?"masculine-":"feminine-")+presetsNames[preset]+'-'+outfit;
        reportCloth(top,avatar,form,surface,presetsNames[preset],outfit,steps,settled);
        if(bottom) reportCloth(*bottom,avatar,form,surface,presetsNames[preset],outfit,steps,settled);
        for(int view=0;view<3;++view) {
            renderer.draw(target,width,height,display,body.height,&top,bottom?&*bottom:nullptr,
                          {view*cloth_math::pi*.5f,0,1},false);
            saveFrame(directory/(stem+'-'+views[view]+".png"),width,height,target.framebuffer);
        }
        std::cout.flush();
    }
    renderer.release(target);
}
} // namespace closet
