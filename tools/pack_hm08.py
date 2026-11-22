from pathlib import Path
import json
from urllib.request import urlretrieve

import numpy as np
from fast_simplification import simplify, replay_simplification

ROOT = Path('build/hm08')
COUNT = 13380
SOURCE = ('https://raw.githubusercontent.com/makehumancommunity/makehuman/'
          'a8bc2d54ff0ac92e78ff71431b1023eda42bf482/makehuman/data/')


def source(relative, local=None):
    file = ROOT / (local or relative)
    if not file.exists():
        file.parent.mkdir(parents=True, exist_ok=True)
        urlretrieve(SOURCE + relative, file)
    return file


points, faces = [], []
group = None
for line in source('3dobjs/base.obj', 'base.obj').read_text().splitlines():
    fields = line.split()
    if not fields:
        continue
    if fields[0] == 'v':
        points.append(tuple(map(float, fields[1:])))
    if fields[0] == 'g':
        group = fields[1]
    if fields[0] == 'f' and group == 'body':
        a, b, c, d = (int(v.split('/')[0]) - 1 for v in fields[1:])
        faces.extend(((a, b, c), (a, c, d)))
base = np.asarray(points)
faces = np.asarray(faces, dtype=np.int32)
joints = json.loads(source('rigs/default.mhskel').read_text())['joints']
weights = json.loads(source('rigs/default_weights.mhw').read_text())['weights']


def influence(prefixes):
    influence = np.zeros(len(base))
    for name, entries in weights.items():
        if name.startswith(prefixes):
            for index, weight in entries:
                influence[index] += weight
    return np.clip(influence, 0, 1)


lower = influence(('lowerarm', 'finger', 'metacarpal', 'wrist'))
arm = influence(('upperarm', 'lowerarm', 'finger', 'metacarpal', 'wrist'))
leg = influence(('upperleg', 'lowerleg', 'foot', 'toe'))


def target(name):
    offsets = np.zeros_like(base)
    for line in source(f'targets/macrodetails/{name}.target').read_text().splitlines():
        if line and not line.startswith('#'):
            index, *delta = line.split()
            offsets[int(index)] = [float(value) for value in delta]
    return offsets


def joint(points, name, suffix='L'):
    return points[joints[f'{name}.{suffix}____head']].mean(axis=0)


def pose(points):
    posed = points.copy()
    arm_angle = np.radians(15)
    for side, suffix in ((1, 'L'), (-1, 'R')):
        shoulder, elbow, wrist = (joint(points, name, suffix) for name in ('upperarm01', 'lowerarm01', 'wrist'))
        upper = elbow - shoulder
        angle = arm_angle - np.arctan2(side * upper[0], -upper[1])
        bend = wrist - elbow
        bend /= np.linalg.norm(bend)
        # Aim the forearm in the shoulder's local frame so the wrist clears the hip.
        forearm_angle = arm_angle - angle
        desired = np.array([side * np.sin(forearm_angle), -np.cos(forearm_angle), .10])
        desired /= np.linalg.norm(desired)
        axis = np.cross(bend, desired)
        rel = points - elbow
        rotated = rel + np.cross(axis, rel) + np.cross(axis, np.cross(axis, rel)) / (1 + bend @ desired)
        mask = (points[:, 0] * side > 0)
        posed += (rotated - rel) * (lower * mask)[:, None]
        c, s = np.cos(angle), np.sin(angle) * side
        x, y = (posed - shoulder)[:, :2].T
        delta = np.array([(c - 1) * x - s * y, s * x + (c - 1) * y, np.zeros(len(base))]).T
        posed += delta * (arm * mask)[:, None]

        hip, knee, ankle = (joint(points, name, suffix) for name in ('upperleg01', 'lowerleg01', 'foot'))
        legShift = np.interp(points[:, 1], [ankle[1], knee[1], hip[1]],
                             [1.08 - abs(ankle[0]), 1.18 - abs(knee[0]), 0])
        posed[:, 0] += side * legShift * leg * mask
    return posed


