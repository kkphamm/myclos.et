#pragma once
#include "closet.hpp"

namespace closet {

enum class Cut { Tank, Tee, OversizedTee, Skirt, Trousers, WideTrousers, Joggers };

struct GarmentSize {
    float lengthCm, chestFlatCm, waistCircCm, hipCircCm, slitLengthCm;
    float shoulderCm{}, sleeveFromBackCm{}, inseamCm{}, thighFlatCm{}, hemFlatCm{};
};

struct Item {
    const char* name;
    const char* colorName;
    Cut cut;
    bool flaredHem;
    Vec3 color;
    const char* productId;
    const char* url;
    std::array<GarmentSize,3> sizes; // S, M, L
    bool mens=false;
    constexpr bool tee() const { return cut==Cut::Tee || cut==Cut::OversizedTee; }
    constexpr bool pants() const { return cut==Cut::Trousers || cut==Cut::WideTrousers || cut==Cut::Joggers; }
    constexpr float supportFraction() const {
        if(cut==Cut::WideTrousers || cut==Cut::Skirt) return .610f;
        if(cut==Cut::Trousers) return .595f;
        return cut==Cut::Joggers?.586f:.83f;
    }
    constexpr bool bottom() const { return cut==Cut::Skirt || pants(); }
    constexpr bool elasticWaist() const { return cut==Cut::Skirt || cut==Cut::Joggers; }
};

// US finished-product charts are in inches. Zero indicates an unpublished dimension.
constexpr GarmentSize chartInches(float length, float chest, float waist=0, float hips=0, float slit=0) {
    return {2.54f*length,2.54f*chest,2.54f*waist,2.54f*hips,2.54f*slit};
}

constexpr GarmentSize teeInches(float length,float shoulder,float chest,float sleeveFromBack) {
    auto size=chartInches(length,chest);
    size.shoulderCm=shoulder*2.54f;
    size.sleeveFromBackCm=sleeveFromBack*2.54f;
    return size;
}
constexpr GarmentSize pantsInches(float waist,float hips,float thigh,float hem,float inseam) {
    auto size=chartInches(0,0,waist,hips);
    size.thighFlatCm=thigh*2.54f;
    size.hemFlatCm=hem*2.54f;
    size.inseamCm=inseam*2.54f;
    return size;
}

inline constexpr std::array<Item,11> items = {{
    {"Random Ribbed Tank Top", "Off White", Cut::Tank, false, {237/255.f,229/255.f,217/255.f},
     "427391", "https://www.uniqlo.com/us/en/products/E427391-000/00?colorDisplayCode=01&sizeDisplayCode=003",
     {chartInches(22.75f,14.25f), chartInches(23.5f,15.5f), chartInches(24.25f,16.75f)}},
    {"AIRism Seamless Tank Top", "Blue", Cut::Tank, false, {81/255.f,87/255.f,110/255.f},
     "433735", "https://www.uniqlo.com/us/en/products/E433735-000/00?colorDisplayCode=68",
     {chartInches(28.5f,17.25f), chartInches(29.25f,18.5f), chartInches(30,19.75f)}},
    {"Ultra Stretch Narrow Skirt", "Black", Cut::Skirt, false, {38/255.f,38/255.f,40/255.f},
     "474947", "https://www.uniqlo.com/us/en/products/E474947-000/00?colorDisplayCode=09&sizeDisplayCode=003",
     {chartInches(33.5f,0,28.5f,37.5f,13.5f), chartInches(33.5f,0,30,39,13.5f), chartInches(34.25f,0,32.5f,41.25f,13.5f)}},
    {"Mermaid Long Skirt", "Green", Cut::Skirt, true, {56/255.f,98/255.f,79/255.f},
     "445974", "https://www.uniqlo.com/us/en/products/E445974-000/00",
     {chartInches(33.75f,0,26,44), chartInches(33.75f,0,28.25f,46.25f), chartInches(34.75f,0,30.75f,48.5f)}},
    {"AIRism Cotton Tank Top", "Black", Cut::Tank, false, {46/255.f,42/255.f,46/255.f},
     "434168", "https://www.uniqlo.com/us/en/products/E434168-000/00?cgid=men-airism-airism-cotton&dwvar_434168_color=COL09",
     {chartInches(26,17.75f), chartInches(27.25f,19), chartInches(28.25f,20.5f)}, true},
    {"DRY-EX Tank Top", "Yellow", Cut::Tank, false, {213/255.f,228/255.f,85/255.f},
     "437410", "https://www.uniqlo.com/us/en/products/E437410-000/00?bx_replen=true",
     {chartInches(26.75f,19.25f), chartInches(28,20.5f), chartInches(29.25f,22)}, true},
    {"DRY-EX Crew Neck T-Shirt", "Purple", Cut::Tee, false, {128/255.f,66/255.f,110/255.f},
     "433399", "https://www.uniqlo.com/us/en/products/E433399-000/00?colorDisplayCode=76",
     {teeInches(26.75f,17,19.25f,17.25f), teeInches(28,17.5f,20.5f,18), teeInches(29.25f,18.25f,22,18.75f)}, true},
    {"U AIRism Oversized T-Shirt", "Wine", Cut::OversizedTee, false, {106/255.f,52/255.f,65/255.f},
     "441601", "https://www.uniqlo.com/us/en/products/E441601-000/00?cgid=men-essentials&dwvar_441601_color=COL19",
     {teeInches(27,20.75f,21.75f,20.75f), teeInches(28.25f,21.25f,23,21.75f), teeInches(29.25f,22,24.5f,22.5f)}, true},
    {"Smart Ankle Pants", "Beige", Cut::Trousers, false, {171/255.f,155/255.f,147/255.f},
     "452112", "https://www.uniqlo.com/us/en/products/E452112-000/00?colorDisplayCode=32&sizeDisplayCode=004",
     {pantsInches(30.75f,38.5f,12.75f,7,27.25f), pantsInches(33,41,13.5f,7.25f,28.25f), pantsInches(35.5f,43.25f,14.25f,7.75f,29)}, true},
    {"Wide Tapered Pants", "Pink", Cut::WideTrousers, false, {219/255.f,183/255.f,171/255.f},
     "471934", "https://www.uniqlo.com/us/en/products/E471934-000/00?colorDisplayCode=12&sizeDisplayCode=004",
     {pantsInches(30.25f,42.75f,14.25f,7,26), pantsInches(32.75f,45,14.75f,7,26.75f), pantsInches(35,47.5f,15.5f,7.5f,27.5f)}, true},
    {"Cargo Jogger Pants", "Black", Cut::Joggers, false, {46/255.f,45/255.f,51/255.f},
     "462318", "https://www.uniqlo.com/us/en/products/E462318-000/00?colorDisplayCode=09&sizeDisplayCode=006",
     {pantsInches(30,39.75f,13.25f,5,27.75f), pantsInches(32.25f,42.25f,14,5.5f,28.5f), pantsInches(34.75f,44.5f,14.5f,5.75f,29.25f)}, true}
}};
}
