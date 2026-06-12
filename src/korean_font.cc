#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

#include "korean_font.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <windows.h>

#include "color.h"
#include "db.h"
#include "settings.h"
#include "text_font.h"

namespace fallout {

struct FontConfig {
    const char* filename;
    float render_size;      // Vertical size for scaling
    float width_scale;      // Horizontal scale multiplier (e.g. 0.65f for compression)
    int line_height;        // Height returned to the layout engine
    int baseline_offset;    // Vertical baseline shift
    bool antialiased;
};

static FontConfig getKoreanFontConfig(int font) {
    switch (font) {
        // Small fonts (D2Coding)
        case 0:
        case 100:
            return { "D2Coding.ttf", 16.0f, 1.0f, 16, 0, true };

        // Normal/dialogue/interface (D2Coding)
        case 1:
        case 101:
            return { "D2Coding.ttf", 10.0f, 1.0f, 10, 0, true };

        // Large fonts (AtoZ8ExtraBold)
        case 2:
        case 102:
            return { "AtoZ8ExtraBold.ttf", 17.0f, 1.0f, 17, 0, true };

        // Bold/medium fonts (AtoZ8ExtraBold)
        case 3:
        case 103:
            return { "AtoZ8ExtraBold.ttf", 13.0f, 1.0f, 13, 0, true };

        // Title/Header fonts (AtoZ8ExtraBold)
        case 104:
            return { "AtoZ8ExtraBold.ttf", 22.0f, 1.0f, 22, 0, true };

        default:
            if (font >= 100) {
                return { "AtoZ8ExtraBold.ttf", 13.0f, 1.0f, 13, 0, true };
            }
            return { "D2Coding.ttf", 10.0f, 1.0f, 10, 0, true };
    }
}

struct LoadedFont {
    std::vector<unsigned char> data;
    stbtt_fontinfo info;
};

static std::unordered_map<std::string, std::unique_ptr<LoadedFont>> gLoadedFonts;

static stbtt_fontinfo* getFontInfo(const std::string& filename) {
    auto it = gLoadedFonts.find(filename);
    if (it != gLoadedFonts.end()) {
        return &it->second->info;
    }

    std::string path = "fonts/korean/" + filename;
    File* f = fileOpen(path.c_str(), "rb");
    if (!f) {
        path = "data/fonts/korean/" + filename;
        f = fileOpen(path.c_str(), "rb");
    }
    if (!f) return nullptr;

    int size = fileGetSize(f);
    auto lf = std::make_unique<LoadedFont>();
    lf->data.resize(size);
    fileRead(lf->data.data(), 1, size, f);
    fileClose(f);

    if (stbtt_InitFont(&lf->info, lf->data.data(), stbtt_GetFontOffsetForIndex(lf->data.data(), 0))) {
        auto* infoPtr = &lf->info;
        gLoadedFonts[filename] = std::move(lf);
        return infoPtr;
    }
    return nullptr;
}

struct CachedGlyph {
    int width;
    int height;
    int xoff;
    int yoff;
    int advance;
    std::vector<unsigned char> bitmap;
};

struct GlyphCacheKey {
    std::string filename;
    int size_key;
    int width_scale_key;
    int codepoint;

