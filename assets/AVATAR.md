# Fitting models

The fitting figures use the MakeHuman Community's HM08 body and young-adult female and male macro targets. The warm matte display mesh has 7,115 vertices and 14,226 triangles with area-weighted vertex normals. The smooth head follows the forehead, nose bridge, and jaw. Rounded surfaces join the wrists to mitten hands and the insteps to continuous feet. A gentle smoothing pass softens the display surface. The fitted surface has 13,380 vertices and drives a separate 10,303-vertex cloth contact mesh.

MakeHuman released the base mesh and morph targets under CC0. The credited artists for the base mesh include the MakeHuman Team, Data Collection AB, Joel Palmius and Jonas Hauquier. The fitting code is part of myclos.et.

- [Asset license and source-code license](https://www.makehumancommunity.org/about/license.html)
- [CC0 license included by the source project](https://github.com/makehumancommunity/makehuman/blob/a8bc2d54ff0ac92e78ff71431b1023eda42bf482/LICENSE.ASSETS.md)
- [Pinned base mesh and source assets](https://github.com/makehumancommunity/makehuman/tree/a8bc2d54ff0ac92e78ff71431b1023eda42bf482/makehuman/data)

The default MakeHuman rig weights pose the arms approximately 15 degrees from the torso, with hands clear of the hips. The silhouette is calibrated to 7.5 heads tall. The body height is normalized after applying stature macro targets. The shoulder, chin and crotch are aligned at approximately 81.8%, 86.7% and 47% of stature.

The packed rig targets upper-arm and forearm lengths of 18.6% and 14.6% of stature. Hip, knee, and ankle landmarks are placed at 53.0%, 28.5%, and 3.9% of stature from the floor. Foot length is about 15% of stature. [Drillis and Contini model, Figure 1](https://cnb2025.tecnico.ulisboa.pt/prog_cnb2025_web/abstracts_cnb225/ID_387_794_Resumo%20CNB2025%20-%20Drillis%20%20Contini_Final.pdf)

Height, weight, muscle, and proportion targets blend from the adult base. The app measures the fitted mesh and corrects chest, waist, seat and shoulder width locally. Chest tape is measured at 72% of stature. Waist and seat tape select the narrowest and widest sections in their respective bands.

The measurement test checks a 0.5 cm tolerance for circumferences and shoulder width. It also checks stature, head proportion, crotch height, ground contact and bilateral symmetry across the six presets and four edited bodies. [Measurement test](../tests/form_measurements.cpp)

Classic, Compact and Tall have separate starting measurements for each model. The masculine Classic starts at 178 cm with a 99 cm chest, 82 cm waist, 98 cm hips and 44 cm shoulder span. Switching fitting models preserves manually edited measurements.

The packed assets are included with the app. To regenerate them from the pinned MakeHuman source and rig, run the importer from the repository root.

```sh
uv run --with 'numpy==2.5.5' --with 'fast-simplification==0.2.0' python tools/pack_hm08.py
```
