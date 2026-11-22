#include "renderer.hpp"
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/gl3.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

namespace closet {
namespace {
constexpr int lightResolution=1024;
Vec3 sub(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
void add(Vec3& a, Vec3 b) { a.x+=b.x; a.y+=b.y; a.z+=b.z; }
float length(Vec3 a) { return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); }

constexpr char vs[] = R"GLSL(#version 330 core
layout(location=0) in vec3 p;
layout(location=1) in vec3 n;
layout(location=2) in float elongation;
layout(location=3) in float trim;
layout(location=4) in vec3 grain;
uniform mat3 basis;
uniform vec2 scale;
uniform float focus;
uniform int mode;
out vec3 normal, yarn, lightPoint;
out vec2 screen;
out float strain, edge;
void main() {
    normal=n; strain=elongation; edge=trim; yarn=grain;
    if(mode==0) {
        vec2 corner=vec2((gl_VertexID<<1)&2,gl_VertexID&2);
        screen=corner*2.-1.;
        gl_Position=vec4(screen,.9999,1);
    } else {
        vec3 key=normalize(vec3(-.80,.86,.64));
        vec3 right=normalize(cross(vec3(0,1,0),key)), up=cross(key,right);
        vec3 d=p-vec3(0,.92,0);
        vec3 lightClip=vec3(dot(d,right)/.74,dot(d,up)/1.10,-dot(d,key)/1.30);
        lightPoint=.5+.5*lightClip;
        if(mode==4) { gl_Position=vec4(lightClip,1); return; }
        vec3 view=transpose(basis)*(p-vec3(0,focus,0));
        screen=view.xy/scale;
        gl_Position=vec4(screen,-view.z*.25,1);
    }
}
)GLSL";
constexpr char fs[] = R"GLSL(#version 330 core
in vec3 normal, yarn, lightPoint;
in vec2 screen;
in float strain, edge;
uniform int mode;
uniform vec3 eye;
uniform sampler2DShadow lightDepth;
uniform vec4 fabric, footShadow[2];
uniform vec3 tailoring; // Cut, rest seam height and rest hip radius.
uniform vec2 scale;
uniform float focus;
out vec4 outColor;
float ellipse(vec2 p, vec2 radii) {
    vec2 v=p/radii;
    return exp(-dot(v,v));
}
float rib(float x) {
    // Fade yarn finer than a pixel to avoid moire when the viewer zooms out.
    return cos(x*6.283185)*(1.-smoothstep(.2,.55,fwidth(x)));
}
float stitch(float distance) {
    return 1.-smoothstep(.0006,.0014+fwidth(distance),abs(distance));
}
void main() {
    if(mode==0) {
        vec3 floorTone=vec3(.810,.756,.750), wallTone=vec3(.863,.804,.800);
        vec3 sky=mix(floorTone,wallTone,smoothstep(-.92,.45,screen.y));
        float softbox=ellipse(screen-vec2(-.20,.35),vec2(1.10,1.04));
        sky=mix(sky,vec3(.943,.908,.890),softbox);
        vec2 floorPos=screen*scale+vec2(0.,focus*sqrt(1.-eye.y*eye.y));
        float shadow=.12*ellipse(floorPos-vec2(.055,-.007),vec2(.38,.040));
        for(int i=0;i<2;++i)
            shadow+=.22*ellipse(floorPos-footShadow[i].xy,footShadow[i].zw);
        outColor=vec4(sky*(1.-shadow),1); return;
    }
    vec3 norm=normalize(normal);
    if(dot(norm,eye)<0.) norm=-norm;
    vec3 key=normalize(vec3(-.80,.86,.64));
    float diffuse=max(dot(norm,key),0.);
    float fill=max(dot(norm,normalize(vec3(.82,.35,.47))),0.);
    vec2 texel=1./vec2(textureSize(lightDepth,0));
    float sun=0., bias=.00065+.0011*(1.-diffuse);
    for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
        sun+=texture(lightDepth,vec3(lightPoint.xy+vec2(x,y)*texel*2.2,lightPoint.z-bias))/9.;
    float spec=pow(max(0.,dot(norm,normalize(key+eye))),16.)*sun;
    float rim=pow(1.-max(dot(eye,norm),0.),3.);
    float back=max(dot(norm,normalize(vec3(-.25,.48,-.84))),0.);
    float ambient=(.26+.08*(.5+.5*norm.y))*mix(.74,1.,sun);
    // The catalog swatches are display RGB. Apply studio irradiance in linear light.
    float light=ambient+.58*diffuse*mix(.25,1.,sun)+.16*fill+.16*back;
    vec3 col;
    if(mode==1) {
        float softKey=smoothstep(-.22,.86,dot(norm,key));
        float matteLight=(.58+.25*softKey+.11*fill+.05*back)*mix(.91,1.,sun);
        col=vec3(.84,.815,.79)*sqrt(matteLight)+.006*rim;
    } else if(mode==3) {
        float t=clamp(strain/.30,0.,1.);
        vec3 cold=vec3(.048,.40,.39), middle=vec3(.98,.71,.26), hot=vec3(.79,.16,.16);
        col=t<.5 ? mix(cold,middle,t*2.) : mix(middle,hot,(t-.5)*2.);
        col*=.86+.14*diffuse;
    } else {
        // Rest-space grain follows the panels even as their silhouette changes.
        float around=atan(yarn.x,yarn.z)/6.283185;
        float weave=fabric.a*(.021*rib(around*205.)+.008*rib(yarn.y*280.));
        if(fabric.a>.9) weave+=.037*rib(around*103.+.6*sin(around*69.12));
        float binding=1.-smoothstep(.004,.007,edge);
        float seam=1.-smoothstep(.0004,.0012+fwidth(edge),abs(edge-.007));
        col=fabric.rgb*pow(light+.13,1./2.2)*(1.+weave-.065*binding-.042*seam);
        col+=vec3((.007+.021*(1.-fabric.a))*spec+.013*rim);
        if(tailoring.x==1. && yarn.y>1.32 && abs(yarn.x)<.105) {
            float neckband=1.-smoothstep(.013,.018,edge);
            col*=1.-.12*neckband-.10*stitch(edge-.015);
        }
        if(tailoring.x==1. && yarn.y<1.27)
            col*=1.-.075*stitch(edge-.017)-.050*stitch(edge-.020);
        if(tailoring.x==2. || tailoring.x==3.) {
            float depth=tailoring.y-yarn.y;
            float waistband=1.-smoothstep(.03,.034,depth);
            float seams=stitch(depth-.033);
            float front=smoothstep(.012,.04,yarn.z);
            float pocket=abs(yarn.x)-tailoring.z*(.66+2.0*depth);
            seams+=front*stitch(pocket)*smoothstep(.032,.04,depth)*(1.-smoothstep(.15,.17,depth));
            if(tailoring.x==2.) {
                float fly=yarn.x-.018*(1.-smoothstep(.115,.155,depth));
                seams+=front*stitch(fly)*smoothstep(.032,.04,depth)*(1.-smoothstep(.15,.16,depth));
            } else {
                // The flap and stitching follow the rest-space side panel as the leg moves.
                vec2 panel=abs(vec2(yarn.z,depth-.345))-vec2(.059,.076);
                float box=max(panel.x,panel.y);
                float outside=smoothstep(.10,.15,abs(yarn.x));
                col*=1.+outside*.055*(1.-smoothstep(-.002,.003,box));
                seams+=outside*(stitch(box)+stitch(depth-.300)*(1.-smoothstep(-.002,.004,panel.x)));
                if(depth>.60) col*=1.-.12*(1.-smoothstep(.025,.030,edge));
            }
            col*=1.-.07*waistband-.18*min(seams,1.);
        }
        if(tailoring.x==4.) {
            float depth=tailoring.y-yarn.y;
            float band=1.-smoothstep(.028,.032,depth);
            col*=1.-.10*band-.08*stitch(depth-.031);
        }
        if(tailoring.x==5. && yarn.z<-.025) {
            float backSeam=yarn.y-tailoring.y-.19*abs(yarn.x);
            col*=1.-.12*stitch(backSeam)*(1.-smoothstep(.070,.096,abs(yarn.x)));
        }
    }
    outColor=vec4(col,1);
}
)GLSL";

