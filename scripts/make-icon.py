#!/usr/bin/env python3
"""Generate app.ico from a procedural design when SVG renderers aren't available.

Produces a multi-resolution Windows ICO (16, 32, 48, 64, 128, 256 px) that
matches the SVG design: dark rounded background, gradient accent rectangle,
prompt glyph ">_" and a thin accent bar.
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont


def find_mono_font(size: int) -> ImageFont.FreeTypeFont:
    candidates = [
        "/System/Library/Fonts/Menlo.ttc",
        "/System/Library/Fonts/Monaco.ttf",
        "/System/Library/Fonts/Supplemental/Andale Mono.ttf",
        "/Library/Fonts/Cascadia Code.ttf",
        "/Library/Fonts/CascadiaCode.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
        "C:/Windows/Fonts/CascadiaCode.ttf",
        "C:/Windows/Fonts/consolab.ttf",
    ]
    for c in candidates:
        if Path(c).exists():
            try:
                return ImageFont.truetype(c, size)
            except OSError:
                continue
    return ImageFont.load_default()


def render(size: int) -> Image.Image:
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    radius = max(2, int(size * 0.22))
    bg = (26, 27, 38, 255)  # #1A1B26
    draw.rounded_rectangle((0, 0, size - 1, size - 1), radius=radius, fill=bg)

    # Soft gradient accent rectangle (Tokyo Night feel)
    inset = max(2, int(size * 0.08))
    accent_radius = max(2, int(size * 0.18))
    accent = Image.new("RGBA", (size - 2 * inset, size - 2 * inset), (0, 0, 0, 0))
    adraw = ImageDraw.Draw(accent)
    grad = Image.new("RGBA", accent.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    w, h = grad.size
    for y in range(h):
        ratio = y / max(1, h - 1)
        r = int(122 + (187 - 122) * ratio)
        g = int(162 + (154 - 162) * ratio)
        b = int(247 + (247 - 247) * ratio)
        gd.line([(0, y), (w, y)], fill=(r, g, b, 38))
    accent.paste(grad, (0, 0))
    mask = Image.new("L", accent.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w - 1, h - 1),
                                            radius=accent_radius, fill=255)
    accent.putalpha(mask)
    img.alpha_composite(accent, (inset, inset))

    # Prompt glyph ">_"
    font = find_mono_font(int(size * 0.52))
    text = ">_"
    tx, ty, bx, by = draw.textbbox((0, 0), text, font=font)
    text_w, text_h = bx - tx, by - ty
    pos = ((size - text_w) // 2 - tx, (size - text_h) // 2 - ty - int(size * 0.04))
    draw.text(pos, text, font=font, fill=(192, 202, 245, 255))  # #C0CAF5

    # Accent underline bar
    bar_h = max(1, int(size * 0.04))
    bar_w = int(size * 0.58)
    bar_x = (size - bar_w) // 2
    bar_y = int(size * 0.78)
    draw.rounded_rectangle(
        (bar_x, bar_y, bar_x + bar_w, bar_y + bar_h),
        radius=bar_h // 2,
        fill=(122, 162, 247, 255),
    )

    return img


def main() -> None:
    sizes = [16, 32, 48, 64, 128, 256]
    layers = [render(s) for s in sizes]
    out = Path(__file__).resolve().parent.parent / "resources" / "icons" / "app.ico"
    out.parent.mkdir(parents=True, exist_ok=True)
    layers[-1].save(out, format="ICO", sizes=[(s, s) for s in sizes],
                    append_images=layers[:-1])
    print(f"wrote {out} ({out.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
