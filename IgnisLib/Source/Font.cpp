#include "Font.h"

// TODO:
//  creates bitmap struct? class?

#include <algorithm>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <latch>
#include <map>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

#include "freetype/freetype.h"
#include "freetype/ftbbox.h"
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack/stb_rect_pack.h"

#define HB_NO_DEPRICATED
#include <hb-ft.h>
#include <hb.h>

namespace Ignis {
namespace Font {
using unicode_t = uint32_t;

static std::atomic<int> testc = 0;
static std::atomic<int> test2c = 0;
// debug
struct Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point start;

    Timer() { start = clock::now(); }

    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(
                   clock::now() - start)
            .count();
    }
};

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
    Timer t{};
    for (int i = 1; i <= 1; i++) {
        packUnicodeRangeSDF(0x0021, 0x007F);  // 1BC

        std::cout << i << ". time elapsed: " << t.elapsed_ms() / i << "ms\n";
    }
    std::cout << "time elapsed: " << t.elapsed_ms() << "ms\n";
    std::cout << "average: " << t.elapsed_ms() / 1000 << "ms\n";
}

std::vector<ShapedGlyph> Font::shapeText(const std::u32string &text, int fontSize, TextAlign align, TextDirection direction, Style style) {
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
    for (uint32_t i = 0; i < glyphCount; i++) {
        hb_codepoint_t cp = glyphInfo[i].codepoint;
        uint32_t cluster = glyphInfo[i].cluster;
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

enum BezierOrder {
    UNSET = 0,
    LINEAR = 2,
    QUADRATIC = 3,
    CUBIC = 4
};

std::string bezierOrderToString(BezierOrder order) {
    switch (order) {
        case UNSET:
            return "unset";
        case LINEAR:
            return "linear";
        case QUADRATIC:
            return "quadratic";
        case CUBIC:
            return "cubic";
    }
    assert(false && "unreachable return");
}

template <typename T>
bool isBitSet(T var, int index) {
    return ((1 << index) & var) != 0;
}

bool isOnCurve(unsigned char tag) {
    return isBitSet(tag, 0);
}

BezierOrder getBezierOrder(unsigned char tag) {
    if (isBitSet(tag, 1)) {
        return CUBIC;
    } else {
        return QUADRATIC;
    }
}
struct BBox {
    float minX = INFINITY, maxX = -INFINITY;
    float minY = INFINITY, maxY = -INFINITY;
};

struct Bezier {
    std::vector<uint16_t> pointsIdx;
    BezierOrder order = UNSET;
    BBox box;
    Vec2f aux1 = Vec2f(INFINITY, INFINITY);
    Vec2f aux2;
    Vec2f aux3;
};

// one or multiple bezier curve indexes
struct Segment {
    uint16_t contourIdx;
    std::vector<uint16_t> bezierIdxs;

    BBox box;
};
struct Outline {
    Outline(FT_Outline &ftOutline, const uint16_t &units, const uint32_t &charcode) {
        unitsPerEM = units;

        flags = ftOutline.flags;
        numContours = ftOutline.n_contours;
        numPoints = ftOutline.n_points;
        unicode = charcode;

        points.resize(numPoints);
        tags.resize(numPoints);
        for (int pIdx = 0; pIdx < numPoints; pIdx++) {
            points[pIdx].x = ftOutline.points[pIdx].x;
            points[pIdx].y = ftOutline.points[pIdx].y;

            tags[pIdx] = ftOutline.tags[pIdx];
        }

        contours.resize(numContours);
        for (int cIdx = 0; cIdx < numContours; cIdx++) {
            contours[cIdx] = ftOutline.contours[cIdx];
        }
        curvesInContours.resize(numContours);

        tResults.reserve(5);
    }

    void printOutline() {
        std::cout << "\nPrint Outline\n";
        std::cout << "numContours: " << numContours << "\n";
        std::cout << "numPoints: " << numPoints << "\n";
        for (int pIdx = 0; pIdx < numPoints; pIdx++) {
            std::cout << "pointIdx: " << pIdx << " xy:(" << points[pIdx].x << "," << points[pIdx].y << ")" << " onCurve: " << isOnCurve(tags[pIdx]) << " " << (isOnCurve(tags[pIdx]) ? "" : bezierOrderToString(getBezierOrder(tags[pIdx]))) << "\n";
        }
        for (int cIdx = 0; cIdx < numContours; cIdx++) {
            std::cout << "contour start: " << ((cIdx > 0) ? contours[cIdx - 1] + 1 : 0) << ", end: " << contours[cIdx] << "\n";
        }
    }

    void printBeziers() {
        std::cout << "\nPrint Beziers\n";
        int idx = 0;
        for (auto &curves : curvesInContours) {
            std::cout << "contourIdx: " << idx << "\n";
            for (auto &bez : curves) {
                std::cout << "idx: " << idx << " pIdxs: ";
                for (auto &pIdx : bez.pointsIdx) {
                    std::cout << pIdx << " ";
                }
                std::cout << "\n";
                std::cout << "bez order: " << bezierOrderToString(bez.order) << "\n";
                idx++;
            }
            std::cout << "\n";
        }
    }

    void printSegments() {
        std::cout << "\nPrint Segments\n";
        for (auto &seg : segments) {
            std::cout << "bezIdxs: ";
            for (auto &bezIdx : seg.bezierIdxs) {
                uint16_t idx = bezIdx;
                if (seg.contourIdx > 0) {
                    for (int i = 0; i < seg.contourIdx; i++)
                        idx += curvesInContours[i].size();
                }
                std::cout << idx << " ";
            }
            std::cout << "\n";
        }
    }

    uint16_t unitsPerEM;
    float xMin = INFINITY, xMax = -INFINITY;
    float yMin = INFINITY, yMax = -INFINITY;
    uint32_t unicode;

    uint16_t numContours;
    uint16_t numPoints;
    int flags;

    std::vector<Vec2f> points;        // length of numPoints
    std::vector<unsigned char> tags;  // length of numPoints
    std::vector<uint16_t> contours;   // length of numContours

    std::vector<std::vector<Bezier>> curvesInContours;

    std::vector<Segment> segments;

    float padding = 64.0f * 64 / 2;

    float xSize;
    float ySize;
    float xPadding;
    float yPadding;

    Vec2f pointAtTOnBezier(Bezier &bez, const float &t);
    Vec2f derivativeOfBezier(const Bezier &bez, const float t);
    float shortestDistanceToBezier(const Vec2f &point, Bezier &bezier, float *tOut);
    float signOfDistance(const Vec2f &point, Bezier &bezier, const float &t);
    float ortogonality(const Vec2f &point, Bezier &bezier, const float t);

    void sanitize();
    void addImpliedPoints();
    void populateBounds();
    void populateBeziers();
    void getSegments(float acceptedAngleDeviation);
    Bezier &findClosestBez(const Vec2f point, float *distOut, float *t);

    Vec2f transformCoord(const float x, const float y);
    uint8_t distToColor(const float dist, const float maxDist);
    void createSDFBitmap(std::vector<uint8_t> &atlas, const uint32_t &atlasWidth, const uint32_t &atlasHeight, const uint32_t startX, const uint32_t startY, const uint16_t width = 64, const uint16_t height = 64);

    // help
    std::vector<float> tResults;
};

void Outline::sanitize() {
    uint16_t contourStart = 0;
    uint16_t contourEnd = 0;
    uint16_t removedPointsCount = 0;
    for (uint16_t contIdx = 0; contIdx < numContours; contIdx++) {
        // Every contour after the first starts at cotours[i] + 1
        if (contIdx > 0) contourStart = contourEnd + 1;
        contourEnd = contours[contIdx] - removedPointsCount;
        for (uint16_t pIdx = contourStart + 1; pIdx <= contourEnd; pIdx++) {
            if (points[pIdx - 1] == points[pIdx]) {
                if (!isOnCurve(tags[pIdx])) {
                    points.erase(points.begin() + pIdx);
                    tags.erase(tags.begin() + pIdx);
                } else {
                    points.erase(points.begin() + pIdx - 1);
                    tags.erase(tags.begin() + pIdx - 1);
                }
                pIdx--;
                contourEnd--;
                removedPointsCount++;
            }
        }
        contours[contIdx] = contourEnd;
    }
    numPoints = points.size();
}

void Outline::addImpliedPoints() {
    // first contour starts at point 0
    uint16_t contourStart = 0;
    uint16_t contourEnd = 0;
    uint16_t impliedPointsCount = 0;
    for (uint16_t contIdx = 0; contIdx < numContours; contIdx++) {
        // Every contour after the first starts at cotours[i] + 1
        if (contIdx > 0) contourStart = contourEnd + 1;
        contourEnd = contours[contIdx] + impliedPointsCount;
        for (uint16_t pIdx = contourStart; pIdx < contourEnd; pIdx++) {
            uint16_t nextPIdx = (pIdx == contourEnd) ? contourStart : pIdx + 1;
            bool onCurve0 = isOnCurve(tags[pIdx]);
            bool onCurve1 = isOnCurve(tags[nextPIdx]);
            if (onCurve0 || onCurve1) continue;

            if (CUBIC == getBezierOrder(tags[pIdx]) || CUBIC == getBezierOrder(tags[nextPIdx])) return;

            Vec2f impliedPoint{};
            impliedPoint = (points[pIdx] + points[nextPIdx]) / 2.0f;
            points.emplace(points.begin() + nextPIdx, impliedPoint);
            tags.insert(tags.begin() + nextPIdx, { 0x1 });  // only set the onCurve bit
            pIdx++;
            contourEnd++;
            impliedPointsCount++;
        }
        contours[contIdx] = contourEnd;
    }
    numPoints = points.size();
}

void Outline::populateBounds() {
    for (auto &p : points) {
        xMin = std::min(p.x, xMin);
        xMax = std::max(p.x, xMax);
        yMin = std::min(p.y, yMin);
        yMax = std::max(p.y, yMax);
    }
    xSize = xMax - xMin;
    ySize = yMax - yMin;
    xPadding = (xSize < ySize) ? (1 - (xSize / ySize)) * ySize + padding : padding;
    yPadding = (ySize < xSize) ? (1 - (ySize / xSize)) * xSize + padding : padding;

    // std::cout << "\nBounds:\n";
    // std::cout << "xMin: " << xMin << " xMax: " << xMax << "\n";
    // std::cout << "yMin: " << yMin << " yMax: " << yMax << "\n";

    // uint16_t padding = unitsPerEM / 8;
    // xMin -= padding;
    // xMax += padding;
    // yMin -= padding;
    // yMax += padding;
    // std::cout << "\nBounds after padding:\n";
    // std::cout << "xMin: " << xMin << " xMax: " << xMax << "\n";
    // std::cout << "yMin: " << yMin << " yMax: " << yMax << "\n";
}

void Outline::populateBeziers() {
    // first contour starts at point 0
    uint16_t contourStart = 0;
    uint16_t contourEnd = 0;
    for (uint16_t contIdx = 0; contIdx < numContours; contIdx++) {
        // Every contour after the first starts at cotours[i] + 1
        if (contIdx > 0) contourStart = contourEnd + 1;
        contourEnd = contours[contIdx];
        Bezier bz{};
        bool firstIsOnCurve = true;

        for (uint16_t pIdx = contourStart; pIdx <= contourEnd + 1 + !(firstIsOnCurve); ++pIdx) {
            if (pIdx == contourStart && !isOnCurve(tags[pIdx]) && firstIsOnCurve) {
                firstIsOnCurve = false;
                continue;
            }
            uint16_t idx = pIdx;
            if (pIdx == contourEnd + 1) {
                idx = contourStart;
            } else if (pIdx == contourEnd + 2) {
                idx = contourStart + 1;
            }
            bool onCurve = isOnCurve(tags[idx]);

            bz.pointsIdx.push_back(idx);

            if (bz.order == UNSET) {
                if (!onCurve)
                    bz.order = getBezierOrder(tags[idx]);
                else if (bz.pointsIdx.size() == 2)
                    bz.order = LINEAR;
            }

            if (bz.order && bz.pointsIdx.size() == bz.order) {
                if (bz.order == LINEAR && bz.pointsIdx[0] == bz.pointsIdx[1]) {
                    bz = {};
                    continue;
                }
                for (auto &pIdx : bz.pointsIdx) {
                    Vec2f &point = points[pIdx];
                    if (point.x < bz.box.minX) bz.box.minX = point.x;
                    if (point.x > bz.box.maxX) bz.box.maxX = point.x;
                    if (point.y < bz.box.minY) bz.box.minY = point.y;
                    if (point.y > bz.box.maxY) bz.box.maxY = point.y;
                }
                curvesInContours[contIdx].push_back(bz);
                bz = {};
                bz.pointsIdx.push_back(idx);
            }
        }
    }
}

Vec2f Outline::pointAtTOnBezier(Bezier &bez, const float &t) {
    if (!(t <= 1.0f && t >= 0.0f)) {
        std::cerr << "t: " << t << "\n";
    }
    assert(t <= 1.0f && t >= 0.0f && "t must be between 0 and 1");
    if (bez.order == LINEAR) {
        if (t == 0.0f)
            return points[bez.pointsIdx[0]];
        else if (t == 1.0f)
            return points[bez.pointsIdx[1]];
        Vec2f &P0 = points[bez.pointsIdx[0]];
        Vec2f &P1 = points[bez.pointsIdx[1]];

        // P0 + t(P1 - P0)
        return P0 + (P1 - P0) * t;
    } else if (bez.order == QUADRATIC) {
        if (t == 0.0f)
            return points[bez.pointsIdx[0]];
        else if (t == 1.0f)
            return points[bez.pointsIdx[2]];
        Vec2f &P0 = points[bez.pointsIdx[0]];
        Vec2f &P1 = points[bez.pointsIdx[1]];
        Vec2f &P2 = points[bez.pointsIdx[2]];

        // P0 + 2t(P1 − P0) + t^2(P2 − 2P1 + P0)
        return P0 + (P1 - P0) * 2.0f * t + (P2 - P1 * 2.0f + P0) * t * t;
    } else if (bez.order == CUBIC) {
        if (t == 0.0f)
            return points[bez.pointsIdx[0]];
        else if (t == 1.0f)
            return points[bez.pointsIdx[3]];

        Vec2f &P0 = points[bez.pointsIdx[0]];
        Vec2f &P1 = points[bez.pointsIdx[1]];
        Vec2f &P2 = points[bez.pointsIdx[2]];
        Vec2f &P3 = points[bez.pointsIdx[3]];

        // P0 + 3t(P1 − P0) + 3t^2(P2 − 2P1 + P0) + t^3(P3 − 3P2 + 3P1 − P0)
        return P0 + (P1 - P0) * 3.0f * t + (P2 - P1 * 2.0f + P0) * 3 * t * t + (P3 - P2 * 3.0f + P1 * 3.0f - P0) * t * t * t;
    }
    assert(false && "unreachable");
}

Vec2f Outline::derivativeOfBezier(const Bezier &bez, const float t) {
    assert(t <= 1 && t >= 0 && "t must be between 0 and 1");
    if (bez.order == LINEAR) {
        Vec2f &P0 = points[bez.pointsIdx[0]];
        Vec2f &P1 = points[bez.pointsIdx[1]];
        return P1 - P0;
    } else if (bez.order == QUADRATIC) {
        Vec2f &P0 = points[bez.pointsIdx[0]];
        Vec2f &P1 = points[bez.pointsIdx[1]];
        Vec2f &P2 = points[bez.pointsIdx[2]];

        if (t == 0)
            // 2(P1 - P0)
            return (P1 - P0) * 2.0f;
        else if (t == 1)
            // 2(P2 - P1)
            return (P2 - P1 * 2.0f + P0) * 2.0f + (P1 - P0) * 2.0f;
        else
            // 2t(P2 - 2P1 + P0) + 2(P1 - P0)
            return (P2 - P1 * 2.0f + P0) * 2.0f * t + (P1 - P0) * 2.0f;

    } else if (bez.order == CUBIC) {
        Vec2f &P0 = points[bez.pointsIdx[0]];
        Vec2f &P1 = points[bez.pointsIdx[1]];
        Vec2f &P2 = points[bez.pointsIdx[2]];
        Vec2f &P3 = points[bez.pointsIdx[3]];

        if (t == 0)
            // 3(P2-P1)
            return (P2 - P1) * 3.0f;
        else if (t == 1)
            // 3(P3-P2)
            return (P3 - P2) * 3.0f;
        else
            // 3t^2(P3 − 3P2 + 3P1 − P0) + 6t(P2 − 2P1 + P0) + 3(P1 − P0)
            return (P3 - P2 * 3.0f + P1 * 3.0f - P0) * 3 * t * t + (P2 - P1 * 2.0f + P0) * 6.0f * t + (P1 - P0) * 3;
    }
    assert(false && "unreachable");
}

// This is used for getting the start and end point index of a continuous segment
void Outline::getSegments(float acceptedAngleDeviation) {
    // first contour starts at point 0
    for (uint16_t contIdx = 0; contIdx < numContours; contIdx++) {
        std::vector<Bezier> &beziers = curvesInContours[contIdx];
        Segment seg{};
        seg.contourIdx = contIdx;
        for (uint16_t bIdx = 0; bIdx < beziers.size(); bIdx++) {
            seg.bezierIdxs.push_back(bIdx);
            uint16_t nextBIdx = (bIdx + 1) % beziers.size();
            Bezier &bez0 = beziers[bIdx];
            Bezier &bez1 = beziers[nextBIdx];
            // get the direction of the bezier at the same points
            // bez0's last point is the first point of bez1
            // so we take bez0 at t = 1 and bez1 at t = 0
            Vec2f derBez0 = derivativeOfBezier(bez0, 1);
            Vec2f derBez1 = derivativeOfBezier(bez1, 0);

            // normalize the derivatives
            Vec2f normDerBez0 = derBez0.normalize();
            Vec2f normDerBez1 = derBez1.normalize();

            float crossProduct = normDerBez0.crossProduct(normDerBez1);
            float sinDev = sin(acceptedAngleDeviation);
            // check if its a corner
            // NOTE: this may need a check if vectors are opposite
            if (std::fabs(crossProduct) > sinDev) {
                for (auto &bezIdx : seg.bezierIdxs) {
                    Bezier &bz = beziers[bezIdx];
                    if (bz.box.minX < seg.box.minX) seg.box.minX = bz.box.minX;
                    if (bz.box.maxX > seg.box.maxX) seg.box.maxX = bz.box.maxX;
                    if (bz.box.minY < seg.box.minY) seg.box.minY = bz.box.minY;
                    if (bz.box.maxY > seg.box.maxY) seg.box.maxY = bz.box.maxY;
                }
                segments.push_back(seg);
                seg.bezierIdxs.clear();
            }
        }
        if (!seg.bezierIdxs.empty()) {
            for (auto &bezIdx : seg.bezierIdxs) {
                Bezier &bz = beziers[bezIdx];
                if (bz.box.minX < seg.box.minX) seg.box.minX = bz.box.minX;
                if (bz.box.maxX > seg.box.maxX) seg.box.maxX = bz.box.maxX;
                if (bz.box.minY < seg.box.minY) seg.box.minY = bz.box.minY;
                if (bz.box.maxY > seg.box.maxY) seg.box.maxY = bz.box.maxY;
            }
            segments.push_back(seg);
        }
    }
}

// Otrogonality matters for when two beziers are the same
// distance away from a point.
// in that case we need to maximize Ortogonality.
float Outline::ortogonality(const Vec2f &point, Bezier &bezier, const float t) {
    Vec2f detNorm = derivativeOfBezier(bezier, t).normalize();
    Vec2f dispNorm = (point - pointAtTOnBezier(bezier, t)).normalize();

    // | ((dB(t)/dt)/||(dB(t)/dt)||) x ((P - B(t))/||P - B(t)||) |
    return fabs(detNorm.crossProduct(dispNorm));
}

// Classic quadratic
int quadraticSolver(std::vector<float> &roots, const float a, const float b, const float c) {
    if (a == 0) {
        assert(fabs(b) > 1e-8f && "god save us");
        // x = -c/b;
        roots.push_back(-c / b);
        return roots.size();
    }
    float discriminant = b * b - 4 * a * c;

    if (discriminant >= 1e-8f) {
        roots.push_back((-b + fsqrt(discriminant)) / (2 * a));
        roots.push_back((-b - fsqrt(discriminant)) / (2 * a));
    } else if (discriminant < 1e-8f) {
        roots.push_back(-b / (2 * a));
    } else {
        // for our purposes the complex root is not needed
        assert(false && "only complex solution to quadratic");
    }
    return roots.size();
}

// Cardano's formula
int cubicSolver(std::vector<float> &roots, const float a, const float b, const float c, const float d) {
    if (a == 0) return quadraticSolver(roots, b, c, d);  // not a cubic

    // we need to depress this happy boy
    // ie.: make the quadratic part 0
    float p = (3.0f * a * c - b * b) / (3.0f * a * a);
    float q = (2.0f * b * b * b - 9.0f * a * b * c + 27.0f * a * a * d) / (27.0f * a * a * a);

    float discriminant = (q * q) / 4.0f + (p * p * p) / 27.0f;

    const float eps = 1e-8f;

    // if discriminant is positive there is only one root
    if (discriminant > eps) {  // one real root
        float sqrt_disc = fsqrt(discriminant);
        float u = cbrtf(-q / 2.0f + sqrt_disc);
        float v = cbrtf(-q / 2.0f - sqrt_disc);
        roots.push_back(u + v - b / (3.0f * a));

        // If discrimanant is negative it has 3 roots
        // and if its 0 it has two or 3, but
        // if thats the case we just get some duplicates
    } else {  // three real roots
        float r = fsqrt(-p * p * p / 27.0f);
        float phi = acosf(std::clamp(-q / (2.0f * r), -1.0f, 1.0f));
        float t = 2 * cbrtf(r);
        roots.push_back(t * cosf(phi / 3.0f) - b / (3.0f * a));
        roots.push_back(t * cosf((phi + 2.0f * std::numbers::pi) / 3.0f) - b / (3.0f * a));
        roots.push_back(t * cosf((phi + 4.0f * std::numbers::pi) / 3.0f) - b / (3.0f * a));
    }

    return roots.size();
}

float Outline::shortestDistanceToBezier(const Vec2f &point, Bezier &bezier, float *tOut = nullptr) {
    if (bezier.order == LINEAR) {
        Vec2f &P0 = points[bezier.pointsIdx[0]];
        Vec2f &P1 = points[bezier.pointsIdx[1]];

        // P = Point
        // t = [(P - P0) * (P1 - P0)] / [(P1 - P0 ) * ( P1 - P0)]
        Vec2 aux0 = P1 - P0;
        float t = 0;
        if (P0 != P1)
            t = (point - P0).dotProduct(aux0) / (aux0).dotProduct(aux0);

        t = std::clamp(t, 0.0f, 1.0f);
        Vec2f Pt = pointAtTOnBezier(bezier, t);
        if (tOut != nullptr) *tOut = t;
        return Pt.distance(point);
    } else if (bezier.order == QUADRATIC) {
        float shortestDist = INFINITY;
        float sT = -1;

        Vec2f &P0 = points[bezier.pointsIdx[0]];
        Vec2f aux0 = point - P0;
        if (bezier.aux1.x == INFINITY) {
            Vec2f &P1 = points[bezier.pointsIdx[1]];
            Vec2f &P2 = points[bezier.pointsIdx[2]];

            bezier.aux1 = P1 - P0;
            bezier.aux2 = P2 - P1 * 2.0f + P0;
        }

        // cringe af generic cubic equation
        // ( aux2 · aux2)t^3 + 3( aux1 · aux2 )t^2 + (2aux1 · aux1 − aux2 · aux0)t − aux1 · aux0 = 0
        tResults.clear();
        int tCount = cubicSolver(tResults, bezier.aux2.dotProduct(bezier.aux2), bezier.aux1.dotProduct(bezier.aux2) * 3.0f, bezier.aux1.dotProduct(bezier.aux1 * 2.0f) - bezier.aux2.dotProduct(aux0), -bezier.aux1.dotProduct(aux0));
        if (tCount == 0) {
            printBeziers();
            printOutline();
            printSegments();
            std::cerr << bezier.pointsIdx[0] << " " << bezier.pointsIdx[1] << " " << bezier.pointsIdx[2] << "\n";
            std::wcerr << "char: " << (wchar_t)unicode << "unicode: 0x" << std::hex << unicode << std::dec << "\n";
        }
        assert(tCount != 0 && "t results cannot be empty");

        float dist;
        // Get the distances for every unique t
        for (auto &t : tResults) {
            if (t == sT) continue;
            if (std::isnan(t)) {
                std::cerr << "nan\n";
                std::cerr << "a: " << bezier.aux2.dotProduct(bezier.aux2) << " b: " << bezier.aux1.dotProduct(bezier.aux2) * 3.0f << " c: " << bezier.aux1.dotProduct(bezier.aux1) * 2.0f - bezier.aux2.dotProduct(aux0) << " d: " << -bezier.aux1.dotProduct(aux0) << "\n";
                for (size_t i = 0; i < tResults.size(); i++) {
                    std::cerr << " t" << i << ": " << tResults[i];
                }
                std::cerr << "\n";

                continue;
            }

            t = std::clamp(t, 0.0f, 1.0f);
            Vec2f p = pointAtTOnBezier(bezier, t);
            dist = p.distance(point);

            if (dist < shortestDist) {
                shortestDist = dist;
                sT = t;
            }
        }

        // if (std::find(tResults.cbegin(), tResults.cend(), 0.0f) != tResults.cend()) {
        //     Vec2f p = pointAtTOnBezier(bezier, 0.0f);
        //     dist = p.distance(point);
        //
        //     if (dist < shortestDist) {
        //         shortestDist = dist;
        //         sT = 0.0f;
        //     }
        // }
        // if (std::find(tResults.cbegin(), tResults.cend(), 1.0f) != tResults.cend()) {
        //     Vec2f p = pointAtTOnBezier(bezier, 1.0f);
        //     dist = p.distance(point);
        //
        //     if (dist < shortestDist) {
        //         shortestDist = dist;
        //         sT = 1.0f;
        //     }
        // }

        if (tOut != nullptr) *tOut = sT;
        return shortestDist;
    } else if (bezier.order == CUBIC) {
        Vec2f &P0 = points[bezier.pointsIdx[0]];
        // Vec2f aux0 = point - P0;
        if (bezier.aux1.x == INFINITY) {
            Vec2f &P1 = points[bezier.pointsIdx[1]];
            Vec2f &P2 = points[bezier.pointsIdx[2]];
            Vec2f &P3 = points[bezier.pointsIdx[3]];

            bezier.aux1 = P1 - P0;
            bezier.aux2 = P2 - P1 * 2.0f + P0;
            bezier.aux3 = P3 - P2 * 3.0f + P1 * 3.0f - P0;
        }

        assert(false && "not implemented");
    }

    assert(false && "unreachable");
}

float Outline::signOfDistance(const Vec2f &point, Bezier &bezier, const float &t) {
    // sign = dB/dt(t) x (B(t) - P)
    return derivativeOfBezier(bezier, t).crossProduct(pointAtTOnBezier(bezier, t) - point) < 0 ? -1.0f : 1.0f;
}

Bezier &Outline::findClosestBez(const Vec2f point, float *distOut = nullptr, float *tOut = nullptr) {
    // TODO: this should be changed so it can fail gracefully
    assert(curvesInContours.size() > 0 && "curvesInContours cannot be 0");
    Bezier *mBez = nullptr;
    float mDist = INFINITY;
    float mOrt = -INFINITY;
    float mT = -1;
    float t = -1;
    float ort = 0;

    std::vector<Bezier *> shortest;
    shortest.reserve(10);
    float smollest = INFINITY;
    float distance = 0;
    Vec2f pos1;
    Vec2f pos2;
    float dx = 0, dy = 0;
    float dist = 0;

    for (auto &beziers : curvesInContours) {
        for (auto &bez : beziers) {
            dx = 0;
            if (point.x < bez.box.minX)
                dx = bez.box.minX - point.x;
            else if (point.x > bez.box.maxX)
                dx = point.x - bez.box.maxX;

            dy = 0;
            if (point.y < bez.box.minY)
                dy = bez.box.minY - point.y;
            else if (point.y > bez.box.maxY)
                dy = point.y - bez.box.maxY;

            dist = dx * dx + dy * dy;
            if (dist <= smollest) {
                switch (bez.order) {
                    case LINEAR:
                        pos1 = points[bez.pointsIdx[0]];
                        pos2 = points[bez.pointsIdx[1]];
                        break;

                    case QUADRATIC: {
                        auto tmp0 = point.distanceCmp(points[bez.pointsIdx[0]]);
                        auto tmp1 = point.distanceCmp(points[bez.pointsIdx[1]]);
                        auto tmp2 = point.distanceCmp(points[bez.pointsIdx[2]]);

                        bool swapped = false;
                        if (tmp1 < tmp0) {
                            swapped = true;
                            pos1 = points[bez.pointsIdx[1]];
                        } else {
                            pos1 = points[bez.pointsIdx[0]];
                        }

                        if (swapped && tmp2 < tmp0)
                            pos2 = points[bez.pointsIdx[2]];
                        else if (swapped)
                            pos2 = points[bez.pointsIdx[0]];
                        else if (tmp2 < tmp1)
                            pos2 = points[bez.pointsIdx[2]];
                        else
                            pos2 = points[bez.pointsIdx[1]];

                        break;
                    }
                    case CUBIC:
                        assert(false && "not implemented");
                        break;

                    default:
                        break;
                }

                Vec2f aux0 = pos2 - pos1;
                if (pos1 == pos2) {
                    printBeziers();
                    printOutline();
                    printSegments();
                    std::cerr << bez.pointsIdx[0] << "\n";
                    std::cerr << bez.pointsIdx[1] << "\n";
                    std::wcerr << "char: " << (wchar_t)unicode << "unicode: 0x" << std::hex << unicode << std::dec << "\n";
                    assert(false && "two points in a bezier cannot be at same position!");
                }
                t = (point - pos1).dotProduct(aux0) / (aux0).dotProduct(aux0);
                t = std::clamp(t, 0.0f, 1.0f);
                distance = point.distanceCmp(pos1 + (aux0)*t);

                if (distance < smollest) {
                    smollest = distance;
                }
            }
        }
    }
    for (auto &beziers : curvesInContours) {
        for (auto &bez : beziers) {
            dx = 0;
            if (point.x < bez.box.minX)
                dx = bez.box.minX - point.x;
            else if (point.x > bez.box.maxX)
                dx = point.x - bez.box.maxX;

            dy = 0;
            if (point.y < bez.box.minY)
                dy = bez.box.minY - point.y;
            else if (point.y > bez.box.maxY)
                dy = point.y - bez.box.maxY;

            dist = dx * dx + dy * dy;
            if (dist <= smollest) {
                switch (bez.order) {
                    case LINEAR:
                        pos1 = points[bez.pointsIdx[0]];
                        pos2 = points[bez.pointsIdx[1]];
                        break;

                    case QUADRATIC: {
                        auto tmp0 = point.distanceCmp(points[bez.pointsIdx[0]]);
                        auto tmp1 = point.distanceCmp(points[bez.pointsIdx[1]]);
                        auto tmp2 = point.distanceCmp(points[bez.pointsIdx[2]]);

                        bool swapped = false;
                        if (tmp1 < tmp0) {
                            swapped = true;
                            pos1 = points[bez.pointsIdx[1]];
                        } else {
                            pos1 = points[bez.pointsIdx[0]];
                        }

                        if (swapped && tmp2 < tmp0)
                            pos2 = points[bez.pointsIdx[2]];
                        else if (swapped)
                            pos2 = points[bez.pointsIdx[0]];
                        else if (tmp2 < tmp1)
                            pos2 = points[bez.pointsIdx[2]];
                        else
                            pos2 = points[bez.pointsIdx[1]];
                        break;
                    }
                    case CUBIC:
                        assert(false && "not implemented");
                        break;

                    default:
                        break;
                }

                Vec2f aux0 = pos2 - pos1;
                if (pos1 == pos2) assert(false && "two points in a bezier cannot be at same position!");
                t = (point - pos1).dotProduct(aux0) / (aux0).dotProduct(aux0);
                t = std::clamp(t, 0.0f, 1.0f);
                distance = point.distanceCmp(pos1 + (aux0)*t);

                if (distance - smollest < 1e-10f) {
                    shortest.push_back(&bez);
                }
            }
        }
    }
    for (auto bez : shortest) {
        // testc++;
        float dist = shortestDistanceToBezier(point, *bez, &t);
        t = std::clamp(t, 0.0f, 1.0f);
        if (!(t <= 1.0f && t >= 0.0f)) {
            std::cerr << "1t: " << t << "\n";
        }
        if (dist < mDist) {
            ort = ortogonality(point, *bez, t);
            mBez = bez;
            mDist = dist;
            mOrt = ort;
            mT = t;
        } else if (fabs(dist - mDist) < 1e-6f) {
            ort = ortogonality(point, *bez, t);
            if (ort > mOrt) {
                mBez = bez;
                mDist = dist;
                mOrt = ort;
                mT = t;
            }
        }
    }

    if (distOut != nullptr) *distOut = mDist;
    if (tOut != nullptr) *tOut = mT;
    return *mBez;
};

Vec2f Outline::transformCoord(const float x, const float y) {
    return { x * (xMax + xPadding) + (xMin - (xPadding / 2)), (yMax + (yPadding / 2)) - y * ((yMax + yPadding) - (yMin - (yPadding / 2))) };
}

uint8_t Outline::distToColor(const float dist, const float maxDist) {
    // return (dist < maxDist) ? 255 : 0;
    return std::clamp((dist / (2 * maxDist) + 0.5f) * 255, 0.0f, 255.0f);
}

void Outline::createSDFBitmap(std::vector<uint8_t> &atlas, const uint32_t &atlasWidth, const uint32_t &atlasHeight, const uint32_t startX, const uint32_t startY, const uint16_t width, const uint16_t height) {
    for (float x = 0; x < width; x++) {
        for (float y = 0; y < height; y++) {
            Vec2f p = transformCoord((x + 0.5f) / width, (y + 0.5f) / height);  // +0.5 so it uses the middle of the pixel
            float t;
            float dist;
            Bezier &bez = findClosestBez(p, &dist, &t);
            t = std::clamp(t, 0.0f, 2.0f);
            dist *= signOfDistance(p, bez, t) / 8;

            uint8_t color = distToColor(dist, 128);
            uint32_t idx = ((y + startY) * atlasWidth + x + startX) * 4;
            // std::unordered_set<int> set = { 28, 29, 30 };
            // if (bez.pointsIdx.size() == 3 && set.contains(bez.pointsIdx[1])) {
            //     atlas[idx + 1] = 255;
            //     atlas[idx + 3] = 255;
            //     continue;
            // }
            if (color > 200) {
                atlas[idx + 0] = color;
                atlas[idx + 3] = 255;
                // std::cout << "(" << x << "," << y << ")\n";
                continue;
            }

            atlas[idx + 0] = color;
            atlas[idx + 1] = color;
            atlas[idx + 2] = color;
            atlas[idx + 3] = 255;
        }
    }
}

void generateMSDF(std::vector<uint8_t> &atlas, Outline outline, const uint32_t &atlasWidth, const uint32_t &atlasHeight, const uint32_t startX, const uint32_t startY, const uint32_t width = 64, const uint32_t height = 64, const int channel = 1) {
    // outline.printOutline();
    outline.sanitize();
    // std::cout << "generate MSDF\n";
    outline.addImpliedPoints();  // TODO: this can probably be moved to Outline Initialization
    outline.populateBounds();
    outline.populateBeziers();                        // TODO: this can probably be moved to Outline Initialization
    constexpr float maxDiff = std::numbers::pi / 18;  // 10 degrees
    outline.getSegments(maxDiff);

    // outline.printOutline();
    // outline.printBeziers();
    // outline.printSegments();
    // std::vector<Vec2f> points = { Vec2f(-1184.05f, 1558.5f), Vec2f(-1184.05f, 1461.5f) };
    // for (auto &point : points) {
    //     std::cout << "Point: (" << point.x << "," << point.y << ")\n";
    //
    //     float t = -1;
    //     float dist = INFINITY;
    //     Bezier &bez = outline.findClosestBez(point, &dist, &t);
    //     std::cout << "closest bezier: \n";
    //     std::cout << "pIdxs: ";
    //     for (auto &pIdx : bez.pointsIdx) {
    //         std::cout << pIdx << " ";
    //     }
    //     std::cout << "\n";
    //     std::cout << "bez order: " << bezierOrderToString(bez.order) << "\n";
    //     std::cout << "distance: " << dist << " t: " << t << "\n";
    //     std::cout << "sign: " << outline.signOfDistance(point, bez, t) << "\n";
    //
    //     std::cout << "der: " << outline.derivativeOfBezier(bez, t).toString() << "\n";
    //     std::cout << (outline.pointAtTOnBezier(bez, t) - point).toString() << "\n";
    //     auto a = outline.derivativeOfBezier(bez, t).crossProduct(point - outline.pointAtTOnBezier(bez, t));
    //     std::cout << a << "\n";
    //     float ort = outline.ortogonality(point, bez, t);
    //     std::cout << "ort: " << ort << "\n";
    //
    //     float tT;
    //     outline.shortestDistanceToBezier(point, bez, &tT);
    //     std::cout << "T: " << tT << "\n";
    // }
    //
    // Vec2f p = outline.transformCoord((0 + 0.5f) / width, (25 + 0.5f) / height);
    // Vec2f p1 = outline.transformCoord((0 + 0.5f) / width, (26 + 0.5f) / height);
    // std::cout << "pos: (" << p.x << "," << p.y << ")\n";
    // std::cout << "pos: (" << p1.x << "," << p1.y << ")\n";
    outline.createSDFBitmap(atlas, atlasWidth, atlasHeight, startX, startY, width, height);
}

void Font::packUnicodeRangeSDF(const uint32_t unicodeStart, const uint32_t unicodeEnd, const Style style, const TextDirection, const int maxCharPerPage, const bool autoPageSize, const uint16_t pSize) {
    if (!ftFace) return;

    FT_Set_Pixel_Sizes(ftFace, 0, 64);
    hb_face_t *hbFace = hb_ft_face_create_referenced(ftFace);

    std::vector<std::pair<unicode_t, hb_codepoint_t>> scriptCodepoints;

    FT_UInt glyphIndex;
    FT_ULong charcode = FT_Get_First_Char(ftFace, &glyphIndex);
    while (glyphIndex != 0) {
        if ((charcode >= unicodeStart && charcode <= unicodeEnd)) {
            scriptCodepoints.push_back({ charcode, glyphIndex });
            // std::wcout << "cp: " << glyphIndex << " code: " << (wchar_t)charcode << "\n";
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
        uint64_t maxChars = (static_cast<uint64_t>(maxPageSize) * static_cast<uint64_t>(maxPageSize)) / (64 * 64);
        charPerPage = std::min(static_cast<uint32_t>(scriptCodepoints.size()) + 1, static_cast<uint32_t>(std::round(maxChars)));
    }

    uint32_t padding = 4;

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
                FT_Load_Glyph(ftFace, cp, FT_LOAD_DEFAULT);
                // uint32_t w = ftFace->glyph->bitmap.width + padding;
                // uint32_t h = ftFace->glyph->bitmap.rows + padding;
                totalArea += (64 + padding) * (64 + padding);
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

        Page page(pageWidth, pageHeight, 64, style);
        std::vector<uint8_t> textureData(pageWidth * pageHeight * 4, 0);

        std::vector<stbrp_rect> rects;
        std::vector<std::pair<unicode_t, hb_codepoint_t>> validPageCodepoints;

        // TODO: change this to something more efficient
        for (size_t i = 0; i < glyphsToPack; ++i) {
            auto [unicode, cp] = scriptCodepoints[glyphsProcessed + i];
            FT_Load_Glyph(ftFace, cp, FT_LOAD_DEFAULT);

            if (ftFace->glyph->format != FT_GLYPH_FORMAT_OUTLINE) {
                badCounter++;
                continue;
            }

            stbrp_rect rect;
            rect.w = 64 + padding;
            rect.h = 64 + padding;
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

        int threadCount = 0;
        for (size_t i = 0; i < rects.size(); ++i) {
            if (!rects[i].was_packed) {
                failedPacks++;
                scriptCodepoints.push_back(validPageCodepoints[i]);
                continue;
            }
            threadCount++;
        }

        std::vector<std::jthread> threads;
        threads.reserve(threadCount);
        std::latch latch{ threadCount };

        // BLIT shit together
        for (size_t i = 0; i < rects.size(); ++i) {
            if (!rects[i].was_packed) {
                continue;
            }

            auto [unicode, cp] = validPageCodepoints[rects[i].id];
            FT_Load_Glyph(ftFace, cp, FT_LOAD_DEFAULT);

            FT_GlyphSlot slot = ftFace->glyph;

            // offset the boxes by padding
            int32_t glyphX = rects[i].x + (padding / 2);
            int32_t glyphY = rects[i].y + (padding / 2);

            if (ftFace->glyph->format != FT_GLYPH_FORMAT_OUTLINE) continue;

            auto &ol = ftFace->glyph->outline;
            uint16_t units = ftFace->units_per_EM;

            Outline outline{ ol, units, unicode };
            if (outline.numContours != 0 || outline.numPoints != 0) {
                threads.emplace_back([&, outline, glyphX, glyphY]() mutable {
                    generateMSDF(textureData, outline,
                                 pageWidth, pageHeight, glyphX, glyphY);
                    latch.count_down();
                });
            } else {
                latch.count_down();
            }
            glyphsAdded++;
            Glyph glyph(unicode, cp, glyphX, glyphY);
            glyph.w = 64;
            glyph.h = 64;
            glyph.bearingX = slot->metrics.horiBearingX;
            glyph.bearingY = slot->metrics.horiBearingY;
            glyph.advanceX = slot->metrics.horiAdvance;
            glyph.advanceY = slot->metrics.vertAdvance;

            // this shit sets the uvs
            page.addGlyph(glyph);
            pagePosition[64].insert({ cp, pages.size() });
        }

        latch.wait();

        // std::cout << "testc: " << testc << "\n";
        // std::cout << "test2c: " << test2c << "\n";
        // page.textureId = Render::CreateFontPage(textureData, pageWidth, pageHeight);
        char filename[20];
        sprintf(filename, "sdf%u.bmp", pageCount);
        FILE *fbmp = fopen(filename, "wb");
        saveAtlasAsBMP(fbmp, textureData, page.w, page.h);
        fclose(fbmp);

        pages.push_back(std::move(page));
        glyphsProcessed += glyphsToPack;
        pageCount++;
    }
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

                    if ((py == 0 || py == bmp.rows - 1) || px == 0 || px == bmp.width - 1) {
                        textureData[idx + 0] = 255;
                        textureData[idx + 1] = 255;
                        textureData[idx + 2] = 255;
                        textureData[idx + 3] = 255;

                    } else {
                        textureData[idx + 0] = 255;
                        textureData[idx + 1] = 255;
                        textureData[idx + 2] = 255;
                        textureData[idx + 3] = alpha;
                    }
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
        sprintf(filename, "goat%u.bmp", pageCount);
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