GLuint compile(GLenum type,const char* source) {
    GLuint shader=glCreateShader(type);
    glShaderSource(shader,1,&source,nullptr);
    glCompileShader(shader);
    GLint ok=0;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) {
        char log[4096]={};
        glGetShaderInfoLog(shader,sizeof(log),nullptr,log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Studio shader compilation failed: ")+log);
    }
    return shader;
}

GLuint linkFragment(GLuint vertex,const char* source) {
    GLuint fragment=compile(GL_FRAGMENT_SHADER,source),p=glCreateProgram();
    glAttachShader(p,vertex); glAttachShader(p,fragment); glLinkProgram(p);
    glDeleteShader(fragment);
    GLint ok=0; glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok) {
        char log[4096]={}; glGetProgramInfoLog(p,sizeof(log),nullptr,log);
        glDeleteProgram(p);
        throw std::runtime_error(std::string("Studio shader link failed: ")+log);
    }
    return p;
}

// ImGui and the screenshot path share this context with the offscreen targets.
struct GLState {
    GLint viewport[4], drawFB, readFB, activeProgram, vertexArray, arrayBuffer, activeTexture, texture, sampler, renderbuffer, depthFunc;
    GLboolean depthMask, colorMask[4];
    static constexpr GLenum flags[]={GL_BLEND,GL_SCISSOR_TEST,GL_CULL_FACE,GL_DEPTH_TEST,GL_FRAMEBUFFER_SRGB,GL_MULTISAMPLE};
    GLboolean enabled[6];
    GLState() {
        glGetIntegerv(GL_VIEWPORT,viewport);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&drawFB);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&readFB);
        glGetIntegerv(GL_CURRENT_PROGRAM,&activeProgram);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vertexArray);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&arrayBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE,&activeTexture);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture);
        glGetIntegerv(GL_SAMPLER_BINDING,&sampler);
        glBindSampler(0,0);
        glGetIntegerv(GL_RENDERBUFFER_BINDING,&renderbuffer);
        glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
        glGetBooleanv(GL_DEPTH_WRITEMASK,&depthMask);
        glGetBooleanv(GL_COLOR_WRITEMASK,colorMask);
        for(int i=0;i<6;++i) enabled[i]=glIsEnabled(flags[i]);
    }
    ~GLState() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER,drawFB);
        glBindFramebuffer(GL_READ_FRAMEBUFFER,readFB);
        glBindRenderbuffer(GL_RENDERBUFFER,renderbuffer);
        glBindTexture(GL_TEXTURE_2D,texture);
        glBindSampler(0,sampler);
        glActiveTexture(activeTexture);
        glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
        glUseProgram(activeProgram);
        glBindVertexArray(vertexArray);
        glBindBuffer(GL_ARRAY_BUFFER,arrayBuffer);
        glDepthFunc(depthFunc);
        glDepthMask(depthMask);
        glColorMask(colorMask[0],colorMask[1],colorMask[2],colorMask[3]);
        for(int i=0;i<6;++i) { if(enabled[i]) glEnable(flags[i]); else glDisable(flags[i]); }
    }
};
}

