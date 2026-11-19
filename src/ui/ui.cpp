#include "ui_internal.hpp"
#include <algorithm>
#include <filesystem>

namespace closet::ui_detail {
ImFont *sans, *bold, *serif, *italic;

namespace {
constexpr ImU32 shell=IM_COL32(249,246,242,255);
ImVec4 rgba(ImU32 color) { return ImGui::ColorConvertU32ToFloat4(color); }
ImU32 swatch(int index) {
    const auto c=items[index].color;
    return ImGui::GetColorU32(ImVec4(c.x,c.y,c.z,1));
}
} // namespace

void label(ImDrawList* dl,ImVec2 p,ImFont* f,float size,ImU32 color,const char* text) {
    dl->AddText(f,size,p,color,text);
}
float measure(ImFont* f,float size,const char* text) {
    return f->CalcTextSizeA(size,10000,0,text).x;
}
void colorDot(ImDrawList* dl,ImVec2 p,float r,int item) {
    dl->AddCircleFilled(p,r,swatch(item),24);
    dl->AddCircle(p,r,IM_COL32(54,40,47,85),24);
}
void focusOutline(ImDrawList* dl,ImVec2 a,ImVec2 b,float rounding) {
    if(ImGui::GetIO().NavVisible && ImGui::IsItemFocused())
        dl->AddRect({a.x+2,a.y+2},{b.x-2,b.y-2},IM_COL32(172,105,141,255),rounding,0,2);
}
bool choice(const char* text,bool selected,float width,float height) {
    ImGui::PushStyleColor(ImGuiCol_Button,rgba(selected?wine:shell));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,rgba(selected?wine:tint));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,rgba(wine));
    ImGui::PushStyleColor(ImGuiCol_Text,rgba(selected?white:ink));
    ImGui::PushStyleColor(ImGuiCol_Border,rgba(selected?wine:line));
    bool clicked=ImGui::Button(text,{width,height});
    if(ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    ImGui::PopStyleColor(5);
    return clicked;
}

// Icons share a 20-point grid and inherit the control's text color.
void icon(ImDrawList* dl,ImVec2 p,Icon glyph,ImU32 color,float scale) {
    auto pt=[&](float x,float y){return ImVec2(p.x+x*scale,p.y+y*scale);};
    auto stroke=[&](float x,float y,float xx,float yy){dl->AddLine(pt(x,y),pt(xx,yy),color,1.55f*scale);};
    switch(glyph) {
    case Icon::Hanger:
        dl->AddBezierCubic(pt(7.7,5.5),pt(7.6,1.3),pt(14.4,1.1),pt(12.2,5.5),color,1.55f*scale);
        dl->PathLineTo(pt(12.2,5.5)); dl->PathLineTo(pt(10,7.6)); dl->PathLineTo(pt(10,9));
        dl->PathLineTo(pt(18,14.2)); dl->PathLineTo(pt(18,16)); dl->PathLineTo(pt(2,16));
        dl->PathLineTo(pt(2,14.2)); dl->PathLineTo(pt(10,9)); dl->PathStroke(color,0,1.55f*scale); break;
    case Icon::Grid:
        for(int i=0;i<4;++i) {
            float x=3+(i%2)*8,y=3+(i/2)*8;
            dl->AddRect(pt(x,y),pt(x+5.5f,y+5.5f),color,1.2f*scale,0,1.4f*scale);
        }
        break;
    case Icon::Reset:
        dl->PathArcTo(pt(10.5,10),6.3f*scale,-2.55f,2.52f,20); dl->PathStroke(color,0,1.55f*scale);
        stroke(4,2.8f,4,7.9f); stroke(4,7.9f,9,7.9f); break;
    case Icon::Orbit:
        dl->AddEllipse(pt(10,10),{8*scale,3.7f*scale},color,-.38f,24,1.45f*scale);
        dl->AddCircleFilled(pt(10,10),1.8f*scale,color,16); break;
    case Icon::Arrow:
        stroke(3,10,16,10); stroke(11,5,16,10); stroke(11,15,16,10); break;
    case Icon::Plus: stroke(10,4,10,16); [[fallthrough]];
    case Icon::Minus: stroke(4,10,16,10); break;
    case Icon::Sliders:
        for(int i=0;i<2;++i) {
            const float y=6+8*i,x=7+6*i;
            stroke(2,y,x-2,y); stroke(x+2,y,18,y);
            dl->AddCircle(pt(x,y),2.4f*scale,color,16,1.4f*scale);
        }
        break;
    }
}
} // namespace closet::ui_detail

