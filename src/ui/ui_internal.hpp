#pragma once
#include "ui.hpp"
#include "imgui.h"

namespace closet::ui_detail {
constexpr float header=64, inset=18;
constexpr ImU32 ink=IM_COL32(42,32,40,255), muted=IM_COL32(101,87,95,255);
constexpr ImU32 wine=IM_COL32(83,43,64,255);
constexpr ImU32 pale=IM_COL32(244,235,239,255), tint=IM_COL32(232,215,224,255);
constexpr ImU32 line=IM_COL32(218,208,209,255);
constexpr ImU32 paper=IM_COL32(255,253,250,255), white=IM_COL32_WHITE;
extern ImFont *sans, *bold, *serif, *italic;
inline constexpr const char* sizes[]={"S","M","L"};

enum class Icon { Hanger, Grid, Reset, Orbit, Arrow, Minus, Plus, Sliders };
void icon(ImDrawList* dl,ImVec2 p,Icon glyph,ImU32 color,float scale=1);
void label(ImDrawList* dl,ImVec2 p,ImFont* f,float size,ImU32 color,const char* text);
float measure(ImFont* f,float size,const char* text);
void colorDot(ImDrawList* dl,ImVec2 p,float r,int item);
void focusOutline(ImDrawList* dl,ImVec2 a,ImVec2 b,float rounding);
bool choice(const char* text,bool selected,float width,float height=31);

void inspector(Ui& ui,float x,float width,float height);
void tryOn(Ui& ui,unsigned texture,float width,float height);
void store(Ui& ui,const std::array<unsigned,items.size()>& thumbs,float width,float height);
}