Renderer::Renderer() {
    GLState state;
    GLuint vert=compile(GL_VERTEX_SHADER,vs);
    try {
        program=linkFragment(vert,fs);
        depthProgram=linkFragment(vert,"#version 330 core\nvoid main(){}");
    } catch(...) {
        glDeleteShader(vert);
        if(program) glDeleteProgram(program);
        throw;
    }
    glDeleteShader(vert);
    glUseProgram(depthProgram);
    glUniform1i(glGetUniformLocation(depthProgram,"mode"),4);
    modeLoc=glGetUniformLocation(program,"mode");
    cameraLoc=glGetUniformLocation(program,"eye");
    basisLoc=glGetUniformLocation(program,"basis");
    scaleLoc=glGetUniformLocation(program,"scale");
    focusLoc=glGetUniformLocation(program,"focus");
    colorLoc=glGetUniformLocation(program,"fabric");
    tailoringLoc=glGetUniformLocation(program,"tailoring");
    footLoc=glGetUniformLocation(program,"footShadow[0]");
    glGenFramebuffers(1,&studio);
    glGenRenderbuffers(1,&studioColor); glGenRenderbuffers(1,&studioDepth);
    glGenVertexArrays(1,&backgroundVao);
    glGenVertexArrays(1,&vao); glBindVertexArray(vao);
    glGenBuffers(1,&vbo); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glGenBuffers(1,&ebo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    constexpr size_t offsets[]={offsetof(Vertex,p),offsetof(Vertex,n),offsetof(Vertex,elongation),
                                offsetof(Vertex,trim),offsetof(Vertex,grain)};
    constexpr int sizes[]={3,3,1,1,3};
    for(GLuint i=0;i<5;++i) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i,sizes[i],GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsets[i]));
    }
    glGenVertexArrays(1,&depthVao); glBindVertexArray(depthVao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vec3),nullptr);
    glGenTextures(1,&lightDepth);
    glBindTexture(GL_TEXTURE_2D,lightDepth);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,lightResolution,lightResolution,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE,GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC,GL_LEQUAL);
    glGenFramebuffers(1,&lightFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER,lightFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,lightDepth,0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Unable to allocate fitting room shadow map");
}