def fit_proportions(points, crotch_vertex):
    fitted = points.copy()
    sole = points[:COUNT, 1].min()
    height = np.ptp(points[:COUNT, 1])
    y = points[:, 1] - sole
    hip, knee, ankle = (joint(points, name)[1] - sole for name in ('upperleg01', 'lowerleg01', 'foot'))
    crotch = y[crotch_vertex]
    # Shoulder and crotch retain their fitting landmarks as the leg joints are aligned.
    leg_y = [0, ankle, knee, crotch, hip, .60*height]
    assert np.all(np.diff(leg_y) > 0)
    shift = np.interp(y, leg_y, [0, .039*height - ankle, .285*height - knee, 0, .530*height - hip, 0])
    fitted[:, 1] += shift * (1 - arm)

    for side, suffix in ((1, 'L'), (-1, 'R')):
        shoulder, elbow, wrist = (joint(points, name, suffix) for name in ('upperarm01', 'lowerarm01', 'wrist'))
        on_side = points[:, 0]*side > 0
        # Lengthen along each bone without scaling its cross-section.
        for start, end, fraction in ((shoulder, elbow, .186), (elbow, wrist, .146)):
            bone = end - start
            length = np.linalg.norm(bone)
            along = np.clip((points - start) @ bone / length**2, 0, 1)
            fitted += (along * arm * on_side)[:, None] * bone * (fraction*height / length - 1)

        foot = on_side[:COUNT] & (y[:COUNT] < .055*height)
        length = np.ptp(points[:COUNT][foot, 2])
        ankle_z = joint(points, 'foot', suffix)[2]
        fade = np.clip((.078*height - y) / (.023*height), 0, 1)
        fitted[:, 2] += (points[:, 2] - ankle_z) * (.148*height/length - 1) * leg * on_side * fade**2 * (3 - 2*fade)
    return fitted


def write(name, rows):
    Path(f'assets/{name}.inc').write_text(''.join('{' + ','.join(map(str, row)) + '},\n' for row in rows))


def display_topology():
    # These cuts select the same open rims on both HM08 fitting models.
    surface = base[:COUNT]
    x, y, z = surface.T
    cut = (y > 6.12) | ((y < -7) & (z > .85))
    for suffix in ('L', 'R'):
        wrist = joint(base, 'wrist', suffix)
        elbow = joint(base, 'lowerarm01', suffix)
        axis = wrist - elbow
        cut |= (x*wrist[0] > 0) & ((surface - wrist) @ (axis / np.linalg.norm(axis)) > .10) & (lower[:COUNT] > .55)
    kept = faces[~cut[faces].any(axis=1)]
    directed = kept[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2)
    _, index, count = np.unique(np.sort(directed, axis=1), axis=0, return_index=True, return_counts=True)
    next_on_rim = dict(directed[index[count == 1]])
    rims = []
    while next_on_rim:
        start = next(iter(next_on_rim))
        rim = [start]
        while (following := next_on_rim.pop(rim[-1])) != start:
            rim.append(following)
        rims.append(np.array(rim))
    assert len(rims) == 5
    return kept, rims


display_body, display_rims = display_topology()


def head_sections(rest, neck):
    crown = rest[rest[:, 1].argmax()]
    levels = np.linspace(neck, crown[1], 81)
    head_faces = faces[(rest[faces, 1] >= neck).any(axis=1)]
    first = rest[head_faces].reshape(-1, 3)
    second = rest[head_faces[:, [1, 2, 0]]].reshape(-1, 3)
    sections = []
    for height in levels[:-1]:
        hits = (first[:, 1] <= height) & (second[:, 1] > height)
        a, b = first[hits], second[hits]
        points = a + (b-a)*((height-a[:, 1]) / (b[:, 1]-a[:, 1]))[:, None]
        sections.append([min(.075, abs(points[:, 0]).max()), points[:, 2].min(), points[:, 2].max()])
    sections.append([0, crown[2], crown[2]])
    width, back, front = np.asarray(sections).T
    # Retain the skull and jaw profile with a continuous curve through the lip region.
    mouth = np.array([.874, .900]) * crown[1]
    region = (levels > mouth[0]) & (levels < mouth[1])
    front[region] = np.interp(levels[region], mouth, np.interp(mouth, levels, front))
    kernel = np.array([1, 8, 28, 56, 70, 56, 28, 8, 1]) / 256
    softened = [np.convolve(np.pad(column, 4, mode='edge'), kernel, mode='valid') for column in (width, back, front)]
    for profile, tip in zip(softened, sections[-1]):
        profile[-1] = tip
    width, back, front = softened
    bridge = np.array([.897, .933]) * crown[1]
    region = (levels > bridge[0]) & (levels < bridge[1])
    face = front.copy()
    face[region] = np.interp(levels[region], bridge, np.interp(bridge, levels, front))
    return levels, (width, back, face), .55*(front-face)


