#pragma once
#include "simulation/catalog.hpp"
#include <array>

namespace closet {
struct Camera {
    float yaw=0.30f, pitch=0.025f, zoom=1.0f;
    bool productView=false;
};
struct Target {
    unsigned texture=0, framebuffer=0, depth=0;
    int width=0, height=0;
};

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&)=delete;
    Renderer& operator=(const Renderer&)=delete;
    void draw(Target&, int width, int height, const Mesh& body, float heightCm,
              const Simulation* selected, const Simulation* other, Camera, bool heatmap);
    void release(Target&);
private:
    struct Vertex { Vec3 p,n; float elongation, trim; Vec3 grain; };
    std::array<std::vector<float>,items.size()*3> boundaries;
    std::vector<Vertex> vertices;
    unsigned program=0, vao=0, backgroundVao=0, vbo=0, ebo=0;
    unsigned studio=0, studioColor=0, studioDepth=0;
    unsigned lightFramebuffer=0, lightDepth=0, depthVao=0, depthProgram=0;
    int studioWidth=0, studioHeight=0;
    int modeLoc, cameraLoc, basisLoc, scaleLoc, focusLoc, colorLoc, footLoc, tailoringLoc;
    void upload(const Mesh&, const Simulation*, const std::vector<float>*);
    void drawGarment(const Simulation&);
};
}