Renderer::~Renderer() {
    glDeleteFramebuffers(1,&lightFramebuffer); glDeleteTextures(1,&lightDepth);
    glDeleteVertexArrays(1,&depthVao);
    glDeleteFramebuffers(1,&studio);
    glDeleteRenderbuffers(1,&studioColor); glDeleteRenderbuffers(1,&studioDepth);
    glDeleteVertexArrays(1,&backgroundVao);
    glDeleteVertexArrays(1,&vao);
    glDeleteBuffers(1,&vbo); glDeleteBuffers(1,&ebo);
    glDeleteProgram(program);
    glDeleteProgram(depthProgram);
}
void Renderer::release(Target& t) {
    glDeleteFramebuffers(1,&t.framebuffer);
    glDeleteRenderbuffers(1,&t.depth);
    glDeleteTextures(1,&t.texture);
    t={};
}

void Renderer::upload(const Mesh& mesh,const Simulation* sim,const std::vector<float>* trim) {
    if(mesh.positions.empty() || mesh.triangles.empty()) return;
    vertices.resize(mesh.positions.size());
    for(size_t i=0;i<vertices.size();++i) {
        vertices[i]={mesh.positions[i],{},sim ? sim->stretch[i] : 0.f,
                     trim ? (*trim)[i] : 0.f,sim ? sim->restPositions[i] : Vec3{}};
    }
    for(const auto& f:mesh.triangles) {
        auto& a=vertices[f[0]]; auto& b=vertices[f[1]]; auto& c=vertices[f[2]];
        Vec3 normal=cross(sub(b.p,a.p),sub(c.p,a.p));
        add(a.n,normal); add(b.n,normal); add(c.n,normal);
    }
    for(auto& v:vertices) {
        const float len=length(v.n);
        if(len>1.e-10f) v.n={v.n.x/len,v.n.y/len,v.n.z/len};
        else v.n={0,1,0};
    }
    glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);
    static_assert(sizeof(std::array<int,3>)==3*sizeof(GLuint));
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,mesh.triangles.size()*sizeof(mesh.triangles[0]),mesh.triangles.data(),GL_STREAM_DRAW);
    glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(mesh.triangles.size()*3),GL_UNSIGNED_INT,nullptr);
}

