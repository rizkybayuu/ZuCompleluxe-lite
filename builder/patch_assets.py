import os
import sys
import json
import re
import shutil
import numpy as np
from PIL import Image

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TABLES_PATH = os.path.join(SCRIPT_DIR, 'color_tables.json')

def load_tables():
    if os.path.exists(TABLES_PATH):
        with open(TABLES_PATH, 'r', encoding='utf-8') as f:
            return json.load(f)
    return {}

def clean_modules_bak(zuma_dir):
    """
    Ensures modules/bak contains strictly Zuma.exe (and .orig / .base.exe).
    Removes any asset backups (baDotz.gif, resources.xml, etc.) to maintain
    a pristine single-binary backup architecture.
    """
    bak_dir = os.path.join(zuma_dir, 'modules', 'bak')
    if not os.path.isdir(bak_dir):
        return

    allowed_exts = {'.exe', '.orig', '.base'}
    for f in os.listdir(bak_dir):
        p = os.path.join(bak_dir, f)
        if os.path.isfile(p):
            lower = f.lower()
            if not any(lower.endswith(ext) or ext in lower for ext in allowed_exts):
                try:
                    os.remove(p)
                    print(f"  [CLEANUP] Removed legacy backup from modules/bak: {f}")
                except Exception as e:
                    print(f"  [WARN] Could not remove {f} from modules/bak: {e}")

def transform_fallback_affine(rgb_arr, dst_color):
    # Dynamic affine fallback if a color is not in table
    r, g, b = rgb_arr[..., 0].astype(float), rgb_arr[..., 1].astype(float), rgb_arr[..., 2].astype(float)
    if dst_color == 'Orange':
        ro = np.clip(0.9202 * r + 1.0647 * g + 0.5275 * b, 0, 255)
        go = np.clip(0.7240 * g, 0, 255)
        bo = np.clip(0.5802 * r, 0, 255)
    elif dst_color == 'Navy':
        ro = np.clip(0.1927 * r - 0.1730 * g + 0.2830 * b - 0.0965, 0, 255)
        go = np.clip(0.8909 * r - 0.8568 * g + 1.4533 * b - 19.8374, 0, 255)
        bo = np.clip(1.2832 * r - 1.1449 * g + 1.2608 * b + 2.0065, 0, 255)
    elif dst_color == 'Cyan':
        ro = np.clip(0.9250 * g, 0, 255)
        go = np.clip(0.8937 * r, 0, 255)
        bo = np.clip(1.1964 * r - 0.1120 * g - 0.0697 * b, 0, 255)
    elif dst_color == 'Black':
        gray = np.clip(1.85 * r - 0.15 * g - 0.65 * b, 0, 255)
        ro, go, bo = gray, gray, gray
    elif dst_color == 'Cream':
        ro = np.clip(1.8323 * r - 0.8496 * g + 0.0252 * b - 3.4022, 0, 255)
        go = np.clip(2.8938 * r - 1.9396 * g + 0.0678 * b - 6.0122, 0, 255)
        bo = np.clip(0.7056 * r + 0.3436 * g - 0.3086 * b + 0.9749, 0, 255)
    else:
        ro, go, bo = r, g, b
    return np.stack([ro, go, bo], axis=-1).astype(np.uint8)

