#include "Font.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>

#include "freetype/fttypes.h"

#define STB_RECT_PACK_IMPLEMENTATION
#include "freetype/freetype.h"
#include "stb_rect_pack/stb_rect_pack.h"

#define HB_NO_DEPRICATED
#include <hb-ft.h>
#include <hb.h>

namespace Ignis {
namespace Font {
using unicode_t = uint32_t;

// debug
void saveAtlasAsBMP(FILE *f, std::vector<uint8_t> &rgbaData, uint16_t width, uint16_t height) {
    const int row_bytes = width * 4;
    // const int palette_size = 256 * 4; no palette currently
    const int file_size = 54 + row_bytes * height;
    uint8_t header[54] = { 0 };
    header[0] = 'B';
    header[1] = 'M';
    *(uint32_t *)(header + 2) = file_size;
    *(uint32_t *)(header + 10) = 54;
    *(uint32_t *)(header + 14) = 40;
    *(int32_t *)(header + 18) = width;
    *(int32_t *)(header + 22) = height;
    *(uint16_t *)(header + 26) = 1;
    *(uint16_t *)(header + 28) = 32;
    *(uint32_t *)(header + 34) = row_bytes * height;

    fwrite(header, 1, 54, f);

    for (int y = 0; y < height; y++) {
        int src_y = height - 1 - y;
        uint8_t *src = rgbaData.data() + src_y * width * 4;

        for (int x = 0; x < width; x++) {
            uint8_t bgra[4] = { src[x * 4 + 2], src[x * 4 + 1], src[x * 4 + 0], src[x * 4 + 3] };
            fwrite(bgra, 1, 4, f);
        }
    }
}
void WriteGrayBMP(FILE *f, FT_Bitmap &bmp) {
    if (bmp.pixel_mode != FT_PIXEL_MODE_GRAY) {
        fprintf(stderr, "Unsupported pixel mode: %d\n", bmp.pixel_mode);
        return;
    }

    const int width = bmp.width;
    const int height = bmp.rows;
    const int row_bytes = ((width + 3) & ~3);  // BMP row padding to multiple of 4
    const int palette_size = 256 * 4;
    const int file_size = 54 + palette_size + row_bytes * height;

    // BMP header
    uint8_t header[54] = { 0 };
    header[0] = 'B';
    header[1] = 'M';
    *(uint32_t *)(header + 2) = file_size;           // file size
    *(uint32_t *)(header + 10) = 54 + palette_size;  // pixel data offset
    *(uint32_t *)(header + 14) = 40;                 // info header size
    *(int32_t *)(header + 18) = width;
    *(int32_t *)(header + 22) = height;
    *(uint16_t *)(header + 26) = 1;  // planes
    *(uint16_t *)(header + 28) = 8;  // bits per pixel
    fwrite(header, 1, 54, f);

    // grayscale palette
    for (int i = 0; i < 256; i++) {
        uint8_t c[4] = { static_cast<uint8_t>(i), static_cast<uint8_t>(i), static_cast<uint8_t>(i), 0 };
        fwrite(c, 1, 4, f);
    }

    // Allocate a padded row buffer
    uint8_t *row = (uint8_t *)malloc(row_bytes);
    if (!row) return;

    // Write bitmap rows bottom-up
    for (int y = 0; y < height; y++) {
        uint8_t *src = bmp.buffer + (height - 1 - y) * abs(bmp.pitch);
        memcpy(row, src, width);
        memset(row + width, 0, row_bytes - width);  // padding
        fwrite(row, 1, row_bytes, f);
    }

    free(row);
}

// TODO: needs testing
// only tested on ascii
std::u32string sToU32s(const std::string_view &utf8) {
    std::u32string utf32;

    size_t i = 0;
    while (i < utf8.size()) {
        uint32_t cp = 0;
        unsigned char c = utf8[i];
        if (c <= 0x7F) {
            cp = c;
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = ((c & 0x0F) << 12) | ((utf8[i + 1] & 0x3F) << 6) | (utf8[i + 2] & 0x3F);
            i += 3;
        } else {
            cp = ((c & 0x07) << 18) | ((utf8[i + 1] & 0x3F) << 12) | ((utf8[i + 2] & 0x3F) << 6) | (utf8[i + 3] & 0x3F);
            i += 4;
        }
        utf32.push_back(cp);
    }
    return utf32;
}
hb_direction_t toHBDirection(TextDirection td) {
    switch (td) {
        case TextDirection::LTR:
            return HB_DIRECTION_LTR;
        case TextDirection::RTL:
            return HB_DIRECTION_RTL;
        case TextDirection::BTT:
            return HB_DIRECTION_BTT;
        case TextDirection::TTB:
            return HB_DIRECTION_TTB;
        case TextDirection::GUESS:
            return HB_DIRECTION_INVALID;
    }
    return HB_DIRECTION_INVALID;
}

// TODO: finish switch
hb_script_t toHBScript(Script s) {
    switch (s) {
        case Script::LATIN:
            return HB_SCRIPT_LATIN;
        default:
            return HB_SCRIPT_UNKNOWN;
    }
    return HB_SCRIPT_UNKNOWN;
}

hb_script_t getHbScript(uint32_t codepoint) {
    hb_unicode_funcs_t *unicode_funcs = hb_unicode_funcs_get_default();
    return hb_unicode_script(unicode_funcs, codepoint);
}

// we will see if this is needed later
// currently not used
std::vector<std::pair<hb_script_t, std::vector<uint32_t>>> getFontScriptRanges(hb_face_t *face) {
    hb_set_t *unicodes = hb_set_create();
    hb_face_collect_unicodes(face, unicodes);

    std::unordered_map<hb_script_t, std::vector<uint32_t>> scriptRanges;

    hb_codepoint_t codepoint;
    while (hb_set_next(unicodes, &codepoint)) {
        hb_script_t script = getHbScript(codepoint);
        scriptRanges[script].push_back(codepoint);
    }

    hb_set_destroy(unicodes);

    // sort
    std::vector<std::pair<hb_script_t, std::vector<uint32_t>>> result;
    for (auto &[script, cps] : scriptRanges) {
        std::sort(cps.begin(), cps.end());
        result.emplace_back(script, std::move(cps));
    }
    return result;
}

void Font::preloadPageByRange(const uint32_t unicodeStart, const uint32_t unicodeEnd) { packUnicodeRange(unicodeStart, unicodeEnd, defaultSize); };

void Font::initializeFont(const std::string filename) {
    FT_Init_FreeType(&lib);
    FT_Error err = FT_New_Face(lib, filename.c_str(), 0, &ftFace);

    if (err == FT_Err_Unknown_File_Format) {
        std::cerr << "Font format is unsupported\n";
        return;
    } else if (err) {
        std::cerr << "Failed to load font\n";
        return;
    }
    FT_Set_Pixel_Sizes(ftFace, 0, defaultSize);
    std::cout << "Font loaded: " << ftFace->family_name << " " << ftFace->style_name << "\n";

    // preloadAscii
    preloadPageByRange(0x0021, 0x007E);
}

std::vector<ShapedGlyph> Font::shapeText(const std::u32string &text, int fontSize, TextAlign align, TextDirection direction, Style style) {
    // TODO: not appease the unused warnings
    align = align;
    direction = direction;
    style = style;

    fontSize = defaultSize;

    hb_font_t *font = hb_ft_font_create_referenced(ftFace);
    buf = hb_buffer_create();
    hb_buffer_add_utf32(buf, reinterpret_cast<const uint32_t *>(text.data()), text.length(), 0, text.size());

    // hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    // hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    // hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_buffer_guess_segment_properties(buf);

    // After shaping codepoints mean glyphIndex
    hb_shape(font, buf, nullptr, 0);

    uint32_t glyphCount;
    hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
    hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);

