import sys
from pathlib import Path

def patch_pth(pth_path_str, root_dir_str=None):
    p = Path(pth_path_str)
    if not p.is_file():
        print(f"[WARN] ._pth file not found: {p}")
        return

    text = p.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    new_lines = []
    has_site = False

    for line in lines:
        stripped = line.strip()
        if stripped in ("#import site", "# import site"):
            new_lines.append("import site")
            has_site = True
        elif stripped == "import site":
            new_lines.append("import site")
            has_site = True
        else:
            new_lines.append(line)

    if not has_site:
        new_lines.append("import site")

    norm = [l.strip().replace("\\", "/") for l in new_lines]

    def add_entry(entry):
        entry_norm = entry.strip().replace("\\", "/")
        if entry_norm not in norm:
            new_lines.append(entry)
            norm.append(entry_norm)

    add_entry(".")
    add_entry("Lib/site-packages")

    if root_dir_str:
        root_path = Path(root_dir_str).resolve()
        add_entry(str(root_path))
        add_entry(str(root_path / "builder"))

    p.write_text("\n".join(new_lines) + "\n", encoding="utf-8")
    print(f"[OK] Patched {p.name}: site-packages enabled, paths injected.")

if __name__ == "__main__":
    if len(sys.argv) > 2:
        patch_pth(sys.argv[1], sys.argv[2])
    elif len(sys.argv) > 1:
        patch_pth(sys.argv[1])
    else:
        print("Usage: prepare_embed.py <path-to-pythonXX._pth> [project-root]")