def display_caps(rest):
    points, triangles = list(rest), list(display_body)
    anchors = [(i, i, 0) for i in range(COUNT)]
    for rim in display_rims:
        base_y = base[rim, 1].mean()
        head, foot = base_y > 6, base_y < -7
        axes = [0, 1, 2] if foot else [0, 2, 1]
        world_axes = np.argsort(axes)
        edge = rest[rim][:, axes]
        center = (edge.max(axis=0) + edge.min(axis=0)) / 2
        radii = (edge.max(axis=0) - edge.min(axis=0))[:2] / 2
        direction = (edge[:, :2] - center[:2]) / radii
        direction /= np.linalg.norm(direction, axis=1)[:, None]
        opposite = rim[np.argmin(direction @ direction.T, axis=1)]
        side = np.sign(center[0])
        if head:
            levels, contours, bridge = head_sections(rest, center[2])
        toe = rest[(rest[:, 0]*side > 0) & (rest[:, 1] < .075), 2].max() if foot else 0

        def profile(t):
            if head:
                y = levels[0] + (levels[-1] - levels[0])*t
                width, back, front = (np.interp(y, levels, profile) for profile in contours)
                return np.array([0, (front+back)/2, y]), np.array([width, (front-back)/2])
            if foot:
                scale = np.sqrt(max(0, 1 - (max(0, t - .21) / .79)**2))
                depth = radii[1]*(1 - .56*t)
                return (np.array([center[0] - side*.012*t, depth + .001, center[2] + (toe - center[2])*t]),
                        np.array([radii[0]*1.02, depth]) * scale)
            scale = np.sqrt(max(0, 1 - ((t - .29) / .71)**2))
            return (center + np.array([side*.011*t, .015*t, -.166*t]), radii * [1.20, 1.35] * scale)

        def pack(point, anchor, partner, mix):
            index = len(points)
            points.append(point[world_axes])
            anchors.append((anchor, partner, mix))
            return index

        previous = rim
        for step in range(1, 19):
            t = (1 - np.cos(np.pi * step / 19)) / 2
            midpoint, width = profile(t)
            contour = np.broadcast_to(midpoint, edge.shape).copy()
            contour[:, :2] += direction * width
            if head:
                contour[:, 1] += np.interp(midpoint[2], levels, bridge) * np.maximum(0, direction[:, 1])**12
            elif not foot:
                thumb = .010 * np.exp(-((t - .39) / .17)**2) * ((1 - side*direction[:, 0]) / 2)**6
                contour[:, 0] -= side*thumb
            blend = min(1, t / .23)
            blend = blend*blend*(3 - 2*blend)
            moved = edge + midpoint - center
            vertices = moved + (contour - moved) * blend
            if foot:
                vertices[:, 1] = np.maximum(0, vertices[:, 1])
            ring = [pack(p, a, b, round(127 * blend)) for p, a, b in zip(vertices, rim, opposite)]
            for i, j in zip(range(len(rim)), np.roll(np.arange(len(rim)), -1)):
                triangles.extend(((previous[j], previous[i], ring[i]), (previous[j], ring[i], ring[j])))
            previous = ring
        tip = pack(profile(1)[0], rim[0], opposite[0], 127)
        triangles.extend((previous[j], previous[i], tip) for i, j in zip(range(len(rim)), np.roll(np.arange(len(rim)), -1)))

    triangles = np.asarray(triangles)
    used = np.unique(triangles)
    assert len(points) == triangles.max() + 1
    points = np.asarray(points)[used]
    anchors = np.asarray(anchors)[used]
    triangles = np.searchsorted(used, triangles)
    directed = triangles[:, [0, 1, 1, 2, 2, 0]].reshape(-1, 2)
    _, count = np.unique(np.sort(directed, axis=1), axis=0, return_counts=True)
    assert np.all(count == 2)

    # Fair only the joins so the carved silhouettes meet the original skin smoothly.
    first, second = directed.T
    degree = np.bincount(first)
    steps = np.full(len(points), 5)
    steps[np.searchsorted(used, np.concatenate(display_rims))] = 0
    for _ in range(4):
        np.minimum.at(steps, first, steps[second] + 1)
    amount = .50 * np.maximum(0, 1 - steps / 4)

    def relax(strength):
        average = np.stack([np.bincount(first, points[second, axis])/degree for axis in range(3)], axis=1)
        points[:] += strength * (average - points)

    for _ in range(16):
        relax(amount[:, None])

    # Alternating passes soften the display surface while conserving its outline.
    joined = points.copy()
    for _ in range(6):
        relax(.45)
        relax(-.47)
    change = points - joined
    limit = .003 / np.maximum(.003, np.linalg.norm(change, axis=1))
    points = joined + change*limit[:, None]

    a, b, mix = anchors.T
    reference = rest[a] + (rest[b] - rest[a]) * (mix / 255)[:, None]
    delta = np.rint((points - reference) * 10000).astype(int)
    assert np.all(np.abs(delta) < 32768)
    return np.column_stack((anchors, delta)), triangles


