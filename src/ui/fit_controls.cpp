#include "ui_internal.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace closet::ui_detail {
namespace {
const char* presetNames[]={"Classic","Compact","Tall"};

void gap(float h) { ImGui::Dummy({1,h}); }

void section(const char* name) {
    ImGui::PushFont(bold);
    ImGui::TextUnformatted(name);
    ImGui::PopFont();
}
void rule() { gap(5); ImGui::Separator(); gap(6); }

void slider(const char* name,float& value,float low,float high,const char* format,bool integral=true) {
    ImGui::PushID(name);
    auto* dl=ImGui::GetWindowDrawList();
    const ImVec2 p=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x;
    char numeric[32]; std::snprintf(numeric,sizeof(numeric),format,value);
    label(dl,p,sans,16,ink,name);
    label(dl,{p.x+width-measure(bold,15,numeric),p.y},bold,15,wine,numeric);
    ImGui::Dummy({1,13});
    ImGui::SetNextItemWidth(width);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    for(auto c:{ImGuiCol_FrameBg,ImGuiCol_FrameBgHovered,ImGuiCol_FrameBgActive,
                ImGuiCol_SliderGrab,ImGuiCol_SliderGrabActive,ImGuiCol_Text})
        ImGui::PushStyleColor(c,{0,0,0,0});
    ImGui::SliderFloat("##value",&value,low,high,integral?"%.0f":"%.2f",
                       ImGuiSliderFlags_AlwaysClamp|ImGuiSliderFlags_NoInput);
    ImGui::PopStyleColor(6); ImGui::PopStyleVar(2);
    const ImVec2 a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
    const float y=std::round((a.y+b.y)*.5f),pad=2+ImGui::GetStyle().GrabMinSize*.5f;
    const float x=a.x+pad+(width-2*pad)*std::clamp((value-low)/(high-low),0.f,1.f);
    dl->AddLine({a.x+pad,y},{b.x-pad,y},line,2);
    dl->AddLine({a.x+pad,y},{x,y},wine,2);
    dl->AddCircleFilled({x,y},ImGui::IsItemActive()?6:5.1f,wine,24);
    focusOutline(dl,a,b,4);
    ImGui::PopID();
}

void toggle(const char* name,bool& state) {
    ImVec2 p=ImGui::GetCursorScreenPos();
    float w=ImGui::GetContentRegionAvail().x;
    if(ImGui::InvisibleButton(name,{w,25},ImGuiButtonFlags_EnableNav)) state=!state;
    if(ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    auto* dl=ImGui::GetWindowDrawList();
    label(dl,{p.x,p.y+3},sans,16,ink,name);
    const float r=p.x+w,y=p.y+12;
    dl->AddRectFilled({r-32,y-9},{r,y+9},state?wine:IM_COL32(132,123,128,255),9);
    dl->AddCircleFilled({state?r-9:r-23,y},6,white,20);
    focusOutline(dl,p,{r,p.y+25},4);
}

void outfitRow(Ui& ui,int slot) {
    bool bottom=slot==1;
    const char* part=bottom?"Bottom":"Top";
    ImGui::PushID(part);
    float left=ImGui::GetCursorPosX(),width=ImGui::GetContentRegionAvail().x;
    if(choice(part,ui.slot==slot,64,25)) ui.slot=slot;
    ImGui::SetItemTooltip("Show %s measurements",bottom?"bottom":"top");
    ImGui::SameLine();
    ImGui::SetCursorPosX(left+width-132);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Size");
    for(int i=0;i<3;++i) {
        ImGui::SameLine(0,4);
        if(choice(sizes[i],ui.garmentSizes[slot]==i,29,25)) {
            ui.slot=slot; ui.garmentSizes[slot]=i;
        }
    }
    ImGui::SetNextItemWidth(-1);
    const int worn=ui.worn[slot];
    const char* preview=worn<0?(bottom?"Choose a bottom":"Choose a top"):items[worn].name;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{7,7});
    if(ImGui::BeginCombo("##garment",preview)) {
        if(ImGui::Selectable("None",worn<0)) ui.remove(slot);
        if(worn<0) ImGui::SetItemDefaultFocus();
        for(size_t i=0;i<items.size();++i) {
            if(items[i].bottom()!=bottom) continue;
            std::string option=std::string(items[i].name)+(items[i].mens?"  ·  Men":"  ·  Women");
            if(ImGui::Selectable(option.c_str(),worn==int(i))) {
                ui.wear(int(i)); ui.store=false;
            }
            if(worn==int(i)) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    } else if(ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetItemTooltip("%s",preview);
    }
    ImGui::PopStyleVar();
    ImGui::PopID();
}

void sizeChart(Ui& ui) {
    const auto& item=items[ui.item()];
    gap(2);
    ImVec2 p=ImGui::GetCursorScreenPos();
    auto* dl=ImGui::GetWindowDrawList();
    const float width=ImGui::GetContentRegionAvail().x;
    label(dl,{p.x,p.y+4},sans,14,muted,ui.slot==0?"Finished top":"Finished bottom");
    ImGui::SetCursorScreenPos({p.x+width-65,p.y});
    if(choice("cm",!ui.inches,32,25)) ui.inches=false;
    ImGui::SameLine(0,2);
    if(choice("in",ui.inches,31,25)) ui.inches=true;
    ImGui::SetCursorScreenPos({p.x,p.y+29});
    float unit=ui.inches?2.54f:1.f;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,{6,3});
    if(ImGui::BeginTable("Garment dimensions",4,ImGuiTableFlags_BordersInnerH)){
        ImGui::TableSetupColumn("Measure",ImGuiTableColumnFlags_WidthFixed,91);
        for(auto size:sizes) ImGui::TableSetupColumn(size,ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::TextDisabled("%s",ui.inches?"Inches":"Centimeters");
        for(int i=0;i<3;++i) {
            ImGui::TableNextColumn();
            if(i==ui.size()) ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,pale);
            ImGui::TextUnformatted(sizes[i]);
        }
        auto row=[&](const char* name,float GarmentSize::*field){
            if(item.sizes[ui.size()].*field<=0) return;
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(name);
            for(int i=0;i<3;++i){
                ImGui::TableNextColumn();
                if(i==ui.size()) ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,pale);
                ImGui::Text(ui.inches?"%.2f":"%.1f",item.sizes[i].*field/unit);
            }
        };
        row(item.bottom()?"Length":"Back length",&GarmentSize::lengthCm);
        row("Chest, flat",&GarmentSize::chestFlatCm); row("Waist, around",&GarmentSize::waistCircCm);
        row("Hip, around",&GarmentSize::hipCircCm); row("Back slit",&GarmentSize::slitLengthCm);
        row("Shoulder span",&GarmentSize::shoulderCm); row("Sleeve¹",&GarmentSize::sleeveFromBackCm);
        row("Thigh, flat",&GarmentSize::thighFlatCm); row("Cuff, flat",&GarmentSize::hemFlatCm);
        row("Inseam",&GarmentSize::inseamCm);
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    if(item.tee()) ImGui::TextDisabled("¹ From center back to cuff");
    ImGui::TextLinkOpenURL("Size chart source",item.url);
}
} // namespace

