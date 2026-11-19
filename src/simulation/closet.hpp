#pragma once
#include "contact.hpp"
#include <array>
#include <vector>

namespace closet {
struct Vec3 { float x{}, y{}, z{}; };
struct Mesh { std::vector<Vec3> positions; std::vector<std::array<int,3>> triangles; };
// Dimensions are cm. Mesh positions are metres, Y up, +Z toward the viewer.
struct Body { float height=170, chest=92, waist=74, hips=98, shoulder=40; };
inline constexpr std::array<Body,3> presets = {{{170,92,74,98,40},{160,84,68,90,37},{180,106,90,112,46}}};
inline constexpr std::array<Body,3> masculinePresets = {{{178,99,82,98,44},{167,90,76,92,41},{185,108,90,105,47}}};
inline bool operator==(Body a,Body b) {
    return a.height==b.height && a.chest==b.chest && a.waist==b.waist && a.hips==b.hips && a.shoulder==b.shoulder;
}
Mesh bodyMesh(Body body, bool masculine=false);
struct Spring { int a,b; float rest, stiffness; bool surface; };
struct Simulation {
    static constexpr int ringSize=56, panelRows=26, measureRow=17, trouserHipRow=5;
    Body body; int item, size; bool masculine; Mesh cloth;
    std::vector<Vec3> restPositions, previous; std::vector<Spring> springs; std::vector<float> stretch;
    double elapsed=0; float rmsSpeed=0, maxPenetration=0; int insideCount=0;
    Simulation(Body b, int itemIndex, int sizeIndex, bool masculine=false);
    Simulation(Body b, int itemIndex, int sizeIndex, bool masculine, const BodyContact& contact);
    void step(float dt, float wind=0, float direction=0);
    void updateBody(Body b);
    void updateBody(Body b, const BodyContact& contact);
private:
    friend void layerOutfit(Simulation& top, Simulation& bottom);
    BodyContact contact;
    struct Tether { int particle,anchor; float length; };
    std::vector<Tether> tethers;
    std::vector<int> anchorIndices, gridHeads, gridNext;
    std::vector<unsigned char> anchored;
    std::vector<float> inverseEdgeCount;
    std::vector<Vec3> forces;
    struct LimbBand { int first,count,side; Vec3 center; };
    std::vector<LimbBand> limbBands;
    void draftPanels();
    void draftTrousers();
    void addQuad(int a,int b,int c,int d);
    void extendBand(std::vector<int>& edge,const std::vector<Vec3>& points,int side,Vec3 center);
    void addSleeve(std::vector<int> armhole,int side);
    Vec3 projectCloth(Vec3 p,float clearance) const;
    void updateAnchors();
    void selfContact();
    void updateStretch();
};
void layerOutfit(Simulation& top, Simulation& bottom);
}