void Renderer::drawGarment(const Simulation& sim) {
    auto& trim=boundaries.at(sim.item*3+sim.size);
    if(trim.size()!=sim.cloth.positions.size()) {
        trim.assign(sim.restPositions.size(),.08f);
        std::map<std::pair<int,int>,int> counts;
        for(const auto& f:sim.cloth.triangles) {
            for(int i=0;i<3;++i) ++counts[std::minmax(f[i],f[(i+1)%3])];
        }
        for(const auto& entry:counts) {
            if(entry.second==1) {
                trim[entry.first.first]=0;
                trim[entry.first.second]=0;
            }
        }
        for(const auto& entry:counts) {
            auto [a,b]=entry.first;
            if(trim[a]==0 || trim[b]==0) {
                float distance=length(sub(sim.restPositions[a],sim.restPositions[b]));
                int far=trim[a]==0 ? b : a;
                trim[far]=std::min(distance,trim[far]);
            }
        }
    }
    const auto& item=items.at(sim.item);
    const std::string_view product=item.productId;
    float textureStrength=product=="427391" ? 1.f : product=="434168" ? .40f : item.bottom() ? .30f : .14f;
    const bool racerback=product=="433735";
    const float cut=item.tee() ? 1.f : item.cut==Cut::Joggers ? 3.f : item.pants() ? 2.f :
                    item.cut==Cut::Skirt ? 4.f : racerback ? 5.f : 0.f;
    const float hipRadius=item.pants()?std::abs(sim.restPositions[Simulation::trouserHipRow*Simulation::ringSize].x):0.f;
    constexpr int lastRing=Simulation::panelRows*Simulation::ringSize;
    const float seamY=item.cut==Cut::Skirt ? sim.restPositions[lastRing].y :
                      racerback ? sim.restPositions[lastRing+3*Simulation::ringSize/4].y-.090f :
                      sim.restPositions.front().y;
    glUniform3f(tailoringLoc,cut,seamY,hipRadius);
    glUniform4f(colorLoc,item.color.x,item.color.y,item.color.z,textureStrength);
    upload(sim.cloth,&sim,&trim);
}

