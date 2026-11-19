#pragma once
#include "closet.hpp"
#include <algorithm>
#include <cmath>

namespace closet::cloth_math {
constexpr float pi = 3.14159265358979f;
constexpr float clearance=.0045f;
inline float blend(float a,float b,float t) { return a+(b-a)*t; }
inline float smoothBetween(float low,float high,float value) {
    const float t=std::clamp((value-low)/(high-low),0.f,1.f);
    return t*t*(3-2*t);
}
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float b) { return {a.x*b,a.y*b,a.z*b}; }
inline float dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline float length(Vec3 a) { return std::sqrt(dot(a,a)); }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
// Circumference of the exact polygon used to discretize a garment section.
inline float ellipsePerimeter(int samples,float depthRatio) {
    float perimeter=0;
    for(int i=0;i<samples;++i) {
        float a=2*pi*i/samples,b=2*pi*(i+1)/samples;
        perimeter+=std::hypot(std::cos(b)-std::cos(a),depthRatio*(std::sin(b)-std::sin(a)));
    }
    return perimeter;
}
}
