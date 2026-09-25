#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 The DiamaneOS Project
"""Derive the FP6 side-sensor enrolment animations from AOSP's.

Settings shows fingerprint_edu_lottie_<variant>.json on the "touch the power
button" screen and picks the variant from the display rotation. It uses the
folded_* variants whenever smallestScreenWidthDp < 600 (ScreenSizeFoldProvider),
which is always the case on the FP6 (1116 px at 480 dpi = 372 dp), so both sets
are written; the other set only matters if that ever changes. AOSP's assets
draw a tablet with the sensor on its top edge. The FP6 is a phone with the
sensor in the power button on its right edge, 44% of the way down (the location
the fingerprint HAL reports). This script reshapes AOSP's drawing and writes the
four rotations; the choreography, colours and layer names (used by Settings'
light-theme colour mapping) are unchanged.

Usage: make_fingerprint_edu.py AOSP_PORTRAIT_BOTTOM_RIGHT_JSON OUTPUT_DIR
The input is packages/apps/Settings/res/raw/fingerprint_edu_lottie_portrait_bottom_right.json.
"""
import copy
import json
import sys
from pathlib import Path

# Base drawing (before the root rotation): the device lies horizontally with
# the sensor on its top edge; +90 degrees turns that edge into the right edge.
TABLET_W, TABLET_H = 326.0, 201.699
PHONE_W, PHONE_H = 326.0, 152.0          # FP6 body is about 2.14:1
CORNER = 16.0
SENSOR_FROM_TOP = 0.44                   # fingerprint HAL: y 1100 of 2484
STOCK_SENSOR_X = 100.25
SENSOR_X = -PHONE_W / 2 + SENSOR_FROM_TOP * PHONE_W
EDGE_SHIFT = (TABLET_H - PHONE_H) / 2    # top edge moves down by this much
SENSOR_SHIFT = (SENSOR_X - STOCK_SENSOR_X, EDGE_SHIFT)
ZOOM_SCALE = 0.59                        # AOSP shrinks the device to 59% at the zoom
ANCHOR = (-PHONE_W / 2, PHONE_H / 2)
START = (ANCHOR[0] + 0.75, ANCHOR[1])    # AOSP centres the device at x=+0.75
# Final device position: AOSP moves it by (30, 20); the phone moves 40 further
# along its length so the zoomed composition stays centred.
END = (START[0] + 30 + 40, START[1] + 20)
SENSOR_LOCAL = (SENSOR_X, -PHONE_H / 2 + 0.683)
SENSOR_END = (END[0] + ZOOM_SCALE * (SENSOR_LOCAL[0] - ANCHOR[0]),
              END[1] + ZOOM_SCALE * (SENSOR_LOCAL[1] - ANCHOR[1]))
SENSOR_START = (START[0] + SENSOR_LOCAL[0] - ANCHOR[0], START[1] + SENSOR_LOCAL[1] - ANCHOR[1])
# AOSP places the magnifier relative to the sensor and the icon above it.
CIRCLE_START = (SENSOR_START[0] - 0.7, SENSOR_START[1] + 12.3)
CIRCLE_END = (SENSOR_END[0] + 6.93, SENSOR_END[1] + 15.75)
ICON = (CIRCLE_END[0], CIRCLE_END[1] - 110.333)

# Settings file name -> root rotation, for display rotations 0, 90, 180 and
# 270 (FingerprintEnrollFindSensor.updateSfpsFindSensorAnimationAsset).
VARIANTS = {
    'fingerprint_edu_lottie_landscape_top_right.json': 90,
    'fingerprint_edu_lottie_portrait_top_left.json': 0,
    'fingerprint_edu_lottie_landscape_bottom_left.json': 270,
    'fingerprint_edu_lottie_portrait_bottom_right.json': 180,
    'fingerprint_edu_lottie_folded_top_right.json': 90,
    'fingerprint_edu_lottie_folded_top_left.json': 0,
    'fingerprint_edu_lottie_folded_bottom_left.json': 270,
    'fingerprint_edu_lottie_folded_bottom_right.json': 180,
}