for sex in ('female', 'male'):
    adult = base + sum(target(f'{ancestry}-{sex}-young') for ancestry in ('african', 'asian', 'caucasian')) / 3
    neutral = target(f'universal-{sex}-young-averagemuscle-averageweight')
    rest = pose(adult + neutral)
    scale = 1.70 / np.ptp(rest[:COUNT, 1])
    origin = np.array([0, rest[:COUNT, 1].min(), .40])
    rest = (rest - origin) * scale
    # Put slightly flexed knees and the lower jaw on the fitting landmarks.
    landmarks = [0, 0.285, 0.47, 0.818, 1 - 1 / 7.5, 1]
    knee = joint(adult + neutral, 'lowerleg01')[1]
    shoulder_index = 8275
    mids = np.where((abs(rest[:COUNT, 0]) < .0001) & (rest[:COUNT, 1] > .65))[0]
    crotch_vertex = mids[np.argmin(rest[mids, 1])]
    source_y = [0, (knee - origin[1]) * scale / 1.70, rest[crotch_vertex, 1] / 1.70,
                rest[shoulder_index, 1] / 1.70, rest[5211, 1] / 1.70, 1]
    height_offset = np.interp(rest[:, 1] / 1.70, source_y, landmarks) * 1.70 - rest[:, 1]
    rest[:, 1] += height_offset
    rest = fit_proportions(rest, crotch_vertex)
    surface = rest[:COUNT]
    lengths = [np.linalg.norm(joint(rest, a) - joint(rest, b))/1.70 for a, b in
               (('upperarm01', 'lowerarm01'), ('lowerarm01', 'wrist'), ('upperleg01', 'lowerleg01'), ('lowerleg01', 'foot'))]
    assert np.max(abs(np.array(lengths) - [.186, .146, .245, .246])) < .006
    print(f'{sex} arm and leg segments as percent of height: ' + ', '.join(f'{100*n:.1f}' for n in lengths))
    samples = np.rint(surface * 10000).astype(int)
    quant_weights = np.rint(np.stack([arm[:COUNT], leg[:COUNT]], axis=1) * 255).astype(int)
    stem = f'{sex}-hm08'
    write(f'{sex}-vertices', np.concatenate([samples, quant_weights], axis=1))
    cap_offsets, cap_faces = display_caps(surface)
    write(f'{sex}-display-offsets', cap_offsets)
    configurations = [
        (f'height/{sex}-young-averagemuscle-averageweight-{part}height', False) for part in ('min', 'max')
    ] + [
        (f'universal-{sex}-young-averagemuscle-{part}weight', True) for part in ('min', 'max')
    ] + [
        (f'universal-{sex}-young-{part}muscle-averageweight', True) for part in ('min', 'max')
    ] + [
        (f'proportions/{sex}-young-averagemuscle-averageweight-{part}proportions', False)
        for part in ('uncommon', 'ideal')
    ]
    offsets = []
    for name, universal in configurations:
        delta = target(name)
        morphed = (pose(adult + delta + (0 if universal else neutral)) - origin) * scale
        morphed[:, 1] += height_offset
        morphed = fit_proportions(morphed, crotch_vertex)
        offsets.append(np.rint((morphed[:COUNT] - surface) * 10000).astype(int))
    with Path(f'assets/{stem}-macros.inc').open('w') as out:
        for entry in zip(*offsets):
            out.write('{' + ','.join('{' + ','.join(map(str, point)) + '}' for point in entry) + '},\n')
    if sex == 'female':
        write('hm08-display-triangles', cap_faces)
        _, _, history = simplify(surface, faces, target_reduction=.23, return_collapses=True)
        proxy, triangles, mapping = replay_simplification(surface.astype(np.float32), faces, history)
        assert len(mapping) == COUNT and len(mapping) > len(proxy) and min(mapping) >= 0
        write('body-triangles', faces)
        write('hm08-proxy-triangles', triangles)
        Path('assets/hm08-proxy-map.inc').write_text(''.join(f'{i},\n' for i in mapping))
        print(f'{len(cap_offsets)} display vertices, {len(surface)} fitted vertices, {len(proxy)} proxy vertices')