void inspector(Ui& ui,float x,float width,float height) {
    ImGui::SetCursorScreenPos({x,header});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{22,16});
    ImGui::BeginChild("Fit controls",{width,height},ImGuiChildFlags_AlwaysUseWindowPadding);
    auto* dl=ImGui::GetWindowDrawList();
    ImVec2 p=ImGui::GetCursorScreenPos();
    label(dl,{p.x,p.y-3},serif,30,ink,"Fit settings");
    icon(dl,{p.x+ImGui::GetContentRegionAvail().x-21,p.y+4},Icon::Sliders,wine);
    ImGui::Dummy({1,31});
    rule();
    section("Your outfit");
    outfitRow(ui,0);
    gap(2);
    outfitRow(ui,1);
    if(!ui.store && ui.item()>=0) sizeChart(ui);
    rule();
    section("Fitting model");
    const float half=(ImGui::GetContentRegionAvail().x-5)/2;
    if(choice("Feminine",!ui.masculine,half)) ui.setModel(false);
    ImGui::SameLine(0,5);
    if(choice("Masculine",ui.masculine,half)) ui.setModel(true);
    gap(5);
    section("Body measurements");
    const float segmentWidth=(ImGui::GetContentRegionAvail().x-10)/3;
    for(int i=0;i<3;++i) {
        if(i) ImGui::SameLine(0,5);
        if(choice(presetNames[i],ui.body==ui.modelPresets()[i],segmentWidth)) ui.body=ui.modelPresets()[i];
    }
    gap(4);
    slider("Height",ui.body.height,150,190,"%.0f cm");
    slider("Chest",ui.body.chest,78,112,"%.0f cm");
    slider("Waist",ui.body.waist,62,100,"%.0f cm");
    slider("Hips",ui.body.hips,82,120,"%.0f cm");
    slider("Shoulder width",ui.body.shoulder,34,50,"%.0f cm");
    if(!ui.store) {
        rule();
        section("Simulation");
        toggle("Relative stretch",ui.heatmap);
        toggle("Wind",ui.wind);
        if(ui.wind) {
            slider("Strength",ui.strength,.1f,3,"%.1f",false);
            float degrees=ui.direction*57.2957795f;
            slider("Direction",degrees,-180,180,"%.0f deg");
            ui.direction=degrees/57.2957795f;
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}
} // namespace closet::ui_detail