def layers(data):
    return {layer['ind']: layer for layer in data['layers']}


def shift_position(layer, delta):
    prop = layer['ks']['p']
    if prop.get('a'):
        for key in prop['k']:
            for field in ('s', 'e'):
                if field in key:
                    key[field][0] += delta[0]
                    key[field][1] += delta[1]
    else:
        prop['k'][0] += delta[0]
        prop['k'][1] += delta[1]


def set_position(layer, keys):
    prop = layer['ks']['p']
    assert prop.get('a') and len(prop['k']) == len(keys)
    for key, value in zip(prop['k'], keys):
        key['s'][:2] = list(value)
        if 'e' in key:
            raise ValueError('unexpected legacy keyframe end value')


def rectangles(layer):
    def walk(items):
        for item in items:
            if item['ty'] == 'gr':
                yield from walk(item['it'])
            elif item['ty'] == 'rc':
                yield item
    return list(walk(layer.get('shapes', [])))


def phone_rect(layer, corner=None):
    for rect in rectangles(layer):
        size = rect['s']['k']
        assert not rect['s'].get('a') and [round(v, 3) for v in size] == [TABLET_W, TABLET_H], size
        rect['s']['k'] = [PHONE_W, PHONE_H]
        if corner is not None:
            rect['r']['k'] = corner


def reshape(data):
    by = layers(data)
    names = {i: layer.get('nm') for i, layer in by.items()}
    assert names[1] == 'Null 5' and names[3] == 'Null_Circle' and names[19] == '.grey800'
    assert names[2] == 'Fingerprint_Animation' and names[24] == '.blue400'
    # Device outline: phone proportions, same shrink, adjusted anchor and path.
    outline = by[19]
    phone_rect(outline, CORNER)
    outline['ks']['a']['k'][:2] = list(ANCHOR)
    set_position(outline, [START, END])
    # Frame masks that clip the sensor pulses to the device.
    for index in (23, 29, 31, 33, 35):
        assert names[index].startswith('device frame mask'), names[index]
        phone_rect(by[index], CORNER)
    # Sensor highlight (child of the outline) and the arrow and its matte (root).
    for index in (24, 21, 22):
        shift_position(by[index], SENSOR_SHIFT)
    # Magnifier path and the fingerprint icon beside it.
    set_position(by[3], [CIRCLE_START, CIRCLE_END])
    by[2]['ks']['p']['k'][:2] = list(ICON)
    return data


def rotate(data, degrees):
    out = copy.deepcopy(data)
    by = layers(out)
    by[1]['ks']['r']['k'] = degrees
    by[2]['ks']['r']['k'] = -degrees
    # The check mark in the magnifier spins in and must end upright.
    checks = [layer for layer in out['layers'] if layer.get('nm') == '.grey900'
              and layer.get('parent') == 3 and layer['ks']['r'].get('a')]
    assert len(checks) == 1
    keys = checks[0]['ks']['r']['k']
    # AOSP starts the spin at -180; with a 180-degree root that would not spin.
    keys[0]['s'] = [-360 if degrees == 180 else -180]
    keys[1]['s'] = [-degrees]
    out['nm'] = 'FP6 side sensor, root rotation %d' % degrees
    return out


def main():
    source, output = Path(sys.argv[1]), Path(sys.argv[2])
    data = json.loads(source.read_text())
    assert layers(data)[1]['ks']['r']['k'] == 90, 'expected AOSP portrait_bottom_right'
    base = reshape(data)
    output.mkdir(parents=True, exist_ok=True)
    for name, degrees in VARIANTS.items():
        (output / name).write_text(json.dumps(rotate(base, degrees), separators=(',', ':')) + '\n')


if __name__ == '__main__':
    main()