    std::vector<ShapedGlyph> shapedGlyphs;
    shapedGlyphs.reserve(glyphCount);
    std::vector<hb_codepoint_t> notPaged;
    hb_position_t cursorX = 0;
    hb_position_t cursorY = 0;
    int same = 0;
    for (uint32_t i = 0; i < glyphCount; i++) {
        hb_codepoint_t cp = glyphInfo[i].codepoint;
        uint32_t cluster = glyphInfo[i].cluster;
        if (pagePosition[fontSize].contains(cp)) same++;
        Glyph *glyph = nullptr;
        uint32_t pN = 0;
        uint32_t texId = 0;
        // TODO: figure out a way to do this for every character that doesn't need drawing
        // also this should be in a separate loop
        if (text[cluster] == ' ') {
        } else if (!pagePosition[fontSize].contains(cp)) {
            notPaged.push_back(cp);
            continue;
        } else {
            pN = pagePosition[fontSize][cp];
            glyph = &pages[pN].glyphs.at(cp);
            texId = pages[pN].textureId;
        }

        hb_position_t xOffset = glyphPos[i].x_offset;
        hb_position_t yOffset = glyphPos[i].y_offset;
        hb_position_t xAdvance = glyphPos[i].x_advance;
        hb_position_t yAdvance = glyphPos[i].y_advance;

        shapedGlyphs.push_back({ glyph, texId, xOffset, yOffset, xAdvance, yAdvance, cluster });
        cursorX += xAdvance;
        cursorY += yAdvance;
    }
    hb_buffer_destroy(buf);
    hb_font_destroy(font);

