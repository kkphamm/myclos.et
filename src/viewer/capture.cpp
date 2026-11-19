#include "capture.hpp"
#include "ui/ui.hpp"
#include <OpenGL/gl3.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace closet {
namespace {
void saveFrame(const std::filesystem::path& file,int w,int h) {
    std::vector<unsigned char> pixels(size_t(w)*h*4);
    glBindFramebuffer(GL_READ_FRAMEBUFFER,0);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,w,h,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
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
    std::cout<<"Captured "<<path<<" ("<<w<<"x"<<h<<")\n";
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
    saveFrame(directory/(std::string(names[stage])+".png"),width,height);
    renderedFrames=0;
    return ++stage==int(std::size(names));
}
} // namespace closet
