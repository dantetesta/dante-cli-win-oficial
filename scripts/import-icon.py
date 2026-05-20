#!/usr/bin/env python3
"""Import a PNG mascot into resources/icons/app.ico (multi-res Windows icon)."""

import sys
from pathlib import Path
from PIL import Image


def main() -> None:
    src = Path(sys.argv[1]) if len(sys.argv) > 1 else None
    if not src or not src.exists():
        print(f"usage: {sys.argv[0]} <source.png>")
        sys.exit(1)

    img = Image.open(src).convert("RGBA")
    w, h = img.size
    side = min(w, h)
    left = (w - side) // 2
    top = (h - side) // 2
    img = img.crop((left, top, left + side, top + side))

    sizes = [16, 24, 32, 48, 64, 96, 128, 192, 256]
    layers = [img.resize((s, s), Image.LANCZOS) for s in sizes]

    out_dir = Path(__file__).resolve().parent.parent / "resources" / "icons"
    out_dir.mkdir(parents=True, exist_ok=True)
    ico_path = out_dir / "app.ico"
    layers[-1].save(ico_path,
                    format="ICO",
                    sizes=[(s, s) for s in sizes],
                    append_images=layers[:-1])
    print(f"wrote {ico_path} ({ico_path.stat().st_size} bytes)")

    # Also keep a high-res PNG mirror for installer assets
    png_path = out_dir / "app-source.png"
    layers[-1].save(png_path, format="PNG")
    print(f"wrote {png_path}")


if __name__ == "__main__":
    main()
