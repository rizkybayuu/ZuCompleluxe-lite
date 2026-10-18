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

def get_backup_path(zuma_dir, filename):
    bak_dir = os.path.join(zuma_dir, 'modules', 'bak')
    os.makedirs(bak_dir, exist_ok=True)
    return os.path.join(bak_dir, filename)

def migrate_legacy_backup(legacy_bak_path, new_bak_path):
    if os.path.exists(legacy_bak_path):
        if not os.path.exists(new_bak_path):
            shutil.move(legacy_bak_path, new_bak_path)
            print(f"  [BACKUP] Migrated {os.path.basename(legacy_bak_path)} -> modules/bak/{os.path.basename(new_bak_path)}")
        else:
            try:
                os.remove(legacy_bak_path)
            except Exception:
                pass

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

def patch_images(zuma_dir, tables):
    img_dir = os.path.join(zuma_dir, 'images')
    if not os.path.isdir(img_dir):
        print(f"[WARN] Images directory not found: {img_dir}")
        return

    pairs = [
        ('Blue', 'Orange'),
        ('Yellow', 'Navy'),
        ('Red', 'Cyan'),
        ('Green', 'Black'),
        ('White', 'Cream')
    ]
    types = ['Accuracy', 'Backwards', 'Ball', 'Bomb', 'Light', 'Slow']

    print(f"[ASSET] Patching extra color images into: {img_dir}")
    
    # 1. Process 30 extra color files
    for src_col, dst_col in pairs:
        for t in types:
            src_name = f'ba{t}{src_col}.gif'
            dst_name = f'ba{t}{dst_col}.gif'
            src_path = os.path.join(img_dir, src_name)
            dst_path = os.path.join(img_dir, dst_name)
            
            if not os.path.exists(src_path):
                continue
                
            im_s = Image.open(src_path)
            w, h = im_s.size  # Dynamic XY dimension
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
                    # Dynamic fallback
                    single_rgb = pal_s[idx:idx+1, :]
                    new_pal[idx] = transform_fallback_affine(single_rgb, dst_col)[0]
                    
            im_gen = im_s.copy()
            im_gen.putpalette(new_pal.flatten().tolist())
            # Save preserving background transparency index
            trans_val = im_s.info.get('transparency')
            if trans_val is None:
                trans_val = int(bg_idx)
            im_gen.save(dst_path, format='GIF', transparency=trans_val)
            print(f"  [OK] {dst_name} ({w}x{h})")

    # 2. Process baDotz.gif (Dynamic XY)
    dotz_path = os.path.join(img_dir, 'baDotz.gif')
    bak_dotz = get_backup_path(zuma_dir, 'baDotz.gif')
    migrate_legacy_backup(dotz_path + '.bak', bak_dotz)

    # Determine pristine 6-dot original source
    src_dotz_path = None
    if os.path.exists(bak_dotz):
        src_dotz_path = bak_dotz
    elif os.path.exists(dotz_path):
        im_check = Image.open(dotz_path)
        w, h = im_check.size
        # Only backup if not already an 11-dot version
        if not (w % 11 == 0 and w // 11 == h):
            shutil.copyfile(dotz_path, bak_dotz)
            src_dotz_path = bak_dotz
        else:
            src_dotz_path = dotz_path

    if src_dotz_path and os.path.exists(src_dotz_path):
        im_dotz = Image.open(src_dotz_path).convert('RGBA')
        w, h = im_dotz.size
        w_dot = h  # Vanilla dot size is square (w_dot == h)
        arr_c = np.array(im_dotz)
        
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
        
        # Backup pristine original if not already backed up
        if not os.path.exists(bak_dotz) and not (w % 11 == 0 and w // 11 == h):
            shutil.copyfile(dotz_path, bak_dotz)
            
        out_im = Image.fromarray(dotz_new_rgba, mode='RGBA')
        out_gif = out_im.convert('P', palette=Image.ADAPTIVE, colors=256)
        out_gif.save(dotz_path, format='GIF')
        print(f"  [OK] baDotz.gif extended ({w_dot*6}x{h} -> {w_dot*11}x{h})")


def patch_resources_file(zuma_dir):
    p_res = os.path.join(zuma_dir, 'properties', 'resources.xml')
    if not os.path.isfile(p_res):
        print(f"[WARN] resources.xml not found at: {p_res}")
        return
        
    bak_res = get_backup_path(zuma_dir, 'resources.xml')
    migrate_legacy_backup(p_res + '.bak', bak_res)

    # Use clean vanilla backup as base source if available
    base_file = bak_res if os.path.exists(bak_res) else p_res
    with open(base_file, 'r', encoding='utf-8', errors='ignore') as f:
        orig_text = f.read()

    # If backup doesn't exist yet, save original vanilla before modifying
    if not os.path.exists(bak_res):
        with open(bak_res, 'w', encoding='utf-8') as f:
            f.write(orig_text)

    text = orig_text
    changed = False

    # 1. Update WHITE_BALL and insert new balls if not present
    if not ('ORANGE_BALL' in text and 'CREAM_BALL' in text):
        new_balls = (
            '<Image id="WHITE_BALL"\t\t\t\t\t\tpath="baBallWhite" alphagrid="_ballalpha" rows=50 />\n'
            '<Image id="ORANGE_BALL"\t\t\t\t\t\tpath="baBallOrange" alphagrid="_ballalpha" rows=47 />\n'
            '<Image id="NAVY_BALL"\t\t\t\t\t\tpath="baBallNavy" alphagrid="_ballalpha" rows=50 />\n'
            '<Image id="CYAN_BALL"\t\t\t\t\t\tpath="baBallCyan" alphagrid="_ballalpha" rows=50 />\n'
            '<Image id="BLACK_BALL"\t\t\t\t\t\tpath="baBallBlack" alphagrid="_ballalpha" rows=50 />\n'
            '<Image id="CREAM_BALL"\t\t\t\t\t\tpath="baBallCream" alphagrid="_ballalpha" rows=50 alias="BALL_END" />'
        )
        text = re.sub(r'<Image\s+id="WHITE_BALL"[^>]*alias="BALL_END"[^>]*/>', new_balls, text)
        text = re.sub(r'(<Image\s+id="NEXT_BALL"[^>]*cols=)6([^>]*/>)', r'\g<1>11\g<2>', text)
        
        # Insert powerups
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
            text = re.sub(pattern, '\n'.join(inserted), text)
        changed = True

    # 2. Register DIALOG_BUTTON_LOCKED under Register Resources
    if 'DIALOG_BUTTON_LOCKED' not in text:
        def insert_locked_button(m):
            return m.group(0) + '\n<Image id="DIALOG_BUTTON_LOCKED"\t\tpath="DialogButtonLocked" />'
        new_text = re.sub(r'<Image\s+id="DIALOG_BUTTON"\s+path="Dialogbutton"\s*/>', insert_locked_button, text)
        if new_text != text:
            text = new_text
            changed = True

    # 3. Register ADVTEMPLE5 and ADVDOOR5 under AdventureScreen
    if 'ADVTEMPLE5' in text:
        m_t5 = re.search(r'<Image\s+id="ADVTEMPLE5"[^>]*/>', text)
        if m_t5 and 'alphaimage=' not in m_t5.group(0):
            text = re.sub(r'(<Image\s+id="ADVTEMPLE5"[^/]*)(/>)', r'\g<1>alphaimage="_advtemple5" \g<2>', text)
            changed = True
    else:
        def insert_temple5(m):
            return m.group(0) + '\n<Image id="ADVTEMPLE5"\t\t\t\t\tpath="advtemple5" alphaimage="_advtemple5" />'
        new_text = re.sub(r'<Image\s+id="ADVTEMPLE4"[^>]*/>', insert_temple5, text)
        if new_text != text:
            text = new_text
            changed = True

    if 'ADVDOOR5' not in text:
        def insert_door5(m):
            return m.group(0) + '\n<Image id="ADVDOOR5"\t\t\t\t\tpath="advdoor5" cols="3" />'
        new_text = re.sub(r'<Image\s+id="ADVDOOR4C"[^>]*/>', insert_door5, text)
        if new_text != text:
            text = new_text
            changed = True

    # 4. Register HELP_FRONT_DEV, HELP_BACK_DEV, TUT_MOUSE_M, and TUT_TEMPLE5
    if 'HELP_FRONT_DEV' not in text:
        def insert_help_dev(m):
            return (
                m.group(0) + '\n'
                '<Image id="HELP_FRONT_DEV"\t\t\t\t\tpath="helpfront_dev" />\n'
                '<Image id="HELP_BACK_DEV"\t\t\t\t\tpath="helpback_dev" />\n'
                '<Image id="TUT_MOUSE_M"\t\t\t\t\t\tpath="tut_mouse_m" cols="2" />\n'
                '<Image id="TUT_TEMPLE5"\t\t\t\t\tpath="advtemple5_tut" />'
            )
        new_text = re.sub(r'<Image\s+id="HELP_BACK"[^>]*/>', insert_help_dev, text)
        if new_text != text:
            text = new_text
            changed = True

    # Clean up any obsolete TUT_TITLE and TUT_BALL entries from resources.xml
    text_cleaned = re.sub(r'[ \t]*<Image\s+id="TUT_(?:TITLE|BALL)_[^>]*/>\r?\n?', '', text)
    if text_cleaned != text:
        text = text_cleaned
        changed = True

    if 'cols=2' in text:
        text = text.replace('cols=2', 'cols="2"')
        changed = True

    with open(p_res, 'w', encoding='utf-8') as f:
        f.write(text)
    print("  [OK] resources.xml patched successfully.")


def patch_levels_file(zuma_dir):
    p_lvl = os.path.join(zuma_dir, 'levels', 'levels.xml')
    if not os.path.isfile(p_lvl):
        print(f"[WARN] levels.xml not found at: {p_lvl}")
        return
        
    bak_lvl = get_backup_path(zuma_dir, 'levels.xml')
    migrate_legacy_backup(p_lvl + '.bak', bak_lvl)

    # Use clean vanilla backup as base source if available
    base_lvl = bak_lvl if os.path.exists(bak_lvl) else p_lvl
    with open(base_lvl, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()

    # Backup if not yet backed up
    if not os.path.exists(bak_lvl):
        with open(bak_lvl, 'w', encoding='utf-8') as f:
            f.write(text)
            
    sp_match = re.search(r'<StageProgression\s+([^>]+)/>', text, re.DOTALL)
    if not sp_match:
        print("[WARN] <StageProgression /> block not found in levels.xml")
        return

    print(f"[XML] Patching levels.xml: {p_lvl}")
    sp_body = sp_match.group(1)
    target_colors = {}
    
    for s_num in range(1, 14):
        m = re.search(rf'diffi{s_num}\s*=\s*"([^"]+)"', sp_body)
        if m:
            sids = [s.strip() for s in m.group(1).split(',')]
            if 1 <= s_num <= 3:
                c_val = 4
            elif 4 <= s_num <= 6:
                c_val = 6
            elif 7 <= s_num <= 9:
                c_val = 8
            elif 10 <= s_num <= 12:
                c_val = 10
            elif s_num == 13:
                c_val = 11
            for sid in sids:
                target_colors[sid] = c_val

    def replace_setting(match):
        full_line = match.group(0)
        sid = match.group(1)
        if sid in target_colors:
            req_c = target_colors[sid]
            if re.search(r'colors="\d+"', full_line):
                return re.sub(r'colors="\d+"', f'colors="{req_c}"', full_line)
            else:
                return re.sub(r'\s*/>', f' colors="{req_c}"/>', full_line)
        return full_line

    new_text = re.sub(r'<Settings\s+id="([^"]+)"[^>]*/>', replace_setting, text)
    with open(p_lvl, 'w', encoding='utf-8') as f:
        f.write(new_text)
    print(f"  [OK] levels.xml patched successfully ({len(target_colors)} stage levels updated).")


def patch_dialog_button(zuma_dir, tables):
    img_dir = os.path.join(zuma_dir, 'images')
    if not os.path.isdir(img_dir):
        return

    jpg_path = os.path.join(img_dir, 'DialogButton.jpg')
    gif_path = os.path.join(img_dir, '_Dialogbutton.gif')
    locked_jpg = os.path.join(img_dir, 'DialogButtonLocked.jpg')
    locked_gif = os.path.join(img_dir, '_Dialogbuttonlocked.gif')

    # 1. Restore vanilla DialogButton.jpg and _Dialogbutton.gif if .bak exists
    bak_jpg = jpg_path + '.bak'
    if os.path.exists(bak_jpg):
        shutil.copyfile(bak_jpg, jpg_path)
        os.remove(bak_jpg)
        print("  [RESTORE] Restored vanilla DialogButton.jpg from backup.")

    bak_gif = gif_path + '.bak'
    if os.path.exists(bak_gif):
        shutil.copyfile(bak_gif, gif_path)
        os.remove(bak_gif)
        print("  [RESTORE] Restored vanilla _Dialogbutton.gif from backup.")

    # 2. Always generate DialogButtonLocked.jpg from DialogButton.jpg
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
                    g = lut[sk]
                else:
                    r, g_val, b = float(arr_s[y, x, 0]), float(arr_s[y, x, 1]), float(arr_s[y, x, 2])
                    g = int(np.clip(0.2982 * r + 0.7296 * g_val + 0.0104 * b + 2.6247, 0, 255))
                arr_out[y, x] = [g, g, g]

        out_im = Image.fromarray(arr_out, mode='RGB')
        out_im.save(locked_jpg, format='JPEG', quality=95)
        print(f"  [OK] DialogButtonLocked.jpg generated ({w}x{h}).")

    # 3. Always refresh _Dialogbuttonlocked.gif from _Dialogbutton.gif
    if os.path.exists(gif_path):
        shutil.copyfile(gif_path, locked_gif)
        print(f"  [OK] _Dialogbuttonlocked.gif created from {gif_path}.")


def patch_temple5_media(zuma_dir):
    src_media = os.path.join(SCRIPT_DIR, '..', 'assets', 'media')
    if not os.path.isdir(src_media):
        return
    img_dir = os.path.join(zuma_dir, 'images')
    if not os.path.isdir(img_dir):
        return

    print(f"[ASSET] Patching Secret Temple 5 & Door 5 media into: {img_dir}")

    # 1. advtemple5.jpg and _advtemple5.gif
    t5_jpg = os.path.join(src_media, 'advtemple5.jpg')
    t5_gif = os.path.join(src_media, '_advtemple5.gif')
    if os.path.exists(t5_jpg):
        im_t5 = Image.open(t5_jpg)
        dst_t5 = os.path.join(img_dir, 'advtemple5.jpg')
        if im_t5.size != (391, 100):
            im_t5 = im_t5.resize((391, 100), Image.Resampling.LANCZOS)
            im_t5.save(dst_t5, format='JPEG', quality=95)
            print("  [OK] Normalized and saved advtemple5.jpg (391x100) to images/")
        else:
            shutil.copyfile(t5_jpg, dst_t5)
            print("  [OK] Copied advtemple5.jpg (391x100) to images/")
    if os.path.exists(t5_gif):
        im_t5g = Image.open(t5_gif)
        dst_t5g = os.path.join(img_dir, '_advtemple5.gif')
        if im_t5g.size != (391, 100):
            im_t5g = im_t5g.resize((391, 100), Image.Resampling.NEAREST)
            im_t5g.save(dst_t5g, format='GIF')
            print("  [OK] Normalized and saved _advtemple5.gif (391x100) to images/")
        else:
            shutil.copyfile(t5_gif, dst_t5g)
            print("  [OK] Copied _advtemple5.gif (391x100) to images/")

    # 2. advdoor5.jpg (normalized to 192x50 with cols=3)
    d5_jpg = os.path.join(src_media, 'advdoor5.jpg')
    dst_d5 = os.path.join(img_dir, 'advdoor5.jpg')
    if os.path.exists(d5_jpg):
        im = Image.open(d5_jpg)
        if im.width != 192 or im.height != 50:
            im = im.crop((0, 0, 192, 50))
            im.save(dst_d5, format='JPEG', quality=95)
            print("  [OK] Normalized and saved advdoor5.jpg (192x50) to images/")
        else:
            shutil.copyfile(d5_jpg, dst_d5)
            print("  [OK] Copied advdoor5.jpg to images/")


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

    # 3. tut_mouse_m.jpg (40x29) & tut_mouse_m_.jpg (alpha mask from tut_mouse_r_.jpg)
    mm_jpg = os.path.join(src_media, 'tut_mouse_m.jpg')
    if os.path.exists(mm_jpg):
        shutil.copyfile(mm_jpg, os.path.join(img_dir, 'tut_mouse_m.jpg'))
        print("  [OK] Copied tut_mouse_m.jpg to images/")

    mr_mask = os.path.join(img_dir, 'tut_mouse_r_.jpg')
    dst_mm_mask = os.path.join(img_dir, 'tut_mouse_m_.jpg')
    if os.path.exists(mr_mask):
        shutil.copyfile(mr_mask, dst_mm_mask)
        print("  [OK] Created tut_mouse_m_.jpg from tut_mouse_r_.jpg")

    # 4. advtemple5_tut.jpg (190x75) preview of Stage 13 with radiant green glowing portal
    src_temple = os.path.join(img_dir, 'advtemple5.jpg')
    src_door = os.path.join(img_dir, 'advdoor5.jpg')
    dst_tut_temple = os.path.join(img_dir, 'advtemple5_tut.jpg')
    if os.path.exists(src_temple) and os.path.exists(src_door):
        from PIL import ImageFilter
        temple = Image.open(src_temple).convert('RGB')
        door = Image.open(src_door).convert('RGB')
        frame1 = door.crop((64, 0, 128, 50))  # Frame 1: glowing green eye
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
        print("  [OK] Generated and saved advtemple5_tut.jpg (190x75) with green glow to images/")

    # 5. Clean up obsolete tutorial title and ball files if present
    obsolete_files = [
        'tut_title_adv.gif', '_tut_title_adv.gif',
        'tut_title_gaunt.gif', '_tut_title_gaunt.gif',
        'tut_title_mmb.gif', '_tut_title_mmb.gif',
        'tut_ball_red.gif', 'tut_ball_yellow.gif',
        'tut_ball_green.gif', 'tut_ball_blue.gif',
        'tut_ball_purple.gif'
    ]
    for obs in obsolete_files:
        for folder in (img_dir, os.path.join(zuma_dir, 'images800x600')):
            fp = os.path.join(folder, obs)
            if os.path.exists(fp):
                try:
                    os.remove(fp)
                    print(f"  [CLEANUP] Removed obsolete {obs} from {os.path.basename(folder)}/")
                except Exception:
                    pass



if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: patch_assets.py <zuma_directory>")
        sys.exit(1)
        
    z_dir = os.path.abspath(sys.argv[1])
    if not os.path.isdir(z_dir):
        print(f"[ERROR] Directory does not exist: {z_dir}")
        sys.exit(1)
        
    tbls = load_tables()
    patch_resources_file(z_dir)
    patch_levels_file(z_dir)
    patch_images(z_dir, tbls)
    patch_dialog_button(z_dir, tbls)
    patch_temple5_media(z_dir)
    patch_dev_media(z_dir)
    print("[DONE] Assets and XML patched successfully.")