    bool operator==(const GlyphCacheKey& o) const {
        return filename == o.filename && size_key == o.size_key && width_scale_key == o.width_scale_key && codepoint == o.codepoint;
    }
};

struct GlyphCacheKeyHash {
    size_t operator()(const GlyphCacheKey& k) const {
        size_t h1 = std::hash<std::string>{}(k.filename);
        size_t h2 = std::hash<int>{}(k.size_key);
        size_t h3 = std::hash<int>{}(k.width_scale_key);
        size_t h4 = std::hash<int>{}(k.codepoint);
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
    }
};

static std::unordered_map<GlyphCacheKey, CachedGlyph, GlyphCacheKeyHash> gGlyphCache;

static const CachedGlyph* getGlyph(const std::string& filename, float render_size, float width_scale, int codepoint) {
    int size_key = (int)(render_size * 10.0f);
    int width_scale_key = (int)(width_scale * 100.0f);
    GlyphCacheKey key{ filename, size_key, width_scale_key, codepoint };
    auto it = gGlyphCache.find(key);
    if (it != gGlyphCache.end()) {
        return &it->second;
    }

    auto* info = getFontInfo(filename);
    if (!info) return nullptr;

    float scale_y = stbtt_ScaleForPixelHeight(info, render_size);
    float scale_x = scale_y * width_scale;

    int advance, lsb;
    stbtt_GetCodepointHMetrics(info, codepoint, &advance, &lsb);

    // Get the exact target bounding box at normal scale
    int target_xoff = 0, target_yoff = 0, target_x2 = 0, target_y2 = 0;
    stbtt_GetCodepointBitmapBox(info, codepoint, scale_x, scale_y, &target_xoff, &target_yoff, &target_x2, &target_y2);
    int target_width = target_x2 - target_xoff;
    int target_height = target_y2 - target_yoff;

    CachedGlyph cg;
    cg.width = target_width;
    cg.height = target_height;
    cg.xoff = target_xoff;
    cg.yoff = target_yoff;
    cg.advance = (int)(advance * scale_x + 0.5f);

    if (target_width > 0 && target_height > 0) {
        cg.bitmap.resize(target_width * target_height, 0);

        // Render at 3x resolution (SSAA)
        const int ssaa_factor = 3;
        float hr_scale_y = stbtt_ScaleForPixelHeight(info, render_size * (float)ssaa_factor);
        float hr_scale_x = hr_scale_y * width_scale;

        int hr_width = 0, hr_height = 0, hr_xoff = 0, hr_yoff = 0;
        unsigned char* bmp = stbtt_GetCodepointBitmap(info, hr_scale_x, hr_scale_y, codepoint, &hr_width, &hr_height, &hr_xoff, &hr_yoff);

        if (bmp && hr_width > 0 && hr_height > 0) {
            // Downsample using box filter
            for (int ty = 0; ty < target_height; ty++) {
                int y1 = ssaa_factor * (target_yoff + ty) - hr_yoff;
                for (int tx = 0; tx < target_width; tx++) {
                    int x1 = ssaa_factor * (target_xoff + tx) - hr_xoff;

                    int sum = 0;
                    int count = 0;
                    for (int dy = 0; dy < ssaa_factor; dy++) {
                        int sy = y1 + dy;
                        if (sy >= 0 && sy < hr_height) {
                            for (int dx = 0; dx < ssaa_factor; dx++) {
                                int sx = x1 + dx;
                                if (sx >= 0 && sx < hr_width) {
                                    sum += bmp[sy * hr_width + sx];
                                    count++;
                                }
                            }
                        }
                    }
                    if (count > 0) {
                        cg.bitmap[ty * target_width + tx] = (unsigned char)(sum / count);
                    }
                }
            }
            stbtt_FreeBitmap(bmp, nullptr);
        }
    }

    gGlyphCache[key] = std::move(cg);
    return &gGlyphCache[key];
}

static std::vector<int> decodeEucKr(const char* str) {
    std::vector<int> codepoints;
    if (!str) return codepoints;

    const unsigned char* p = (const unsigned char*)str;
    while (*p) {
        unsigned char c = *p;
        if (c == 0x95) {
            codepoints.push_back(0x2022); // Map custom bullet point symbol (knob)
            p++;
        } else if (c >= 0x81 && c <= 0xFE && *(p + 1)) {
            char src[2] = { (char)c, (char)*(p + 1) };
            wchar_t dst[1] = { 0 };
            if (MultiByteToWideChar(949, 0, src, 2, dst, 1) > 0) {
                codepoints.push_back(dst[0]);
            } else {
                codepoints.push_back((c << 8) | *(p + 1));
            }
            p += 2;
        } else {
            codepoints.push_back(c);
            p++;
        }
    }
    return codepoints;
}

static int cp949ToUnicodeChar(int ch) {
    if (ch == 0x95) return 0x2022;
    if (ch < 128) return ch;
    if (ch > 255) {
        char src[2] = { (char)(ch >> 8), (char)(ch & 0xFF) };
        wchar_t dst[1] = { 0 };
        if (MultiByteToWideChar(949, 0, src, 2, dst, 1) > 0) {
            return dst[0];
        }
    }
    return ch;
}

static void koreanFontDrawText(unsigned char* buf, const char* string, int length, int pitch, int color) {
    if ((color & FONT_SHADOW) != 0) {
        color &= ~FONT_SHADOW;
        koreanFontDrawText(buf + pitch + 1, string, length, pitch, (color & ~0xFF) | _colorTable[0]);
    }

    FontConfig cfg = getKoreanFontConfig(gCurrentFont);
    auto* info = getFontInfo(cfg.filename);
    if (!info) return;

    float scale_y = stbtt_ScaleForPixelHeight(info, cfg.render_size);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);
    int baseline = (int)(ascent * scale_y + 0.5f) + cfg.baseline_offset;

    unsigned char* blendTable = nullptr;
    if (cfg.antialiased) {
        blendTable = _getColorBlendTable(color & 0xFF);
    }