void Renderer::draw(Target& t,int width,int height,const Mesh& body,float heightCm,
                    const Simulation* selected,const Simulation* other,Camera cam,bool heatmap) {
    if(width<1 || height<1) return;
    GLState state;
    if(!t.framebuffer) {
        glGenFramebuffers(1,&t.framebuffer);
        glGenTextures(1,&t.texture);
    }
    glBindFramebuffer(GL_FRAMEBUFFER,t.framebuffer);
    if(width!=t.width || height!=t.height) {
        glBindTexture(GL_TEXTURE_2D,t.texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t.texture,0);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Unable to allocate the studio render target");
        t.width=width; t.height=height;
    }
    glBindFramebuffer(GL_FRAMEBUFFER,studio);
    if(width!=studioWidth || height!=studioHeight) {
        const int samples=(int64_t(width)*height>2000000) ? 2 : 4;
        glBindRenderbuffer(GL_RENDERBUFFER,studioColor);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER,samples,GL_RGBA8,width,height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,studioColor);
        glBindRenderbuffer(GL_RENDERBUFFER,studioDepth);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER,samples,GL_DEPTH_COMPONENT24,width,height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,studioDepth);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Unable to allocate the multisampled studio framebuffer");
        studioWidth=width; studioHeight=height;
    }
    for(auto flag:GLState::flags) glDisable(flag);
    glDepthMask(GL_TRUE);
    const float depth=1;
    glBindTexture(GL_TEXTURE_2D,0);
    glBindFramebuffer(GL_FRAMEBUFFER,lightFramebuffer);
    glViewport(0,0,lightResolution,lightResolution);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS);
    glUseProgram(depthProgram);
    glBindVertexArray(depthVao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glClearBufferfv(GL_DEPTH,0,&depth);
    auto castShadow=[&](const Mesh& mesh) {
        glBufferData(GL_ARRAY_BUFFER,mesh.positions.size()*sizeof(Vec3),mesh.positions.data(),GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,mesh.triangles.size()*sizeof(mesh.triangles[0]),mesh.triangles.data(),GL_STREAM_DRAW);
        glDrawElements(GL_TRIANGLES,static_cast<GLsizei>(3*mesh.triangles.size()),GL_UNSIGNED_INT,nullptr);
    };
    castShadow(body);
    if(other) castShadow(other->cloth);
    if(selected) castShadow(selected->cloth);

    glBindFramebuffer(GL_FRAMEBUFFER,studio); glBindTexture(GL_TEXTURE_2D,lightDepth);
    glViewport(0,0,width,height); glDisable(GL_DEPTH_TEST); glEnable(GL_MULTISAMPLE);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearBufferfv(GL_DEPTH,0,&depth);
    glUseProgram(program); glBindVertexArray(backgroundVao);
    float cy=std::cos(cam.yaw), sy=std::sin(cam.yaw), cp=std::cos(cam.pitch), sp=std::sin(cam.pitch);
    const float basis[]={cy,0,-sy, -sy*sp,cp,-cy*sp, sy*cp,sp,cy*cp};
    glUniformMatrix3fv(basisLoc,1,GL_FALSE,basis);
    glUniform3f(cameraLoc,sy*cp,sp,cy*cp);
    float zoom=std::clamp(cam.zoom,.55f,2.5f);
    float aspect=float(width)/height;
    float bodyHeight=heightCm*.01f;
    // Keep each contact shadow beneath its sole as the figure orbits.
    float left[]={1.f,1.f}, right[]={-1.f,-1.f}, bottom[]={1.f,1.f}, top[]={-1.f,-1.f};
    for(Vec3 p:body.positions) if(p.y<bodyHeight*.006f) {
        const int i=p.x>0;
        float u=cy*p.x-sy*p.z, v=-sp*(sy*p.x+cy*p.z);
        left[i]=std::min(left[i],u); right[i]=std::max(right[i],u);
        bottom[i]=std::min(bottom[i],v); top[i]=std::max(top[i],v);
    }
    float shadows[2][4];
    for(int i=0;i<2;++i) {
        shadows[i][0]=left[i]<=right[i] ? (left[i]+right[i])*.5f : 100.f;
        shadows[i][1]=(bottom[i]+top[i])*.5f;
        shadows[i][2]=std::max(.025f,(right[i]-left[i])*.48f);
        shadows[i][3]=std::max(.008f,(top[i]-bottom[i])*.48f);
    }
    glUniform4fv(footLoc,2,&shadows[0][0]);
    float scaleY=std::max(bodyHeight*.564f,.50f/aspect)/zoom;
    const Simulation* subject=selected ? selected : other;
    float upperFocus=.50f;
    if(subject) upperFocus=items.at(subject->item).bottom() ? .45f : .69f;
    float focus=bodyHeight*(.50f+(upperFocus-.50f)*std::clamp(zoom-1.f,0.f,1.f));
    if(cam.productView && subject && !subject->cloth.positions.empty()){
        auto bounds=std::minmax_element(subject->cloth.positions.begin(),subject->cloth.positions.end(),
                                         [](Vec3 a,Vec3 b){return a.y<b.y;});
        float bottom=bounds.first->y, top=bounds.second->y;
        if(!items.at(subject->item).bottom()){
            for(Vec3 p:body.positions) top=std::max(top,p.y);
        }
        focus=.5f*(top+bottom);
        scaleY=std::max(.34f/aspect,(top-bottom)*.55f+.015f);
    }
    glUniform2f(scaleLoc,scaleY*aspect,scaleY);
    glUniform1f(focusLoc,focus);
    glUniform1i(modeLoc,0);
    glDrawArrays(GL_TRIANGLES,0,3);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL);
    glUniform1i(modeLoc,1);
    upload(body,nullptr,nullptr);

    glUniform1i(modeLoc,heatmap ? 3 : 2);
    if(other) drawGarment(*other);
    if(selected) drawGarment(*selected);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER,t.framebuffer);
    glBlitFramebuffer(0,0,width,height,0,0,width,height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
}
}
