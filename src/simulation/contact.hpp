#pragma once
#include <array>
#include <memory>
#include <vector>

namespace closet {
struct Mesh;
struct Vec3;
class BodyContact {
public:
    explicit BodyContact(const Mesh& body,const std::vector<std::array<float,2>>* limbWeights=nullptr,
                         const Mesh* fittedSurface=nullptr,float naturalWaist=0);
    float waistHeight() const;
    Vec3 dress(Vec3 point, float clearance, float armGap=0,bool snug=false) const;
    float panelDepth(Vec3 point,float direction,float clearance) const;
    Vec3 limbCenter(float y,int side,bool arm,Vec3 fallback) const;
    float armGirth(float y,int side) const;
    Vec3 project(Vec3 point, float clearance) const;
    float floorHeight(Vec3 point) const;
    float shoulderHeight(float x, float z, float upperY, float fallbackY) const;
    float minVertexDistance(const Mesh& cloth) const;
private:
    struct Data;
    std::shared_ptr<const Data> data;
};
}
