#!/usr/bin/env python3
"""
Smart Menubar Patcher & Generator for Zuma Deluxe
Reconstructs menubar.jpg and _menubar.gif for ANY arbitrary resolution (>= 480x320)
using authentic reference tiles extracted from Zuma800x600 reference stored in assets/media.

Constraints:
- Global Minimum Window Resolution: 480x320.
- Maximum Resolution: Unlimited (800x600, 1024x768, 1280x720, 1920x1080, etc.).
- STRICT: The 5 top button/compartment areas (Lives, Stage/Level, Score, Zuma Bar, Menu)
  MUST NEVER BE SQUISHED HORIZONTALLY (1.0x native unscaled size, ZERO DISTORTION).
- At 480px: Bridges removed/minimized with smoothstep alpha-blending across seams.
- Provides calc_hud_coords(w) for synchronizing with zumareshandler.dll.
"""

import os
import sys
import argparse
import numpy as np
from PIL import Image

MIN_W = 480
MIN_H = 320

def tile_horizontal(tile, length):
    if length <= 0:
        shape = list(tile.shape)
        shape[1] = 0
        return np.zeros(shape, dtype=tile.dtype)
    repeats = (length // tile.shape[1]) + 2
    stacked = np.tile(tile, (1, repeats, 1) if tile.ndim == 3 else (1, repeats))
    return stacked[:, :length]

def tile_vertical(tile, length):
    if length <= 0:
        shape = list(tile.shape)
        shape[0] = 0
        return np.zeros(shape, dtype=tile.dtype)
    repeats = (length // tile.shape[0]) + 2
    stacked = np.tile(tile, (repeats, 1, 1) if tile.ndim == 3 else (repeats, 1))
    return stacked[:length]

def blend_pair(im1, im2, blend_w=7):
    """
    Smoothly alpha-blends the right edge of im1 with the left edge of im2
    using a smoothstep ramp (3*t^2 - 2*t^3) over blend_w pixels.
    """
    if blend_w <= 0:
        return np.hstack([im1, im2])
    blend_w = min(blend_w, im1.shape[1], im2.shape[1])
    h = im1.shape[0]
    t = np.linspace(0, 1, blend_w)
    weight = 3 * t**2 - 2 * t**3
    w_ramp = weight[None, :, None] if im1.ndim == 3 else weight[None, :]

    left = im1[:, :-blend_w]
    right = im2[:, blend_w:]

    over_l = im1[:, -blend_w:].astype(float)
    over_r = im2[:, :blend_w].astype(float)
    blended = ((1.0 - w_ramp) * over_l + w_ramp * over_r).astype(np.uint8)

    return np.hstack([left, blended, right])

def ensure_master_assets(assets_dir, ref_800_path=None):
    required_files = [
        'menubar_bridge_a.png', '_menubar_bridge_a.png',
        'menubar_bridge_b.png', '_menubar_bridge_b.png',
        'menubar_bridge_c.png', '_menubar_bridge_c.png',
        'menubar_bridge_d.png', '_menubar_bridge_d.png',
        'menubar_tile_swirl_15px.png', '_menubar_tile_swirl_15px.png',
        'menubar_tile_swirl_26px_b.png', '_menubar_tile_swirl_26px_b.png',
        'menubar_tile_swirl_26px_c.png', '_menubar_tile_swirl_26px_c.png',
        'menubar_tile_pillar_l.png', '_menubar_tile_pillar_l.png',
        'menubar_tile_pillar_r.png', '_menubar_tile_pillar_r.png',
        'menubar_tile_bottom_l.png', '_menubar_tile_bottom_l.png',
        'menubar_tile_bottom_r.png', '_menubar_tile_bottom_r.png'
    ]

    all_exist = all(os.path.exists(os.path.join(assets_dir, f)) for f in required_files)
    if all_exist:
        return

    if ref_800_path and os.path.exists(ref_800_path):
        os.makedirs(assets_dir, exist_ok=True)
        ref_rgb = Image.open(ref_800_path).convert('RGB')
        ref_mask_path = os.path.join(os.path.dirname(ref_800_path), '_menubar.gif')
        if os.path.exists(ref_mask_path):
            ref_mask = Image.open(ref_mask_path).convert('L')
        else:
            ref_mask = Image.new('L', ref_rgb.size, 255)

        ref_rgb.crop((134, 0, 164, 42)).save(os.path.join(assets_dir, 'menubar_bridge_a.png'))
        ref_mask.crop((134, 0, 164, 42)).save(os.path.join(assets_dir, '_menubar_bridge_a.png'))
        ref_rgb.crop((273, 0, 350, 42)).save(os.path.join(assets_dir, 'menubar_bridge_b.png'))
        ref_mask.crop((273, 0, 350, 42)).save(os.path.join(assets_dir, '_menubar_bridge_b.png'))
        ref_rgb.crop((463, 0, 520, 42)).save(os.path.join(assets_dir, 'menubar_bridge_c.png'))
        ref_mask.crop((463, 0, 520, 42)).save(os.path.join(assets_dir, '_menubar_bridge_c.png'))
        ref_rgb.crop((633, 0, 678, 42)).save(os.path.join(assets_dir, 'menubar_bridge_d.png'))
        ref_mask.crop((633, 0, 678, 42)).save(os.path.join(assets_dir, '_menubar_bridge_d.png'))

        ref_rgb.crop((134, 0, 149, 42)).save(os.path.join(assets_dir, 'menubar_tile_swirl_15px.png'))
        ref_mask.crop((134, 0, 149, 42)).save(os.path.join(assets_dir, '_menubar_tile_swirl_15px.png'))
        ref_rgb.crop((299, 0, 325, 42)).save(os.path.join(assets_dir, 'menubar_tile_swirl_26px_b.png'))
        ref_mask.crop((299, 0, 325, 42)).save(os.path.join(assets_dir, '_menubar_tile_swirl_26px_b.png'))
        ref_rgb.crop((463, 0, 489, 42)).save(os.path.join(assets_dir, 'menubar_tile_swirl_26px_c.png'))
        ref_mask.crop((463, 0, 489, 42)).save(os.path.join(assets_dir, '_menubar_tile_swirl_26px_c.png'))

        ref_rgb.crop((0, 82, 24, 97)).save(os.path.join(assets_dir, 'menubar_tile_pillar_l.png'))
        ref_mask.crop((0, 82, 24, 97)).save(os.path.join(assets_dir, '_menubar_tile_pillar_l.png'))
        ref_rgb.crop((776, 82, 800, 97)).save(os.path.join(assets_dir, 'menubar_tile_pillar_r.png'))
        ref_mask.crop((776, 82, 800, 97)).save(os.path.join(assets_dir, '_menubar_tile_pillar_r.png'))

        ref_rgb.crop((156, 576, 171, 600)).save(os.path.join(assets_dir, 'menubar_tile_bottom_l.png'))
        ref_mask.crop((156, 576, 171, 600)).save(os.path.join(assets_dir, '_menubar_tile_bottom_l.png'))
        ref_rgb.crop((442, 576, 457, 600)).save(os.path.join(assets_dir, 'menubar_tile_bottom_r.png'))
        ref_mask.crop((442, 576, 457, 600)).save(os.path.join(assets_dir, '_menubar_tile_bottom_r.png'))

def calc_hud_coords(w):
    """
    Computes exact pixel HUD coordinates matching the menubar layout for any W >= 480.
    Synchronized 1:1 with zumareshandler.dll.c.
    """
    w = max(MIN_W, w)
    if w >= 540:
        extra_w = w - 800
        if extra_w >= 0:
            ba = int(round(extra_w * 0.15))
            bb = int(round(extra_w * 0.35))
            bc = int(round(extra_w * 0.35))
            bd = extra_w - ba - bb - bc
            ba += 30
            bb += 77
            bc += 57
            bd += 45
        else:
            s = -extra_w
            sha = int(round(s * (30 / 209)))
            shb = int(round(s * (77 / 209)))
            shc = int(round(s * (57 / 209)))
            shd = s - sha - shb - shc
            ba = max(0, 30 - sha)
            bb = max(0, 77 - shb)
            bc = max(0, 57 - shc)
            bd = max(0, 45 - shd)

        c1_w = 134
        c2_x = c1_w + ba
        c2_w = 109
        c3_x = c2_x + c2_w + bb
        c3_w = 113
        c4_x = c3_x + c3_w + bc
        c4_w = 113
        c5_x = c4_x + c4_w + bd

        lives_x = 28
        # Red bar visual oval center is at c2_x + 66 (vanilla: 134+66=200, 800x600: 164+66=230)
        stage_cx = c2_x + 66
        score_rx = c3_x + 90
        zuma_draw_x = c4_x + 19
        menu_x = c5_x + 22
    else:
        # 480 <= W < 540
        t = (w - 480) / 60.0
        lives_x = 26 if w <= 480 else 28
        stage_cx = int(round(153 + t * (200 - 153)))
        score_rx = int(round(278 + t * (333 - 278)))
        zuma_draw_x = int(round(312 + t * (375 - 312)))
        menu_x = int(round(381 + t * (491 - 381)))

    return {
        'W': w,
        'Lives_X': lives_x,
        'Stage_CX': stage_cx,
        'Score_RX': score_rx,
        'Zuma_X': zuma_draw_x,
        'Menu_X': menu_x
    }

def generate_menubar(v_rgb, v_mask, target_w, target_h, assets_dir=None):
    """
    Procedurally generates custom menubar and mask for target resolution.
    Strictly enforces minimum 480x320 and zero distortion / 1.0x native button sizing.
    """
    target_w = max(MIN_W, target_w)
    target_h = max(MIN_H, target_h)

    if assets_dir is None:
        assets_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'assets', 'media')

    out_rgb = np.zeros((target_h, target_w, 3), dtype=np.uint8)
    out_mask = np.zeros((target_h, target_w), dtype=np.uint8)

    pillar_w = 24

    tile_15_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_tile_swirl_15px.png')))
    tile_15_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_tile_swirl_15px.png')))
    tile_26b_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_tile_swirl_26px_b.png')))
    tile_26b_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_tile_swirl_26px_b.png')))

    p_tile_l_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_tile_pillar_l.png')))
    p_tile_l_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_tile_pillar_l.png')))
    p_tile_r_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_tile_pillar_r.png')))
    p_tile_r_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_tile_pillar_r.png')))

    b_tile_l_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_tile_bottom_l.png')))
    b_tile_l_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_tile_bottom_l.png')))
    b_tile_r_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_tile_bottom_r.png')))
    b_tile_r_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_tile_bottom_r.png')))

    # 1. PILLARS
    mid_len = target_h - 42 - 24
    if mid_len >= 0:
        pillar_l_rgb = np.vstack([v_rgb[:42, :pillar_w], tile_vertical(p_tile_l_rgb, mid_len), v_rgb[456:, :pillar_w]])
        pillar_l_m   = np.vstack([v_mask[:42, :pillar_w], tile_vertical(p_tile_l_m, mid_len), v_mask[456:, :pillar_w]])
        pillar_r_rgb = np.vstack([v_rgb[:42, 640-pillar_w:], tile_vertical(p_tile_r_rgb, mid_len), v_rgb[456:, 640-pillar_w:]])
        pillar_r_m   = np.vstack([v_mask[:42, 640-pillar_w:], tile_vertical(p_tile_r_m, mid_len), v_mask[456:, 640-pillar_w:]])
    else:
        pillar_l_rgb = v_rgb[:target_h, :pillar_w]
        pillar_l_m   = v_mask[:target_h, :pillar_w]
        pillar_r_rgb = v_rgb[:target_h, 640-pillar_w:]
        pillar_r_m   = v_mask[:target_h, 640-pillar_w:]

    out_rgb[:, :pillar_w] = pillar_l_rgb[:target_h]
    out_mask[:, :pillar_w] = pillar_l_m[:target_h]
    out_rgb[:, target_w-pillar_w:] = pillar_r_rgb[:target_h]
    out_mask[:, target_w-pillar_w:] = pillar_r_m[:target_h]

    # 2. TOP BAR & BOTTOM BORDER
    if target_w >= 540:
        # Standard Widescreen/Desktop (W >= 540): 5 buttons at top with expanded Aztec swirl bridges
        c1_rgb, c1_m = v_rgb[:42, 0:134], v_mask[:42, 0:134]
        c2_rgb, c2_m = v_rgb[:42, 133:242], v_mask[:42, 133:242]
        c3_rgb, c3_m = v_rgb[:42, 270:383], v_mask[:42, 270:383]
        c4_rgb, c4_m = v_rgb[:42, 390:503], v_mask[:42, 390:503]
        c5_rgb, c5_m = v_rgb[:42, 518:640], v_mask[:42, 518:640]

        br_a_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_bridge_a.png')))
        br_a_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_bridge_a.png')))
        br_b_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_bridge_b.png')))
        br_b_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_bridge_b.png')))
        br_c_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_bridge_c.png')))
        br_c_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_bridge_c.png')))
        br_d_rgb = np.array(Image.open(os.path.join(assets_dir, 'menubar_bridge_d.png')))
        br_d_m   = np.array(Image.open(os.path.join(assets_dir, '_menubar_bridge_d.png')))

        extra_w = target_w - 800
        if extra_w >= 0:
            add_a = int(round(extra_w * 0.15))
            add_b = int(round(extra_w * 0.35))
            add_c = int(round(extra_w * 0.35))
            add_d = extra_w - add_a - add_b - add_c

            bridge_a_rgb = np.hstack([br_a_rgb, tile_horizontal(tile_15_rgb, add_a)])
            bridge_a_m   = np.hstack([br_a_m,   tile_horizontal(tile_15_m, add_a)])
            bridge_b_rgb = np.hstack([br_b_rgb, tile_horizontal(tile_26b_rgb, add_b)])
            bridge_b_m   = np.hstack([br_b_m,   tile_horizontal(tile_26b_m, add_b)])
            bridge_c_rgb = np.hstack([br_c_rgb, tile_horizontal(tile_26b_rgb, add_c)])
            bridge_c_m   = np.hstack([br_c_m,   tile_horizontal(tile_26b_m, add_c)])
            bridge_d_rgb = np.hstack([br_d_rgb, tile_horizontal(tile_15_rgb, add_d)])
            bridge_d_m   = np.hstack([br_d_m,   tile_horizontal(tile_15_m, add_d)])
        else:
            shrink = -extra_w
            sh_a = int(round(shrink * (30 / 209)))
            sh_b = int(round(shrink * (77 / 209)))
            sh_c = int(round(shrink * (57 / 209)))
            sh_d = shrink - sh_a - sh_b - sh_c

            bridge_a_rgb = br_a_rgb[:, :max(0, 30 - sh_a)]
            bridge_a_m   = br_a_m[:,   :max(0, 30 - sh_a)]
            bridge_b_rgb = br_b_rgb[:, :max(0, 77 - sh_b)]
            bridge_b_m   = br_b_m[:,   :max(0, 77 - sh_b)]
            bridge_c_rgb = br_c_rgb[:, :max(0, 57 - sh_c)]
            bridge_c_m   = br_c_m[:,   :max(0, 57 - sh_c)]
            bridge_d_rgb = br_d_rgb[:, :max(0, 45 - sh_d)]
            bridge_d_m   = br_d_m[:,   :max(0, 45 - sh_d)]

        top_rgb = np.hstack([c1_rgb, bridge_a_rgb, c2_rgb, bridge_b_rgb, c3_rgb, bridge_c_rgb, c4_rgb, bridge_d_rgb, c5_rgb])
        top_mask = np.hstack([c1_m, bridge_a_m, c2_m, bridge_b_m, c3_m, bridge_c_m, c4_m, bridge_d_m, c5_m])

        # Bottom Border
        v_bot_rgb = v_rgb[456:480, :]
        v_bot_mask = v_mask[456:480, :]
        delta_w = target_w - 640
        dw_l = delta_w // 2
        dw_r = delta_w - dw_l

        if dw_l >= 0:
            bot_l_rgb = np.hstack([v_bot_rgb[:, :124], tile_horizontal(b_tile_l_rgb, dw_l), v_bot_rgb[:, 124:320]])
            bot_l_mask = np.hstack([v_bot_mask[:, :124], tile_horizontal(b_tile_l_m, dw_l), v_bot_mask[:, 124:320]])
            bot_r_rgb = np.hstack([v_bot_rgb[:, 320:526], tile_horizontal(b_tile_r_rgb, dw_r), v_bot_rgb[:, 526:640]])
            bot_r_mask = np.hstack([v_bot_mask[:, 320:526], tile_horizontal(b_tile_r_m, dw_r), v_bot_mask[:, 526:640]])
        else:
            bot_l_rgb = np.hstack([v_bot_rgb[:, :124+dw_l], v_bot_rgb[:, 124:320]])
            bot_l_mask = np.hstack([v_bot_mask[:, :124+dw_l], v_bot_mask[:, 124:320]])
            bot_r_rgb = np.hstack([v_bot_rgb[:, 320:526], v_bot_rgb[:, 526-dw_r:640]])
            bot_r_mask = np.hstack([v_bot_mask[:, 320:526], v_bot_mask[:, 526-dw_r:640]])

        bot_rgb = np.hstack([bot_l_rgb, bot_r_rgb])
        bot_mask = np.hstack([bot_l_mask, bot_r_mask])

    else:
        # Minimum Boundary (480 <= W < 540, e.g. 480x320):
        # 5 Buttons at top with bridges minimized and smoothstep alpha blending between elements!
        # ZERO DISTORTION! 1.0x native unscaled button compartments!
        s1_rgb, s1_m = v_rgb[:42, :126], v_mask[:42, :126]
        s2_rgb, s2_m = v_rgb[:42, 165:241], v_mask[:42, 165:241]
        s3_rgb, s3_m = v_rgb[:42, 260:381], v_mask[:42, 260:381]
        s4_rgb, s4_m = v_rgb[:42, 399:475], v_mask[:42, 399:475]
        s5_rgb, s5_m = v_rgb[:42, 532:640], v_mask[:42, 532:640]

        extra = target_w - 479
        ba = extra // 4
        bb = extra // 4
        bc = extra // 4
        bd = extra - ba - bb - bc

        # If extra width exists between 480 and 540, insert small bridge tiles seamlessly
        p12_rgb = np.hstack([s1_rgb, tile_horizontal(tile_15_rgb, ba)])
        p12_m   = np.hstack([s1_m,   tile_horizontal(tile_15_m,   ba)])
        b12_rgb = blend_pair(p12_rgb, s2_rgb, blend_w=7)
        b12_m   = blend_pair(p12_m,   s2_m,   blend_w=7)

        p23_rgb = np.hstack([b12_rgb, tile_horizontal(tile_26b_rgb, bb)])
        p23_m   = np.hstack([b12_m,   tile_horizontal(tile_26b_m,   bb)])
        b123_rgb = blend_pair(p23_rgb, s3_rgb, blend_w=7)
        b123_m   = blend_pair(p23_m,   s3_m,   blend_w=7)

        p34_rgb = np.hstack([b123_rgb, tile_horizontal(tile_26b_rgb, bc)])
        p34_m   = np.hstack([b123_m,   tile_horizontal(tile_26b_m,   bc)])
        b1234_rgb = blend_pair(p34_rgb, s4_rgb, blend_w=7)
        b1234_m   = blend_pair(p34_m,   s4_m,   blend_w=7)

        p45_rgb = np.hstack([b1234_rgb, tile_horizontal(tile_15_rgb, bd)])
        p45_m   = np.hstack([b1234_m,   tile_horizontal(tile_15_m,   bd)])
        top_rgb = blend_pair(p45_rgb, s5_rgb, blend_w=7)
        top_mask = blend_pair(p45_m,   s5_m,   blend_w=7)

        if top_rgb.shape[1] < target_w:
            diff = target_w - top_rgb.shape[1]
            top_rgb = np.hstack([top_rgb[:, :-24], np.repeat(top_rgb[:, -25:-24], diff, axis=1), top_rgb[:, -24:]])
            top_mask = np.hstack([top_mask[:, :-24], np.repeat(top_mask[:, -25:-24], diff, axis=1), top_mask[:, -24:]])
        elif top_rgb.shape[1] > target_w:
            top_rgb = top_rgb[:, :target_w]
            top_mask = top_mask[:, :target_w]

        # Bottom Border: Aztec center motif at W//2
        v_bot_rgb = v_rgb[456:480, :]
        v_bot_mask = v_mask[456:480, :]
        c_motif_rgb = v_bot_rgb[:, 280:360]
        c_motif_m   = v_bot_mask[:, 280:360]
        c_bl_rgb    = v_bot_rgb[:, :24]
        c_bl_m      = v_bot_mask[:, :24]
        c_br_rgb    = v_bot_rgb[:, 616:]
        c_br_m      = v_bot_mask[:, 616:]

        side_w = (target_w - 48 - 80) // 2
        ins_l_rgb = tile_horizontal(b_tile_l_rgb, side_w)
        ins_l_m   = tile_horizontal(b_tile_l_m, side_w)
        ins_r_rgb = tile_horizontal(b_tile_r_rgb, side_w)
        ins_r_m   = tile_horizontal(b_tile_r_m, side_w)

        bot_rgb = np.hstack([c_bl_rgb, ins_l_rgb, c_motif_rgb, ins_r_rgb, c_br_rgb])
        bot_mask = np.hstack([c_bl_m, ins_l_m, c_motif_m, ins_r_m, c_br_m])

    out_rgb[:42, :target_w] = top_rgb[:, :target_w]
    out_mask[:42, :target_w] = top_mask[:, :target_w]
    out_rgb[target_h-24:, :target_w] = bot_rgb[:, :target_w]
    out_mask[target_h-24:, :target_w] = bot_mask[:, :target_w]

    # Inner window clearing
    out_rgb[42:target_h-24, pillar_w:target_w-pillar_w] = 0
    out_mask[42:target_h-24, pillar_w:target_w-pillar_w] = 0

    return out_rgb, out_mask

def save_custom_menubar(out_rgb, out_mask, out_jpg_path, out_gif_path):
    os.makedirs(os.path.dirname(os.path.abspath(out_jpg_path)), exist_ok=True)
    
    im_rgb = Image.fromarray(out_rgb)
    im_rgb.save(out_jpg_path, 'JPEG', quality=95)

    im_mask = Image.fromarray(out_mask, mode='L')
    im_mask_p = im_mask.convert('P', palette=Image.Palette.ADAPTIVE, colors=256)
    im_mask_p.save(out_gif_path, 'GIF')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="Generate custom-resolution Zuma Deluxe menubar HUD assets.\n"
                    "Requires vanilla menubar.jpg and _menubar.gif from the game's images/ directory."
    )
    parser.add_argument("zuma_dir", help="Path to the vanilla Zuma Deluxe installation directory (must contain images/menubar.jpg)")
    parser.add_argument("--width",  "-W", type=int, default=800, help="Target resolution width  (default: 800)")
    parser.add_argument("--height", "-H", type=int, default=600, help="Target resolution height (default: 600)")
    parser.add_argument("--out-dir", default=None,
                        help="Output directory for generated assets (default: <zuma_dir>/imagescustomres)")
    args = parser.parse_args()

    vanilla_jpg = os.path.join(args.zuma_dir, 'images', 'Menubar.jpg')
    if not os.path.exists(vanilla_jpg):
        vanilla_jpg = os.path.join(args.zuma_dir, 'images', 'menubar.jpg')
    vanilla_gif = os.path.join(args.zuma_dir, 'images', '_menubar.gif')

    if not os.path.exists(vanilla_jpg) or not os.path.exists(vanilla_gif):
        print(f"[ERROR] Vanilla menubar not found at:\n  {vanilla_jpg}\n  {vanilla_gif}")
        sys.exit(1)

    assets_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'assets', 'media')
    ensure_master_assets(assets_dir)

    v_rgb  = np.array(Image.open(vanilla_jpg).convert('RGB'))
    v_mask = np.array(Image.open(vanilla_gif).convert('L'))

    out_dir = args.out_dir or os.path.join(args.zuma_dir, 'imagescustomres')
    os.makedirs(out_dir, exist_ok=True)

    rgb, mask = generate_menubar(v_rgb, v_mask, args.width, args.height, assets_dir)
    jpg_path  = os.path.join(out_dir, 'menubar.jpg')
    gif_path  = os.path.join(out_dir, '_menubar.gif')
    save_custom_menubar(rgb, mask, jpg_path, gif_path)
    print(f"[OK] Generated {args.width}x{args.height} menubar -> {jpg_path}")
