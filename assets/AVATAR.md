# Fitting models

The fitting figures use the MakeHuman Community's HM08 body mesh and its adult female and male morphs. Both have a featureless head, smooth hands and feet, and a single matte gray material.

MakeHuman released the base mesh and morph targets under CC0. The credited artists for the base mesh include the MakeHuman Team, Data Collection AB, Joel Palmius and Jonas Hauquier. The fitting code is part of myclos.et.

- [Asset license and source-code license](https://www.makehumancommunity.org/about/license.html)
- [CC0 license included by the source project](https://github.com/makehumancommunity/makehuman/blob/a8bc2d54ff0ac92e78ff71431b1023eda42bf482/LICENSE.ASSETS.md)
- [Pinned base mesh and source assets](https://github.com/makehumancommunity/makehuman/tree/a8bc2d54ff0ac92e78ff71431b1023eda42bf482/makehuman/data)

Both figures stand in a neutral, symmetrical fitting pose. The shoulders and hips are level, the elbows flex slightly, and the soles rest alongside one another. The head, palms and feet have smooth closed surfaces. The two forms share the same 19,940 triangles. The renderer and the cloth collision field use these same triangles after each measurement edit.

The reference for the limb proportions was Alvanon’s US ASTM Men size 39 and Missy Straight size 8. Their crotch-to-floor measurements are 81.25 cm at 177.75 cm stature and 77.50 cm at 166.25 cm respectively. These ratios set the height of the crotch. Head, upper arm and thigh circumferences guided the shaping of the two meshes. The app keeps its own Classic, Compact and Tall body measurements. [Alvanon professional fit forms](https://www.alvanon.com/products)

At 170 cm, the base shapes are calibrated to a 92 cm chest, 74 cm waist and 98 cm seat. A test takes horizontal tape measurements around the visible triangles and checks the shoulder landmarks, stature, crotch height, ground contact and bilateral symmetry. It covers all six presets and four configurations with independent changes to chest and shoulders. [Measurement test](../tests/form_measurements.cpp) · [Measured forms](../evidence/form-measurements.csv)

Classic, Compact and Tall have separate starting measurements for each model. The masculine Classic starts at 178 cm with a 99 cm chest, 82 cm waist, 98 cm hips and 44 cm shoulder span. Switching fitting models preserves manually edited measurements.