    return shapedGlyphs;
};

hb_script_t getHarfBuzzScript(uint32_t codepoint) {
    hb_script_t script;
    hb_unicode_funcs_t *unicode_funcs = hb_unicode_funcs_get_default();
    script = hb_unicode_script(unicode_funcs, codepoint);
    return script;
}

uint16_t nextPow2(uint16_t num) {
    num--;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num++;
    return num;
}

struct Outline {
    Outline(FT_Outline &ftOutline) {
        flags = ftOutline.flags;
        numContours = ftOutline.n_contours;
        numPoints = ftOutline.n_points;
        points.reserve(numPoints);
        tags.reserve(numPoints);
        for (int pIdx = 0; pIdx < numPoints; pIdx++) {
            points[pIdx].x = ftOutline.points[pIdx].x;
            points[pIdx].y = ftOutline.points[pIdx].y;

            tags[pIdx] = ftOutline.tags[pIdx];
        }

        contours.reserve(numContours);
        for (int cIdx = 0; cIdx < numContours; cIdx++) contours[cIdx] = ftOutline.contours[cIdx];
    }

    uint16_t numContours;
    uint16_t numPoints;

    std::vector<Vec2i> points;        // length of numPoints
    std::vector<unsigned char> tags;  // length of numPoints
    std::vector<uint16_t> contours;   // length of numContours

