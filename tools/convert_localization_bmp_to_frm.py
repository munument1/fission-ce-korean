#!/usr/bin/env python3
import argparse
import pathlib
import struct


FRM_HEADER_SIZE = 74


def read_act_palette(path):
    data = pathlib.Path(path).read_bytes()
    if len(data) < 768:
        raise ValueError(f"Palette is too small: {path}")
    return [(data[i * 3], data[i * 3 + 1], data[i * 3 + 2]) for i in range(256)]


def nearest_palette_index(rgb, palette, exact):
    found = exact.get(rgb)
    if found is not None:
        return found, True

    r, g, b = rgb
    best_index = 0
    best_distance = 1 << 62
    for i, (pr, pg, pb) in enumerate(palette):
        dr = r - pr
        dg = g - pg
        db = b - pb
        distance = dr * dr + dg * dg + db * db
        if distance < best_distance:
            best_distance = distance
            best_index = i
            if distance == 0:
                break
    return best_index, False


def read_bmp_as_palette_indexes(path, palette):
    data = pathlib.Path(path).read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"Not a BMP file: {path}")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]

    if dib_size == 12:
        width, height, planes, bpp = struct.unpack_from("<HHHH", data, 18)
        compression = 0
        signed_height = height
        palette_offset = 14 + dib_size
        color_entry_size = 3
    elif dib_size >= 40:
        width, signed_height, planes, bpp, compression = struct.unpack_from("<iiHHI", data, 18)
        palette_offset = 14 + dib_size
        color_entry_size = 4
    else:
        raise ValueError(f"Unsupported BMP DIB header size {dib_size}: {path}")

    if planes != 1:
        raise ValueError(f"Unsupported BMP plane count {planes}: {path}")
    if compression != 0:
        raise ValueError(f"Compressed BMP is not supported: {path}")

    height_abs = abs(signed_height)
    row_stride = ((width * bpp + 31) // 32) * 4
    top_down = signed_height < 0

    exact = {}
    for i, rgb in enumerate(palette):
        exact.setdefault(rgb, i)

    out = bytearray(width * height_abs)
    exact_count = 0
    nearest_count = 0

    if bpp == 8:
        for y in range(height_abs):
            source_y = y if top_down else height_abs - 1 - y
            source = pixel_offset + source_y * row_stride
            target = y * width
            out[target : target + width] = data[source : source + width]
        exact_count = width * height_abs
    elif bpp in (24, 32):
        bytes_per_pixel = bpp // 8
        for y in range(height_abs):
            source_y = y if top_down else height_abs - 1 - y
            row = pixel_offset + source_y * row_stride
            for x in range(width):
                p = row + x * bytes_per_pixel
                b, g, r = data[p], data[p + 1], data[p + 2]
                index, is_exact = nearest_palette_index((r, g, b), palette, exact)
                out[y * width + x] = index
                if is_exact:
                    exact_count += 1
                else:
                    nearest_count += 1
    else:
        raise ValueError(f"Unsupported BMP bit depth {bpp}: {path}")

    return width, height_abs, bytes(out), exact_count, nearest_count


def find_template(template_dir, stem):
    wanted = f"{stem}.frm".lower()
    for candidate in pathlib.Path(template_dir).iterdir():
        if candidate.is_file() and candidate.name.lower() == wanted:
            return candidate
    return None


def convert_all(source_dir, template_dir, output_dir, palette_path):
    source_dir = pathlib.Path(source_dir)
    template_dir = pathlib.Path(template_dir)
    output_dir = pathlib.Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    palette = read_act_palette(palette_path)

    converted = []
    skipped = []

    for bmp in sorted(source_dir.glob("*.bmp"), key=lambda p: p.name.lower()):
        template = find_template(template_dir, bmp.stem)
        if template is None:
            skipped.append((bmp.name, "no matching FRM template"))
            continue

        width, height, pixels, exact_count, nearest_count = read_bmp_as_palette_indexes(bmp, palette)
        template_bytes = template.read_bytes()
        expected_size = FRM_HEADER_SIZE + width * height
        if len(template_bytes) != expected_size:
            skipped.append((bmp.name, f"template size mismatch: {template.name} is {len(template_bytes)}, expected {expected_size}"))
            continue

        output_path = output_dir / template.name
        output_path.write_bytes(template_bytes[:FRM_HEADER_SIZE] + pixels)
        converted.append((bmp.name, output_path.name, width, height, exact_count, nearest_count))

    return converted, skipped


def main():
    parser = argparse.ArgumentParser(description="Convert F.I.S.S.I.O.N. localization BMP files to Fallout FRM files.")
    parser.add_argument("--source", default="files/localization_ko")
    parser.add_argument("--templates", default="files/fission/art/intrface")
    parser.add_argument("--output", default=r"D:\Translation\Fission-CE-KR-art-intrface")
    parser.add_argument("--palette", default="files/localization_ko/Fallout.act")
    args = parser.parse_args()

    converted, skipped = convert_all(args.source, args.templates, args.output, args.palette)

    for bmp_name, frm_name, width, height, exact_count, nearest_count in converted:
        print(f"converted {bmp_name} -> {frm_name} ({width}x{height}, exact={exact_count}, nearest={nearest_count})")

    for bmp_name, reason in skipped:
        print(f"skipped {bmp_name}: {reason}")

    print(f"Converted: {len(converted)}")
    print(f"Skipped: {len(skipped)}")


if __name__ == "__main__":
    main()
