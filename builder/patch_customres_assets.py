import os, re
from PIL import Image

def generate_resized_images(w, h, game_dir, out_dir=None):
    if not out_dir:
        out_dir = game_dir
    xml_path = os.path.join(game_dir, 'properties', 'resources.xml')
    img_dir = os.path.join(game_dir, 'images')
    target_img_dir = os.path.join(out_dir, 'imagescustomres')
    os.makedirs(target_img_dir, exist_ok=True)

    with open(xml_path, 'r', encoding='utf-8', errors='ignore') as f:
        text = f.read()

    w = max(w, 480)
    h = max(h, 320)
    scale = min(w / 640.0, h / 480.0)
    print(f"[*] Generating assets for {w}x{h} (Scale = {scale:.4f}) into {target_img_dir}")

    target_groups = ['Init', 'MainMenu', 'AdventureScreen', 'GauntletScreen', 'HelpScreen', 'MoreGames']

    # Build case-insensitive map of files in images directory
    all_files_map = {f.lower(): f for f in os.listdir(img_dir)}

    # Items to process: (rel_name, full_path, cols, rows)
    items = []
    res_blocks = re.findall(r'<Resources\s+id=[\"\'](.*?)[\"\']>(.*?)</Resources>', text, re.S)
    for gid, body in res_blocks:
        if gid in target_groups:
            img_tags = re.findall(r'<Image\s+([^>]+)/>', body)
            for attrs in img_tags:
                id_m = re.search(r'id=[\"\'](.*?)[\"\']', attrs)
                path_m = re.search(r'path=[\"\'](.*?)[\"\']', attrs)
                alpha_m = re.search(r'(?:alphaimage|alphagrid)=[\"\'](.*?)[\"\']', attrs)
                cols_m = re.search(r'cols=[\"\']?(\d+)[\"\']?', attrs)
                rows_m = re.search(r'rows=[\"\']?(\d+)[\"\']?', attrs)
                path = path_m.group(1) if path_m else ''
                alpha = alpha_m.group(1) if alpha_m else ''
                cols = int(cols_m.group(1)) if cols_m else 1
                rows = int(rows_m.group(1)) if rows_m else 1

                for name in [path, alpha]:
                    if not name: continue
                    # Gameplay assets, cursors, and tutorial detail assets must never be resized:
                    lower_name = name.lower()
                    if 'ball' in lower_name or 'tut_title' in lower_name or 'cursor' in lower_name or 'tut_mouse' in lower_name or 'advtemple5_tut' in lower_name or lower_name in (
                        '_ballalpha', 'ballalpha', '_ballshadow', 'smallfrogonpad', 'hatchback',
                        'tongue', 'eyeblink', 'hole', 'pitcover', 'coin', 'gauge', 'gaugegreen', 'toadlives'
                    ):
                        continue
                    
                    # 1. Base image
                    for ext in ['.jpg', '.gif', '.png']:
                        cand = (name + ext).lower()
                        if cand in all_files_map:
                            real_name = all_files_map[cand]
                            items.append((real_name, os.path.join(img_dir, real_name), cols, rows))
                            break

                    # 2. Check for underscore alpha mask: _<name>.(gif|jpg|png) or <name>_.(gif|jpg|png)
                    for ext in ['.gif', '.png', '.jpg']:
                        cand_mask = ('_' + name + ext).lower()
                        if cand_mask in all_files_map:
                            real_mask = all_files_map[cand_mask]
                            items.append((real_mask, os.path.join(img_dir, real_mask), cols, rows))
                            break
                        cand_mask2 = (name + '_' + ext).lower()
                        if cand_mask2 in all_files_map:
                            real_mask = all_files_map[cand_mask2]
                            items.append((real_mask, os.path.join(img_dir, real_mask), cols, rows))
                            break

    # Add standalone fullscreen backgrounds and frames (mmsunglow, happyending, nebula1)
    extra_bases = [
        'mmsunglow', '_mmsunglow',
        'happyending', '_happyending',
        'nebula1'
    ]
    for base in extra_bases:
        for ext in ['.jpg', '.gif', '.png']:
            cand = (base + ext).lower()
            if cand in all_files_map:
                real_name = all_files_map[cand]
                items.append((real_name, os.path.join(img_dir, real_name), 1, 1))
                break

    # Remove duplicates preserving order
    seen = set()
    unique_items = []
    for item in items:
        if item[0].lower() not in seen:
            seen.add(item[0].lower())
            unique_items.append(item)

    print(f"[*] Found {len(unique_items)} unique menu image and mask assets to scale.")

    for rel_name, full_path, cols, rows in unique_items:
        out_path = os.path.join(target_img_dir, rel_name)
        try:
            im = Image.open(full_path)
            orig_w, orig_h = im.size

            # Adjust cols for known button frames if not specified in xml
            c = cols
            r = rows
            if c == 1:
                lower = rel_name.lower()
                if 'advdoor5' in lower:
                    c = 3
                elif 'door' in lower:
                    c = 4
                elif 'moregamesbutton' in lower:
                    c = 6
                elif 'button' in lower:
                    if orig_w % 4 == 0 and orig_w % 3 != 0:
                        c = 4
                    elif orig_w % 3 == 0:
                        c = 3

            # Calculate frame dimensions with exact integer scaling per frame
            frame_w = orig_w / c
            frame_h = orig_h / r
            new_frame_w = max(1, round(frame_w * scale))
            new_frame_h = max(1, round(frame_h * scale))
            new_w = new_frame_w * c
            new_h = new_frame_h * r

            # Nebula scrolling background: width strictly matches screen width X, height ratio-locked to X
            if 'nebula1' in rel_name.lower():
                new_w = w
                new_h = max(1, round(orig_h * (float(w) / float(orig_w))))

            # Resample
            is_mask = os.path.basename(rel_name).startswith('_')
            if is_mask:
                # Alpha mask: grayscale intensity determines transparency
                im_gray = im.convert('L')
                res = im_gray.resize((new_w, new_h), Image.Resampling.LANCZOS)
                if rel_name.lower().endswith('.gif'):
                    res_p = res.convert('P', palette=Image.Palette.ADAPTIVE, colors=256)
                    res_p.save(out_path)
                elif rel_name.lower().endswith('.jpg') or rel_name.lower().endswith('.jpeg'):
                    res.convert('RGB').save(out_path, quality=95)
                else:
                    res.save(out_path)
            elif im.mode == 'P':
                # Paletted color image: convert to RGBA for smooth Lanczos scaling, then quantize back
                im_rgba = im.convert('RGBA')
                res = im_rgba.resize((new_w, new_h), Image.Resampling.LANCZOS)
                res_p = res.convert('P', palette=Image.Palette.ADAPTIVE, colors=256)
                res_p.save(out_path)
            elif im.mode == 'L':
                res = im.resize((new_w, new_h), Image.Resampling.LANCZOS)
                res.save(out_path)
            elif im.mode == 'RGBA':
                res = im.resize((new_w, new_h), Image.Resampling.LANCZOS)
                res.save(out_path)
            else: # RGB / JPEG
                res = im.convert('RGB').resize((new_w, new_h), Image.Resampling.LANCZOS)
                res.save(out_path, quality=95)

        except Exception as e:
            print(f"[!] Error resizing {rel_name}: {e}")

    print(f"[+] Successfully scaled and saved {len(unique_items)} images to {target_img_dir}")

    # Procedurally generate seamless menubar and mask
    try:
        try:
            from builder.patch_menubar import generate_menubar, save_custom_menubar, ensure_master_assets
        except ImportError:
            from patch_menubar import generate_menubar, save_custom_menubar, ensure_master_assets
        import numpy as np

        v_mb_jpg = None
        for cand in ['menubar.jpg', 'Menubar.jpg']:
            p = os.path.join(img_dir, cand)
            if os.path.exists(p):
                v_mb_jpg = p
                break
        v_mb_gif = None
        for cand in ['_menubar.gif', '_Menubar.gif']:
            p = os.path.join(img_dir, cand)
            if os.path.exists(p):
                v_mb_gif = p
                break

        if v_mb_jpg and v_mb_gif:
            assets_media = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'assets', 'media')
            ensure_master_assets(assets_media)
            v_rgb = np.array(Image.open(v_mb_jpg).convert('RGB'))
            v_mask = np.array(Image.open(v_mb_gif).convert('L'))
            out_rgb, out_mask = generate_menubar(v_rgb, v_mask, w, h, assets_media)
            save_custom_menubar(out_rgb, out_mask,
                                os.path.join(target_img_dir, 'menubar.jpg'),
                                os.path.join(target_img_dir, '_menubar.gif'))
            print(f"[+] Procedurally reconstructed seamless menubar ({w}x{h}) into {target_img_dir}")
        else:
            print(f"[!] Warning: Menubar reference files not found in {img_dir}")
    except Exception as e:
        print(f"[!] Error generating menubar: {e}")

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(description="ZuCompleluxe Custom Resolution Asset Resizer")
    parser.add_argument("game_dir", help="Path to Zuma Deluxe game directory")
    parser.add_argument("--out-dir", default=None, help="Target output directory (defaults to game_dir)")
    parser.add_argument("--width", "-W", type=int, default=800, help="Target resolution width")
    parser.add_argument("--height", "-H", type=int, default=600, help="Target resolution height")
    args = parser.parse_args()

    generate_resized_images(args.width, args.height, args.game_dir, args.out_dir)
