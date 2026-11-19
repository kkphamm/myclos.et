#include "ui_internal.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace closet::ui_detail {
namespace {
void zoomButton(Ui& ui,float x,float y,bool zoomIn) {
    const bool limit=zoomIn ? ui.zoom>=1.9f : ui.zoom<=.7f;
    ImGui::SetCursorScreenPos({x,y});
    ImGui::BeginDisabled(limit);
    if(ImGui::InvisibleButton(zoomIn?"Zoom in":"Zoom out",{31,30},ImGuiButtonFlags_EnableNav))
        ui.zoom=std::clamp(ui.zoom*(zoomIn?1.12f:1.f/1.12f),.7f,1.9f);
    auto* dl=ImGui::GetWindowDrawList();
    if(ImGui::IsItemHovered()) {
        dl->AddRectFilled({x+1,y+1},{x+30,y+29},tint,5);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    icon(dl,{x+7.5f,y+7},zoomIn?Icon::Plus:Icon::Minus,limit?line:wine,.8f);
    focusOutline(dl,{x,y},{x+31,y+30},5);
    ImGui::EndDisabled();
}

void stretchLegend(ImDrawList* dl,float x,float y) {
    dl->AddRectFilled({x,y},{x+161,y+65},paper,6);
    dl->AddRect({x,y},{x+161,y+65},line,6);
    label(dl,{x+11,y+6},bold,14,ink,"Relative stretch");
    x+=12; y+=30;
    const ImU32 low=IM_COL32(12,102,99,255),mid=IM_COL32(250,181,66,255),high=IM_COL32(201,41,41,255);
    dl->AddRectFilledMultiColor({x,y},{x+68,y+5},low,mid,mid,low);
    dl->AddRectFilledMultiColor({x+68,y},{x+136,y+5},mid,high,high,mid);
    label(dl,{x,y+7},sans,12,muted,"0%");
    label(dl,{x+58,y+7},sans,12,muted,"15%");
    label(dl,{x+108,y+7},sans,12,muted,"30%+");
}
} // namespace

void tryOn(Ui& ui,unsigned texture,float width,float height) {
    auto* dl=ImGui::GetWindowDrawList();
    ImVec2 top(inset,header+inset), end(top.x+width,top.y+height);
    if(texture) dl->AddImageRounded(texture,top,end,{0,1},{1,0},white,9);
    ImGui::SetCursorScreenPos(top);
    ImGui::InvisibleButton("Orbit garment",{width,height},ImGuiButtonFlags_EnableNav);
    const auto& io=ImGui::GetIO();
    if(ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ui.yaw+=io.MouseDelta.x*.009f; ui.pitch-=io.MouseDelta.y*.005f;
    }
    if(ImGui::IsItemHovered()) ui.zoom*=std::exp(io.MouseWheel*.08f);
    if(ImGui::IsItemFocused()) {
        if(ImGui::IsKeyDown(ImGuiKey_A)) ui.yaw-=io.DeltaTime;
        if(ImGui::IsKeyDown(ImGuiKey_D)) ui.yaw+=io.DeltaTime;
        if(ImGui::IsKeyDown(ImGuiKey_W)) ui.zoom*=std::exp(io.DeltaTime);
        if(ImGui::IsKeyDown(ImGuiKey_S)) ui.zoom*=std::exp(-io.DeltaTime);
    }
    ui.zoom=std::clamp(ui.zoom,.7f,1.9f); ui.pitch=std::clamp(ui.pitch,-.50f,.55f);
    focusOutline(dl,top,end,8);

    const float margin=std::clamp(width*.035f,19.f,36.f),x=top.x+margin;
    int shown=ui.item()>=0?ui.item():ui.worn[1-ui.slot];
    if(shown>=0) {
        const auto& item=items[shown];
        std::string name=item.name;
        const auto lastSpace=name.rfind(' ');
        const auto split=lastSpace==std::string::npos ? lastSpace : name.rfind(' ',lastSpace-1);
        std::string first=name.substr(0,split),last=split==std::string::npos ? "" : name.substr(split+1);
        float size=std::min(52.f,(width*.34f-margin)/std::max(measure(serif,1,first.c_str()),measure(italic,1,last.c_str())));
        float y=end.y-margin-64-1.93f*size;
        label(dl,{x,y},serif,size,ink,first.c_str());
        label(dl,{x,y+size*.93f},italic,size,ink,last.c_str());
        y+=size*1.93f+6;
        dl->AddLine({x,y},{x+27,y},wine,1.5f);
        char details[88];
        std::snprintf(details,sizeof(details),"%s  /  Size %s",item.mens?"Men":"Women",sizes[ui.garmentSizes[item.bottom()]]);
        label(dl,{x,y+10},bold,15,ink,details);
        colorDot(dl,{x+5.5f,y+43},5.5f,shown);
        label(dl,{x+18,y+34},sans,15,ink,item.colorName);
        if(ui.heatmap) stretchLegend(dl,top.x+margin,top.y+margin);
    } else {
        label(dl,{x,end.y-margin-52},serif,30,ink,"Choose a garment");
        label(dl,{x,end.y-margin-16},sans,15,muted,"Select a top or bottom in Fit settings.");
    }

    const float ty=end.y+9;
    icon(dl,{top.x+3,ty+6},Icon::Orbit,muted);
    label(dl,{top.x+29,ty+6},sans,14,muted,"Drag to orbit   ·   Scroll to zoom");
    const float zx=end.x-110;
    dl->AddRectFilled({zx,ty},{end.x,ty+30},paper,6);
    dl->AddRect({zx,ty},{end.x,ty+30},line,6);
    zoomButton(ui,zx,ty,false);
    zoomButton(ui,end.x-31,ty,true);
    char percent[12]; std::snprintf(percent,sizeof(percent),"%.0f%%",ui.zoom*100);
    label(dl,{zx+(110-measure(bold,13,percent))*.5f,ty+7},bold,13,ink,percent);
}
} // namespace closet::ui_detail
