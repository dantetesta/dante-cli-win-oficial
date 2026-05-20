#!/usr/bin/env python3
"""Generate Inno Setup wizard BMP images (large + small)."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def find_font(size: int) -> ImageFont.FreeTypeFont:
    candidates = [
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/SFNSDisplay.ttf",
        "/Library/Fonts/Arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
    ]
    for c in candidates:
        if Path(c).exists():
            try:
                return ImageFont.truetype(c, size)
            except OSError:
                continue
    return ImageFont.load_default()


def _load_mascot() -> Image.Image | None:
    path = Path(__file__).resolve().parent.parent / "resources" / "icons" / "app-source.png"
    if path.exists():
        return Image.open(path).convert("RGBA")
    return None


def large_image() -> Image.Image:
    # 164 x 314 px is the Inno Setup default
    w, h = 164, 314
    img = Image.new("RGB", (w, h), (10, 18, 8))
    draw = ImageDraw.Draw(img)

    # Vertical gradient: deep green → black
    for y in range(h):
        ratio = y / (h - 1)
        r = int(10 + (18 - 10) * ratio)
        g = int(18 + (54 - 18) * (1 - ratio))
        b = int(8  + (12 -  8) * ratio)
        draw.line([(0, y), (w, y)], fill=(r, g, b))

    # Mascot
    mascot = _load_mascot()
    if mascot is not None:
        m = mascot.resize((148, 148), Image.LANCZOS)
        img.paste(m, ((w - 148) // 2, 26), m)

    # Brand text
    fb = find_font(22)
    text_b = "Dante CLI"
    bx0, by0, bx1, by1 = draw.textbbox((0, 0), text_b, font=fb)
    draw.text(((w - (bx1 - bx0)) // 2, 190), text_b, font=fb, fill=(230, 247, 232))

    fs = find_font(11)
    sub1 = "Terminal nativo"
    sub2 = "Windows"
    sub3 = "CODE. SECURE. REPEAT."
    for line, y, color in [
        (sub1, 230, (128, 160, 136)),
        (sub2, 246, (128, 160, 136)),
        (sub3, 280, (61, 214, 104)),
    ]:
        tx0, ty0, tx1, ty1 = draw.textbbox((0, 0), line, font=fs)
        draw.text(((w - (tx1 - tx0)) // 2, y), line, font=fs, fill=color)

    return img


def small_image() -> Image.Image:
    # 55 x 58 px
    w, h = 55, 58
    img = Image.new("RGB", (w, h), (10, 18, 8))
    mascot = _load_mascot()
    if mascot is not None:
        m = mascot.resize((52, 52), Image.LANCZOS)
        img.paste(m.convert("RGB"), ((w - 52) // 2, 3))
    else:
        draw = ImageDraw.Draw(img)
        f = find_font(28)
        draw.text((6, 14), ">_", font=f, fill=(61, 214, 104))
    return img


def main() -> None:
    out_dir = Path(__file__).resolve().parent.parent / "installer" / "assets"
    out_dir.mkdir(parents=True, exist_ok=True)
    large_image().save(out_dir / "wizard-large.bmp", format="BMP")
    small_image().save(out_dir / "wizard-small.bmp", format="BMP")
    print(f"wrote {out_dir / 'wizard-large.bmp'}")
    print(f"wrote {out_dir / 'wizard-small.bmp'}")


if __name__ == "__main__":
    main()