    int flags;
};

void generateMSDF(uint8_t *bmp, Outline outline, int channel = 1) {
}

void Font::packUnicodeRange(const uint32_t unicodeStart, const uint32_t unicodeEnd, int16_t fontSize, const Style style, const TextDirection, const int maxCharPerPage, const bool autoPageSize, const uint16_t pSize) {
    if (!ftFace) return;

    if (fontSize < 1) fontSize = defaultSize;

    FT_Set_Pixel_Sizes(ftFace, 0, fontSize);
    hb_face_t *hbFace = hb_ft_face_create_referenced(ftFace);

    std::vector<std::pair<unicode_t, hb_codepoint_t>> scriptCodepoints;

    FT_UInt glyphIndex;
    FT_ULong charcode = FT_Get_First_Char(ftFace, &glyphIndex);
    while (glyphIndex != 0) {
        if ((charcode >= unicodeStart && charcode <= unicodeEnd)) {
            scriptCodepoints.push_back({ charcode, glyphIndex });
        }
        charcode = FT_Get_Next_Char(ftFace, charcode, &glyphIndex);
    }

    std::cout << "Packing U+" << std::hex << unicodeStart << std::dec << " - U+" << std::hex << unicodeEnd << std::dec << ": " << scriptCodepoints.size() << " glyphs\n";

    hb_face_destroy(hbFace);

    if (scriptCodepoints.empty()) return;

    const uint16_t maxPageSize = 2048;
    const uint32_t maxPageArea = maxPageSize * maxPageSize;
    uint32_t charPerPage = maxCharPerPage;
    if (maxCharPerPage < 1) {
        uint64_t maxChars = (static_cast<uint64_t>(maxPageSize) * static_cast<uint64_t>(maxPageSize)) / (fontSize * fontSize);
        charPerPage = std::min(static_cast<uint32_t>(scriptCodepoints.size()) + 1, static_cast<uint32_t>(std::round(maxChars)));
    }

    uint32_t padding = std::min(5, std::max(1, fontSize % 20));

    // Auto page size
    uint16_t pageWidth = pSize, pageHeight = pSize;

    int glyphsAdded = 0;
    size_t glyphsProcessed = 0;
    int pageCount = 0;
    int badCounter = 0;
    while (glyphsProcessed < scriptCodepoints.size()) {
        uint32_t pageSize = 0;
        size_t glyphsToPackCount = 0;
        if (autoPageSize) {
            uint32_t totalArea = 0;
            for (size_t ci = glyphsProcessed; ci < scriptCodepoints.size(); ci++) {
                int cp = scriptCodepoints[ci].second;
                int unicode = scriptCodepoints[ci].first;
                if (unicode < 0x0020) continue;
                FT_Load_Glyph(ftFace, cp, FT_LOAD_RENDER);
                uint32_t w = ftFace->glyph->bitmap.width + padding;
                uint32_t h = ftFace->glyph->bitmap.rows + padding;
                totalArea += w * h;
                if (totalArea > maxPageArea * 0.90 || glyphsToPackCount == charPerPage) {
                    pageSize = std::min(maxPageSize, std::max(pageWidth, nextPow2(static_cast<uint16_t>(std::sqrt(totalArea)))));
                    break;
                }
                glyphsToPackCount++;
            }
            if (pageSize == 0) pageSize = std::min(maxPageSize, std::max(pageWidth, nextPow2(static_cast<uint16_t>(std::sqrt(totalArea)))));
        }
        size_t glyphsToPack = std::min(static_cast<size_t>(glyphsToPackCount), scriptCodepoints.size() - glyphsProcessed);
        std::vector<std::pair<unicode_t, hb_codepoint_t>> pageCodepoints(scriptCodepoints.begin() + glyphsProcessed, scriptCodepoints.begin() + glyphsProcessed + glyphsToPack);
        pageWidth = pageHeight = pageSize;

        Page page(pageWidth, pageHeight, fontSize, style);
        std::vector<uint8_t> textureData(pageWidth * pageHeight * 4, 0);

        std::vector<stbrp_rect> rects;
        std::vector<std::pair<unicode_t, hb_codepoint_t>> validPageCodepoints;

        // TODO: change this to something more efficient
        for (size_t i = 0; i < glyphsToPack; ++i) {
            auto [unicode, cp] = scriptCodepoints[glyphsProcessed + i];
            FT_Load_Glyph(ftFace, cp, FT_LOAD_RENDER);
            const FT_Bitmap &bmp = ftFace->glyph->bitmap;

            if (bmp.width == 0 || bmp.rows == 0) {
                badCounter++;
                continue;
            }

            stbrp_rect rect;
            rect.w = bmp.width + padding;
            rect.h = bmp.rows + padding;
            rect.id = validPageCodepoints.size();
            rects.push_back(rect);
            validPageCodepoints.push_back({ unicode, cp });
        }

        if (rects.empty()) {
            glyphsProcessed += glyphsToPack;
            continue;
        }
        // Pack layout
        stbrp_context context;
        std::vector<stbrp_node> nodes(pageWidth);
        stbrp_init_target(&context, pageWidth, pageHeight, nodes.data(), pageWidth);
        stbrp_pack_rects(&context, rects.data(), rects.size());

        int failedPacks = 0;
        // BLIT shit together
        for (size_t i = 0; i < rects.size(); ++i) {
            if (!rects[i].was_packed) {
                failedPacks++;
                scriptCodepoints.push_back(validPageCodepoints[i]);
                continue;
            }

            auto [unicode, cp] = validPageCodepoints[rects[i].id];
            // EXPERIMENTAL
            FT_Load_Glyph(ftFace, cp, FT_LOAD_DEFAULT);
            if (ftFace->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
                std::cout << "outline\n";
                auto &outline = ftFace->glyph->outline;
                generateMSDF(nullptr, outline);
            };

            // EXPERIMENTAL END
            FT_Load_Glyph(ftFace, cp, FT_LOAD_RENDER);

            const FT_Bitmap &bmp = ftFace->glyph->bitmap;
            FT_GlyphSlot slot = ftFace->glyph;

            // offset the boxes by padding
            int32_t glyph_x = rects[i].x + (padding / 2);
            int32_t glyph_y = rects[i].y + (padding / 2);

            int pitch = std::abs(bmp.pitch);

            // This has two purposes
            // 1. if pitch is negative it flips the letter
            // 2. FreeType y is inverted, so this flips it around
            bool flipped = bmp.pitch < 0;

            for (uint16_t py = 0; py < bmp.rows; ++py) {
                int srcY = flipped ? (bmp.rows - 1 - py) : py;
                const uint8_t *row = bmp.buffer + srcY * pitch;

                for (uint16_t px = 0; px < bmp.width; ++px) {
                    uint8_t alpha = row[px];
                    if (!alpha) continue;

                    int x = glyph_x + px;
                    int y = glyph_y + py;
                    if ((unsigned)x >= pageWidth || (unsigned)y >= pageHeight) {
                        continue;
                    }

                    uint32_t idx = (y * pageWidth + x) * 4;
                    textureData[idx + 0] = 255;
                    textureData[idx + 1] = 255;
                    textureData[idx + 2] = 255;
                    textureData[idx + 3] = alpha;
                }
            }
            glyphsAdded++;
            Glyph glyph(unicode, cp, glyph_x, glyph_y);
            glyph.w = bmp.width;
            glyph.h = bmp.rows;
            glyph.bearingX = slot->metrics.horiBearingX;
            glyph.bearingY = slot->metrics.horiBearingY;
            glyph.advanceX = slot->metrics.horiAdvance;
            glyph.advanceY = slot->metrics.vertAdvance;

            // this shit sets the uvs
            page.addGlyph(glyph);
            pagePosition[fontSize].insert({ cp, pages.size() });
        }
        page.textureId = Render::CreateFontPage(textureData, pageWidth, pageHeight);

        char filename[20];
        sprintf(filename, "goated%u.bmp", pageCount);
        FILE *fbmp = fopen(filename, "wb");
        saveAtlasAsBMP(fbmp, textureData, page.w, page.h);
        fclose(fbmp);

        pages.push_back(std::move(page));
        glyphsProcessed += glyphsToPack;
        pageCount++;
    }
}

Font::~Font() {
    std::cout << "Font Cleanup\n";
    FT_Done_Face(ftFace);
    FT_Done_FreeType(lib);
}

void Page::addGlyph(Glyph &glyph) {
    glyph.u0 = static_cast<float>(glyph.x) / w;
    glyph.v0 = static_cast<float>(glyph.y) / h;
    glyph.u1 = static_cast<float>(glyph.x + glyph.w) / w;
    glyph.v1 = static_cast<float>(glyph.y + glyph.h) / h;

    glyphs.insert({ glyph.glyphIndex, glyph });
}

}  // namespace Font
}  // namespace Ignis