namespace closet {
using namespace ui_detail;
namespace {
constexpr float toolbar=46;
constexpr ImU32 masthead=IM_COL32(53,30,44,255);

bool navButton(const char* text,Icon glyph,float x,float width,bool selected) {
    ImVec2 p(x,12), end(x+width,52);
    ImGui::SetCursorScreenPos(p);
    bool clicked=ImGui::InvisibleButton(text,{width,40},ImGuiButtonFlags_EnableNav);
    bool hovered=ImGui::IsItemHovered();
    if(hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    auto* dl=ImGui::GetWindowDrawList();
    if(selected || hovered) dl->AddRectFilled(p,end,selected?pale:IM_COL32(82,54,71,255),7);
    ImU32 color=selected?wine:paper;
    float left=x+(width-measure(bold,17,text)-24)*.5f;
    icon(dl,{left,23},glyph,color,.9f);
    label(dl,{left+24,21},bold,17,color,text);
    focusOutline(dl,p,end,7);
    return clicked;
}

float panelWidth(float windowWidth) { return std::clamp(windowWidth*.265f,300.f,356.f); }
} // namespace

void initStyle() {
    auto& io=ImGui::GetIO();
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard; io.IniFilename=nullptr;
    io.FontGlobalScale=.5f;
    auto load=[&](const char* path,int face,float size) {
        ImFontConfig config; config.FontNo=face;
        if(std::filesystem::is_regular_file(path))
            if(ImFont* font=io.Fonts->AddFontFromFileTTF(path,2*size,&config)) return font;
        config.FontNo=0; config.SizePixels=2*size; return io.Fonts->AddFontDefault(&config);
    };
    sans=load("/System/Library/Fonts/Avenir Next.ttc",5,16);
    bold=load("/System/Library/Fonts/Avenir Next.ttc",2,17);
    serif=load("/System/Library/Fonts/Supplemental/Baskerville.ttc",0,40);
    italic=load("/System/Library/Fonts/Supplemental/Baskerville.ttc",2,40);
    io.FontDefault=sans;
    auto& s=ImGui::GetStyle();
    s.WindowPadding={0,0}; s.WindowBorderSize=0; s.ChildBorderSize=0; s.WindowRounding=0;
    s.FrameRounding=5; s.FrameBorderSize=1; s.FramePadding={7,4}; s.ItemSpacing={5,4};
    s.GrabMinSize=11; s.ScrollbarSize=7; s.ScrollbarRounding=4;
    s.PopupRounding=6;
    s.Colors[ImGuiCol_PopupBg]=rgba(paper); s.Colors[ImGuiCol_Border]=rgba(line);
    s.Colors[ImGuiCol_FrameBg]=rgba(shell); s.Colors[ImGuiCol_FrameBgHovered]=rgba(pale);
    s.Colors[ImGuiCol_Button]=rgba(pale); s.Colors[ImGuiCol_ButtonHovered]=rgba(tint);
    s.Colors[ImGuiCol_Header]=rgba(pale); s.Colors[ImGuiCol_HeaderHovered]=rgba(tint);
    s.Colors[ImGuiCol_HeaderActive]=rgba(tint);
    s.Colors[ImGuiCol_WindowBg]=rgba(shell); s.Colors[ImGuiCol_ChildBg]=rgba(paper);
    s.Colors[ImGuiCol_Text]=rgba(ink); s.Colors[ImGuiCol_TextLink]=rgba(wine);
    s.Colors[ImGuiCol_TableBorderLight]=rgba(line); s.Colors[ImGuiCol_TextDisabled]=rgba(muted);
    s.Colors[ImGuiCol_Separator]=rgba(line); s.Colors[ImGuiCol_NavCursor]=rgba(wine);
    s.Colors[ImGuiCol_ScrollbarBg]=rgba(paper); s.Colors[ImGuiCol_ScrollbarGrab]=rgba(line);
    s.Colors[ImGuiCol_ScrollbarGrabHovered]=rgba(muted); s.Colors[ImGuiCol_ScrollbarGrabActive]=rgba(wine);
}

std::array<int,2> sceneSize(float width,float height) {
    return {std::max(1,int(width-panelWidth(width)-2*inset)),std::max(1,int(height-header-inset-toolbar))};
}

void drawUi(Ui& ui,unsigned sceneTexture,const std::array<unsigned,items.size()>& thumbs) {
    ImVec2 display=ImGui::GetIO().DisplaySize;
    if(display.x<=0 || display.y<=0) return;
    ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(display);
    ImGui::Begin("myclos.et",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|
                  ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoSavedSettings|
                  ImGuiWindowFlags_NoBringToFrontOnFocus);
    auto* dl=ImGui::GetWindowDrawList();
    dl->AddRectFilled({0,0},{display.x,header},masthead);
    icon(dl,{24,20},Icon::Hanger,tint,1.3f);
    label(dl,{60,8},italic,38,paper,"myclos.et");
    float center=display.x*.5f;
    if(navButton("Try On",Icon::Hanger,center-103,100,!ui.store)) ui.store=false;
    if(navButton("Closet",Icon::Grid,center+1,100,ui.store)) ui.store=true;
    if(navButton("Reset",Icon::Reset,display.x-112,92,false)) { ui=Ui{}; ui.reset=true; }
    const float width=display.x-panelWidth(display.x),height=display.y-header;
    if(ui.store) store(ui,thumbs,width,height);
    else {
        auto scene=sceneSize(display.x,display.y);
        tryOn(ui,sceneTexture,scene[0],scene[1]);
    }
    inspector(ui,width,display.x-width,height);
    dl->AddLine({width,header},{width,display.y},line);
    ImGui::End();
}
} // namespace closet