def patch_images(zuma_dir, tables, is_lite=False):
    img_dir = os.path.join(zuma_dir, 'images')
    if not os.path.isdir(img_dir):
        print(f"[WARN] Images directory not found: {img_dir}")
        return

    # Clean legacy .bak files if present in images/
    for f in os.listdir(img_dir):
        if f.lower().endswith('.bak'):
            try:
                os.remove(os.path.join(img_dir, f))
            except Exception:
                pass

    # 1. Process baDotz.gif (Ensures baDotz.gif is ALWAYS vanilla 6 dots: 6*h x h)
    dotz_path = os.path.join(img_dir, 'baDotz.gif')
    dotz_extra_path = os.path.join(img_dir, 'baDotz_extra.gif')

    if os.path.exists(dotz_path):
        im_dotz = Image.open(dotz_path)
        w, h = im_dotz.size
        w_dot = h

        # If baDotz.gif was previously extended to 11 dots, crop back to pristine 6 dots
        if w == 11 * h or (w % 11 == 0 and w // 11 == h and w > 6 * h):
            im_6 = im_dotz.crop((0, 0, 6 * w_dot, h))
            trans = im_dotz.info.get('transparency')
            im_6.save(dotz_path, format='GIF', transparency=trans)
            im_dotz = Image.open(dotz_path)
            w, h = im_dotz.size
            print(f"  [RESTORE] Restored baDotz.gif to native 6-dot resolution ({w}x{h})")

        # In Full Edition, generate baDotz_extra.gif (11 dots)
        if not is_lite:
            im_rgba = im_dotz.convert('RGBA')
            arr_c = np.array(im_rgba)
            dot_map_keys = [
                (0, 'baDotz_Dot1_to_Dot7_Orange', 'Orange'),
                (1, 'baDotz_Dot2_to_Dot8_Navy', 'Navy'),
                (2, 'baDotz_Dot3_to_Dot9_Cyan', 'Cyan'),
                (3, 'baDotz_Dot4_to_Dot10_Black', 'Black'),
                (5, 'baDotz_Dot6_to_Dot11_Cream', 'Cream')
            ]
            new_dots = []
            for s_idx, tbl_key, dst_col in dot_map_keys:
                s_slice = arr_c[:, s_idx*w_dot : (s_idx+1)*w_dot, :3]
                lut = tables.get(tbl_key, {})
                out_dot = np.zeros_like(s_slice)
                for y in range(h):
                    for x in range(w_dot):
                        st = s_slice[y, x]
                        sk = f"{st[0]},{st[1]},{st[2]}"
                        if sk in lut:
                            out_dot[y, x] = lut[sk]
                        else:
                            out_dot[y, x] = transform_fallback_affine(st[None, :], dst_col)[0]
                new_dots.append(out_dot)

            dotz_new_rgb = np.hstack([arr_c[:, :6*w_dot, :3]] + new_dots)
            dotz_new_rgba = np.dstack([dotz_new_rgb, np.full((h, w_dot * 11), 255, dtype=np.uint8)])
            out_im = Image.fromarray(dotz_new_rgba, mode='RGBA')
            out_gif = out_im.convert('P', palette=Image.ADAPTIVE, colors=256)
            out_gif.save(dotz_extra_path, format='GIF')
            print(f"  [OK] Generated baDotz_extra.gif ({w_dot*11}x{h}, 11 dots)")

    # 2. In Full Edition, generate 30 extra color files (Orange, Navy, Cyan, Black, Cream)
    if not is_lite:
        pairs = [
            ('Blue', 'Orange'),
            ('Yellow', 'Navy'),
            ('Red', 'Cyan'),
            ('Green', 'Black'),
            ('White', 'Cream')
        ]
        types = ['Accuracy', 'Backwards', 'Ball', 'Bomb', 'Light', 'Slow']

        print(f"[ASSET] Patching extra color images into: {img_dir}")
        for src_col, dst_col in pairs:
            for t in types:
                src_name = f'ba{t}{src_col}.gif'
                dst_name = f'ba{t}{dst_col}.gif'
                src_path = os.path.join(img_dir, src_name)
                dst_path = os.path.join(img_dir, dst_name)
                
                if not os.path.exists(src_path):
                    continue
                    
                im_s = Image.open(src_path)
                w, h = im_s.size
                bg_idx = np.array(im_s)[0, 0]
                
                table_key = f'ba{t}_{src_col}_to_{dst_col}'
                lut = tables.get(table_key, {})
                
                pal_s = np.array(im_s.getpalette()).reshape(-1, 3)
                new_pal = pal_s.copy()
                
                for idx in range(len(pal_s)):
                    sk = f"{pal_s[idx, 0]},{pal_s[idx, 1]},{pal_s[idx, 2]}"
                    if sk in lut:
                        new_pal[idx] = lut[sk]
                    else:
                        single_rgb = pal_s[idx:idx+1, :]
                        new_pal[idx] = transform_fallback_affine(single_rgb, dst_col)[0]
                        
                im_gen = im_s.copy()
                im_gen.putpalette(new_pal.flatten().tolist())
                trans_val = im_s.info.get('transparency')
                if trans_val is None:
                    trans_val = int(bg_idx)
                im_gen.save(dst_path, format='GIF', transparency=trans_val)
                print(f"  [OK] {dst_name} ({w}x{h})")


def patch_resources_file(zuma_dir, is_lite=False):
    """
    Maintains dual-manifest architecture:
    - properties/resources.xml: STRICTLY vanilla 6 colors (baDotz, cols=6).
    - properties/resource_extra.xml: 11 colors (baDotz_extra, cols=11).
    """
    p_res = os.path.join(zuma_dir, 'properties', 'resources.xml')
    if not os.path.isfile(p_res):
        print(f"[WARN] resources.xml not found at: {p_res}")
        return

    with open(p_res, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()

    # Clean resources.xml so that it is strictly 6 colors
    balls_pat = r'<Image\s+id="WHITE_BALL"[^>]*>.*?</Image>|<Image\s+id="WHITE_BALL"[^>]*/>(?:\s*<Image\s+id="(?:ORANGE|NAVY|CYAN|BLACK|CREAM)_BALL"[^>]*/>)+'
    vanilla_white_ball = '<Image id="WHITE_BALL"\t\t\t\t\t\tpath="baBallWhite" alphagrid="_ballalpha" rows=50 alias="BALL_END" />'
    text_clean = re.sub(balls_pat, vanilla_white_ball, text, flags=re.S)

    # Remove extra powerup tags: ORANGE_, NAVY_, CYAN_, BLACK_, CREAM_
    text_clean = re.sub(r'\s*<Image\s+id="(?:ORANGE|NAVY|CYAN|BLACK|CREAM)_[^>]*/>', '', text_clean)

    # Ensure NEXT_BALL is baDotz cols=6 in vanilla manifest
    text_clean = re.sub(r'<Image\s+id="NEXT_BALL"[^>]*/>', '<Image id="NEXT_BALL"\t\t\t\t\t\tpath="baDotz" cols=6 />', text_clean)

    # Register DIALOG_BUTTON_LOCKED under Register Resources
    if 'DIALOG_BUTTON_LOCKED' not in text_clean:
        def insert_locked_button(m):
            return m.group(0) + '\n<Image id="DIALOG_BUTTON_LOCKED"\t\tpath="DialogButtonLocked" />'
        text_clean = re.sub(r'<Image\s+id="DIALOG_BUTTON"\s+path="Dialogbutton"\s*/>', insert_locked_button, text_clean)

    # Register ADVTEMPLE5 and ADVDOOR5 under AdventureScreen
    if 'ADVTEMPLE5' in text_clean:
        m_t5 = re.search(r'<Image\s+id="ADVTEMPLE5"[^>]*/>', text_clean)
        if m_t5 and 'alphaimage=' not in m_t5.group(0):
            text_clean = re.sub(r'(<Image\s+id="ADVTEMPLE5"[^/]*)(/>)', r'\g<1>alphaimage="_advtemple5" \g<2>', text_clean)
    else:
        def insert_temple5(m):
            return m.group(0) + '\n<Image id="ADVTEMPLE5"\t\t\t\t\tpath="advtemple5" alphaimage="_advtemple5" />'
        text_clean = re.sub(r'<Image\s+id="ADVTEMPLE4"[^>]*/>', insert_temple5, text_clean)

    if 'ADVDOOR5' not in text_clean:
        def insert_door5(m):
            return m.group(0) + '\n<Image id="ADVDOOR5"\t\t\t\t\tpath="advdoor5" cols="3" />'
        text_clean = re.sub(r'<Image\s+id="ADVDOOR4C"[^>]*/>', insert_door5, text_clean)

    # Register HELP_FRONT_DEV, HELP_BACK_DEV, TUT_MOUSE_M, and TUT_TEMPLE5
    if 'HELP_FRONT_DEV' not in text_clean:
        def insert_help_dev(m):
            return (
                m.group(0) + '\n'
                '<Image id="HELP_FRONT_DEV"\t\t\t\t\tpath="helpfront_dev" />\n'
                '<Image id="HELP_BACK_DEV"\t\t\t\t\tpath="helpback_dev" />\n'
                '<Image id="TUT_MOUSE_M"\t\t\t\t\t\tpath="tut_mouse_m" cols="2" />\n'
                '<Image id="TUT_TEMPLE5"\t\t\t\t\tpath="advtemple5_tut" />'
            )
        text_clean = re.sub(r'<Image\s+id="HELP_BACK"[^>]*/>', insert_help_dev, text_clean)

    # Clean up any obsolete TUT_TITLE and TUT_BALL entries from resources.xml
    text_clean = re.sub(r'[ \t]*<Image\s+id="TUT_(?:TITLE|BALL)_[^>]*/>\r?\n?', '', text_clean)
    if 'cols=2' in text_clean:
        text_clean = text_clean.replace('cols=2', 'cols="2"')

    # Save clean 6-color manifest to properties/resources.xml
    with open(p_res, 'w', encoding='utf-8', newline='\r\n') as f:
        f.write(text_clean)
    print("  [OK] resources.xml preserved as clean vanilla 6-color manifest (cols=6).")

    # In Full Edition, generate 11-color properties/resource_extra.xml & resources_extra.xml
    if not is_lite:
        extra_balls = (
            '<Image id="WHITE_BALL"\t\t\t\t\t\tpath="baBallWhite" alphagrid="_ballalpha" rows=50 />\n'
            '<Image id="ORANGE_BALL"\t\t\t\t\t\tpath="baBallOrange" alphagrid="_ballalpha" rows=47 />\n'
            '<Image id="NAVY_BALL"\t\t\t\t\t\tpath="baBallNavy" alphagrid="_ballalpha" rows=50 />\n'
            '<Image id="CYAN_BALL"\t\t\t\t\t\tpath="baBallCyan" alphagrid="_ballalpha" rows=50 />\n'
            '<Image id="BLACK_BALL"\t\t\t\t\t\tpath="baBallBlack" alphagrid="_ballalpha" rows=50 />\n'
            '<Image id="CREAM_BALL"\t\t\t\t\t\tpath="baBallCream" alphagrid="_ballalpha" rows=50 alias="BALL_END" />'
        )
        text_extra = re.sub(r'<Image\s+id="WHITE_BALL"[^>]*alias="BALL_END"[^>]*/>', extra_balls, text_clean)
        text_extra = re.sub(r'<Image\s+id="NEXT_BALL"[^>]*/>', '<Image id="NEXT_BALL"\t\t\t\t\t\tpath="baDotz_extra" cols=11 />', text_extra)

        sections = [
            ('WHITE_ACCURACY', 'ACCURACY', 'baAccuracy', '_ballalpha'),
            ('WHITE_BACKWARDS', 'BACKWARDS', 'baBackwards', '_ballalpha'),
            ('WHITE_SLOW', 'SLOW', 'baSlow', '_ballalpha'),
            ('WHITE_BOMB', 'BOMB', 'baBomb', '_baBomb'),
            ('WHITE_LIGHT', 'LIGHT', 'baLight', '_baLight')
        ]
        colors = ['ORANGE', 'NAVY', 'CYAN', 'BLACK', 'CREAM']
        for anchor, type_name, path_prefix, alpha_name in sections:
            inserted = ['\\g<0>']
            for col in colors:
                line = f'<Image id="{col}_{type_name}"\t\t\t\t\tpath="{path_prefix}{col.capitalize()}" alphaimage="{alpha_name}" />'
                inserted.append(line)
            pattern = rf'<Image\s+id="{anchor}"[^>]*/>'
            text_extra = re.sub(pattern, '\n'.join(inserted), text_extra)

        p_extra = os.path.join(zuma_dir, 'properties', 'resource_extra.xml')
        p_extra_alt = os.path.join(zuma_dir, 'properties', 'resources_extra.xml')
        with open(p_extra, 'w', encoding='utf-8', newline='\r\n') as f:
            f.write(text_extra)
        with open(p_extra_alt, 'w', encoding='utf-8', newline='\r\n') as f:
            f.write(text_extra)
        print("  [OK] resource_extra.xml generated with 11-color support (baDotz_extra, cols=11).")


def patch_dialog_button(zuma_dir, tables):
    img_dir = os.path.join(zuma_dir, 'images')
    if not os.path.isdir(img_dir):
        return

    jpg_path = os.path.join(img_dir, 'DialogButton.jpg')
    gif_path = os.path.join(img_dir, '_Dialogbutton.gif')
    locked_jpg = os.path.join(img_dir, 'DialogButtonLocked.jpg')
    locked_gif = os.path.join(img_dir, '_Dialogbuttonlocked.gif')

    # Restore vanilla DialogButton.jpg and _Dialogbutton.gif if .bak exists
    bak_jpg = jpg_path + '.bak'
    if os.path.exists(bak_jpg):
        shutil.copyfile(bak_jpg, jpg_path)
        try:
            os.remove(bak_jpg)
        except Exception:
            pass

    bak_gif = gif_path + '.bak'
    if os.path.exists(bak_gif):
        shutil.copyfile(bak_gif, gif_path)
        try:
            os.remove(bak_gif)
        except Exception:
            pass

    # Always generate DialogButtonLocked.jpg from DialogButton.jpg
    if os.path.exists(jpg_path):
        im_src = Image.open(jpg_path).convert('RGB')
        arr_s = np.array(im_src)
        h, w, _ = arr_s.shape
        lut = tables.get('DialogButton_to_Locked', {})
        arr_out = np.zeros_like(arr_s)

        for y in range(h):
            for x in range(w):
                sk = f"{arr_s[y, x, 0]},{arr_s[y, x, 1]},{arr_s[y, x, 2]}"
                if sk in lut:
                    arr_out[y, x] = lut[sk]
                else:
                    gray = int(np.clip(0.299 * arr_s[y, x, 0] + 0.587 * arr_s[y, x, 1] + 0.114 * arr_s[y, x, 2], 0, 255))
                    arr_out[y, x] = [gray, gray, gray]

        im_locked = Image.fromarray(arr_out)
        im_locked.save(locked_jpg, format='JPEG', quality=95)
        print("  [OK] Generated DialogButtonLocked.jpg (grayscale disabled style)")

    if os.path.exists(gif_path) and not os.path.exists(locked_gif):
        shutil.copyfile(gif_path, locked_gif)
        print("  [OK] Copied _Dialogbuttonlocked.gif mask")


def patch_temple5_media(zuma_dir):
    src_media = os.path.join(SCRIPT_DIR, '..', 'assets', 'media')
    if not os.path.isdir(src_media):
        return
    img_dir = os.path.join(zuma_dir, 'images')
    if not os.path.isdir(img_dir):
        return

    t5_jpg = os.path.join(src_media, 'advtemple5.jpg')
    dst_t5 = os.path.join(img_dir, 'advtemple5.jpg')
    if os.path.exists(t5_jpg):
        shutil.copyfile(t5_jpg, dst_t5)
        print("  [OK] Copied advtemple5.jpg (330x99) to images/")

    t5_mask = os.path.join(src_media, '_advtemple5.gif')
    dst_mask = os.path.join(img_dir, '_advtemple5.gif')
    if os.path.exists(t5_mask):
        shutil.copyfile(t5_mask, dst_mask)
        print("  [OK] Copied _advtemple5.gif (330x99) to images/")

    d5_jpg = os.path.join(src_media, 'advdoor5.jpg')
    dst_d5 = os.path.join(img_dir, 'advdoor5.jpg')
    if os.path.exists(d5_jpg):
        shutil.copyfile(d5_jpg, dst_d5)
        print("  [OK] Copied advdoor5.jpg (192x50, 3-frame cel) to images/")


def patch_dev_media(zuma_dir):
    src_media = os.path.join(SCRIPT_DIR, '..', 'assets', 'media')
    if not os.path.isdir(src_media):
        return
    img_dir = os.path.join(zuma_dir, 'images')
    if not os.path.isdir(img_dir):
        return

    print(f"[ASSET] Patching DevTester HelpScreen media into: {img_dir}")

    # 1. helpfront_dev.jpg (normalized to 640x480) & _helpfront_dev.gif
    hf_jpg = os.path.join(src_media, 'helpfront_dev.jpg')
    dst_hf = os.path.join(img_dir, 'helpfront_dev.jpg')
    if os.path.exists(hf_jpg):
        im = Image.open(hf_jpg)
        if im.width != 640 or im.height != 480:
            im = im.crop((0, 0, 640, 480))
            im.save(dst_hf, format='JPEG', quality=95)
            print("  [OK] Normalized and saved helpfront_dev.jpg (640x480) to images/")
        else:
            shutil.copyfile(hf_jpg, dst_hf)
            print("  [OK] Copied helpfront_dev.jpg to images/")

    hf_gif = os.path.join(src_media, '_helpfront_dev.gif')
    if os.path.exists(hf_gif):
        shutil.copyfile(hf_gif, os.path.join(img_dir, '_helpfront_dev.gif'))
        print("  [OK] Copied _helpfront_dev.gif to images/")

    # 2. helpback_dev.jpg (normalized to 618x363)
    hb_jpg = os.path.join(src_media, 'helpback_dev.jpg')
    dst_hb = os.path.join(img_dir, 'helpback_dev.jpg')
    if os.path.exists(hb_jpg):
        im = Image.open(hb_jpg)
        if im.width != 618 or im.height != 363:
            im = im.crop((0, 0, 618, 363))
            im.save(dst_hb, format='JPEG', quality=95)
            print("  [OK] Normalized and saved helpback_dev.jpg (618x363) to images/")
        else:
            shutil.copyfile(hb_jpg, dst_hb)
            print("  [OK] Copied helpback_dev.jpg to images/")

    # 3. tut_mouse_m.jpg (40x29) & tut_mouse_m_.jpg
    mm_jpg = os.path.join(src_media, 'tut_mouse_m.jpg')
    if os.path.exists(mm_jpg):
        shutil.copyfile(mm_jpg, os.path.join(img_dir, 'tut_mouse_m.jpg'))
        print("  [OK] Copied tut_mouse_m.jpg to images/")

    mr_mask = os.path.join(img_dir, 'tut_mouse_r_.jpg')
    dst_mm_mask = os.path.join(img_dir, 'tut_mouse_m_.jpg')
    if os.path.exists(mr_mask):
        shutil.copyfile(mr_mask, dst_mm_mask)
        print("  [OK] Created tut_mouse_m_.jpg from tut_mouse_r_.jpg")

    # 4. advtemple5_tut.jpg (190x75) preview of Stage 13
    src_temple = os.path.join(img_dir, 'advtemple5.jpg')
    src_door = os.path.join(img_dir, 'advdoor5.jpg')
    dst_tut_temple = os.path.join(img_dir, 'advtemple5_tut.jpg')
    if os.path.exists(src_temple) and os.path.exists(src_door):
        from PIL import ImageFilter
        temple = Image.open(src_temple).convert('RGB')
        door = Image.open(src_door).convert('RGB')
        frame1 = door.crop((64, 0, 128, 50))
        comp = temple.copy()
        comp.paste(frame1, (165, 28))
        glow_layer = Image.new('RGBA', comp.size, (0, 0, 0, 0))
        bloom = Image.new('RGBA', (140, 90), (0, 255, 120, 160))
        glow_layer.paste(bloom, (197 - 70, 53 - 45))
        glow_layer = glow_layer.filter(ImageFilter.GaussianBlur(15))
        comp_final = Image.alpha_composite(comp.convert('RGBA'), glow_layer)
        comp_final.paste(frame1.convert('RGBA'), (165, 28))
        crop_x1 = 197 - 110
        crop_x2 = 197 + 110
        cropped = comp_final.crop((crop_x1, 0, crop_x2, 99))
        resized = cropped.resize((190, 75), Image.Resampling.LANCZOS)
        resized.convert('RGB').save(dst_tut_temple, format='JPEG', quality=95)
        print("  [OK] Generated and saved advtemple5_tut.jpg (190x75) to images/")


def patch_outscreen_media(zuma_dir):
    src_media = os.path.join(SCRIPT_DIR, '..', 'assets', 'media')
    if not os.path.isdir(src_media):
        return
    img_dir = os.path.join(zuma_dir, 'images')
    if not os.path.isdir(img_dir):
        return
    out_jpg = os.path.join(src_media, 'outscreen.jpg')
    dst_out = os.path.join(img_dir, 'outscreen.jpg')
    if os.path.exists(out_jpg):
        shutil.copyfile(out_jpg, dst_out)
        print("  [OK] Copied outscreen.jpg (160x160) to images/")


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    flags = [a.lower() for a in sys.argv[1:] if a.startswith('--')]
    if not args:
        print("Usage: patch_assets.py <zuma_directory> [--lite]")
        sys.exit(1)
        
    z_dir = os.path.abspath(args[0])
    if not os.path.isdir(z_dir):
        print(f"[ERROR] Directory does not exist: {z_dir}")
        sys.exit(1)
        
    is_lite = '--lite' in flags or os.path.basename(os.path.abspath('.')).lower().endswith('-lite')
    tbls = load_tables()
    
    clean_modules_bak(z_dir)
    patch_resources_file(z_dir, is_lite=is_lite)
    patch_images(z_dir, tbls, is_lite=is_lite)
    patch_dialog_button(z_dir, tbls)
    patch_temple5_media(z_dir)
    patch_dev_media(z_dir)
    patch_outscreen_media(z_dir)
    clean_modules_bak(z_dir)
    print("[DONE] Assets and XML patched successfully.")