    std::vector<int> codepoints = decodeEucKr(string);
    unsigned char* ptr = buf;
    int current_width = 0;

    for (int cp : codepoints) {
        const auto* cg = getGlyph(cfg.filename, cfg.render_size, cfg.width_scale, cp);
        if (!cg) continue;

        int charWidth = cg->advance;
        if (current_width + charWidth > length) {
            break;
        }

        if (!cg->bitmap.empty()) {
            int y_start = baseline + cg->yoff;
            for (int y = 0; y < cg->height; y++) {
                int screen_y = y_start + y;
                if (screen_y < 0 || screen_y >= (int)fontGetLineHeight()) continue;

                unsigned char* dest_row = ptr + screen_y * pitch;
                const unsigned char* src_row = cg->bitmap.data() + y * cg->width;

                for (int x = 0; x < cg->width; x++) {
                    int screen_x = cg->xoff + x;
                    if (screen_x < 0 || current_width + screen_x >= length) continue;

                    unsigned char alpha = src_row[x];
                    if (alpha > 0) {
                        if (!blendTable) {
                            if (alpha > 127) {
                                dest_row[screen_x] = color & 0xFF;
                            }
                        } else {
                            unsigned char alpha8;
                            if (cfg.render_size <= 11.0f) {
                                if (alpha >= 120) {
                                    alpha8 = 7;
                                } else if (alpha >= 60) {
                                    alpha8 = 5;
                                } else if (alpha >= 20) {
                                    alpha8 = 3;
                                } else {
                                    alpha8 = 0;
                                }
                            } else {
                                alpha8 = alpha >> 5;
                            }

                            if (alpha8 == 7) {
                                dest_row[screen_x] = color & 0xFF;
                            } else if (alpha8 > 0) {
                                dest_row[screen_x] = blendTable[(alpha8 << 8) + dest_row[screen_x]];
                            }
                        }
                    }
                }
            }
        }

        ptr += charWidth;
        current_width += charWidth;
    }

    if (blendTable) {
        _freeColorBlendTable(color & 0xFF);
    }
}

static int koreanFontGetLineHeight() {
    FontConfig cfg = getKoreanFontConfig(gCurrentFont);
    return cfg.line_height;
}

static int koreanFontGetStringWidth(const char* string) {
    FontConfig cfg = getKoreanFontConfig(gCurrentFont);
    std::vector<int> codepoints = decodeEucKr(string);
    int width = 0;
    for (int cp : codepoints) {
        const auto* cg = getGlyph(cfg.filename, cfg.render_size, cfg.width_scale, cp);
        if (cg) {
            width += cg->advance;
        }
    }
    return width;
}

static int koreanFontGetCharacterWidth(int ch) {
    FontConfig cfg = getKoreanFontConfig(gCurrentFont);
    int unicode_cp = cp949ToUnicodeChar(ch);
    const auto* cg = getGlyph(cfg.filename, cfg.render_size, cfg.width_scale, unicode_cp);
    if (cg) {
        return cg->advance;
    }
    return 0;
}

static int koreanFontGetMonospacedCharacterWidth() {
    FontConfig cfg = getKoreanFontConfig(gCurrentFont);
    const auto* cg = getGlyph(cfg.filename, cfg.render_size, cfg.width_scale, '0');
    if (cg) {
        return cg->advance;
    }
    return (int)(cfg.render_size * cfg.width_scale);
}

static int koreanFontGetMonospacedStringWidth(const char* string) {
    std::vector<int> codepoints = decodeEucKr(string);
    return koreanFontGetMonospacedCharacterWidth() * codepoints.size();
}

static int koreanFontGetLetterSpacing() {
    return 0;
}

static int koreanFontGetBufferSize(const char* string) {
    return koreanFontGetStringWidth(string) * koreanFontGetLineHeight();
}

void koreanFontSetCurrent(int font) {
    fontDrawText = koreanFontDrawText;
    fontGetLineHeight = koreanFontGetLineHeight;
    fontGetStringWidth = koreanFontGetStringWidth;
    fontGetCharacterWidth = koreanFontGetCharacterWidth;
    fontGetMonospacedStringWidth = koreanFontGetMonospacedStringWidth;
    fontGetLetterSpacing = koreanFontGetLetterSpacing;
    fontGetBufferSize = koreanFontGetBufferSize;
    fontGetMonospacedCharacterWidth = koreanFontGetMonospacedCharacterWidth;
}

void koreanFontsExit() {
    gLoadedFonts.clear();
    gGlyphCache.clear();
}

} // namespace fallout
