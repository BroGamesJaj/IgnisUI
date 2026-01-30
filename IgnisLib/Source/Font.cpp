#include "Font.h"

// TODO:
//  creates bitmap struct? class?

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
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
    packUnicodeRangeSDF(0x0021, 0x007E);
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
struct Bezier {
    std::vector<uint16_t> pointsIdx;
    BezierOrder order = UNSET;
};

// one or multiple bezier curve indexes
struct Segment {
    uint16_t contourIdx;
    std::vector<uint16_t> bezierIdxs;
};
struct Outline {
    Outline(FT_Outline &ftOutline, const uint16_t &units) {
        unitsPerEM = units;

        flags = ftOutline.flags;
        numContours = ftOutline.n_contours;
        numPoints = ftOutline.n_points;

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
        for (auto &[contourIdx, curves] : curvesInContours) {
            std::cout << "contourIdx: " << contourIdx << "\n";
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

    uint16_t numContours;
    uint16_t numPoints;
    int flags;

    std::vector<Vec2f> points;        // length of numPoints
    std::vector<unsigned char> tags;  // length of numPoints
    std::vector<uint16_t> contours;   // length of numContours

    std::unordered_map<uint16_t, std::vector<Bezier>> curvesInContours;
    std::vector<Segment> segments;

    Vec2f pointAtTOnBezier(const Bezier &bez, const float t);
    Vec2f derivativeOfBezier(const Bezier &bez, const float t);
    float shortestDistanceToBezier(const Vec2f &point, const Bezier &bezier, float *tOut);
    float signOfDistance(const Vec2f &point, const Bezier &bezier, const float &t);
    float ortogonality(const Vec2f &point, const Bezier &bezier, const float t);

    void addImpliedPoints();
    void populateBounds();
    void populateBeziers();
    void getSegments(float acceptedAngleDeviation);
    Bezier &findClosestBez(const Vec2f point, float *distOut, float *t);

    Vec2f transformCoord(const float x, const float y);
    uint8_t distToColor(const float dist, const float maxDist);
    void createBitmap(std::vector<uint8_t> &bmp, const uint16_t width, const uint16_t height);
};

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
    std::cout << "\nBounds:\n";
    std::cout << "xMin: " << xMin << " xMax: " << xMax << "\n";
    std::cout << "yMin: " << yMin << " yMax: " << yMax << "\n";

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
        for (uint16_t pIdx = contourStart; pIdx <= contourEnd + 1; ++pIdx) {
            uint16_t idx = (pIdx == contourEnd + 1) ? contourStart : pIdx;
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
                curvesInContours[contIdx].push_back(bz);
                bz = {};
                bz.pointsIdx.push_back(idx);
            }
        }
    }
}

Vec2f Outline::pointAtTOnBezier(const Bezier &bez, const float t) {
    assert(t <= 1 && t >= 0 && "t must be between 0 and 1");
    if (bez.order == LINEAR) {
        Vec2f &P0 = points[bez.pointsIdx[0]];
        Vec2f &P1 = points[bez.pointsIdx[1]];

        // P0 + t(P1 - P0)
        return P0 + (P1 - P0) * t;
    } else if (bez.order == QUADRATIC) {
        Vec2f &P0 = points[bez.pointsIdx[0]];
        Vec2f &P1 = points[bez.pointsIdx[1]];
        Vec2f &P2 = points[bez.pointsIdx[2]];

        // P0 + 2t(P1 − P0) + t^2(P2 − 2P1 + P0)
        return P0 + (P1 - P0) * 2.0f * t + (P2 - P1 * 2.0f + P0) * t * t;
    } else if (bez.order == CUBIC) {
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
            return (P2 - P1) * 2.0f;
        else
            // 2t(P2 - 2P1 + P0) + 2(P1 - P0)
            return (P2 - P1 * 2.0f + P0) * 2 * t + (P1 - P0) * 2.0f;

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
                segments.push_back(seg);
                seg.bezierIdxs.clear();
            }
        }
        if (!seg.bezierIdxs.empty()) segments.push_back(seg);
    }
}

// Otrogonality matters for when two beziers are the same
// distance away from a point.
// in that case we need to maximize Ortogonality.
float Outline::ortogonality(const Vec2f &point, const Bezier &bezier, const float t) {
    Vec2f detNorm = derivativeOfBezier(bezier, t).normalize();
    Vec2f dispNorm = (point - pointAtTOnBezier(bezier, t)).normalize();

    // | ((dB(t)/dt)/||(dB(t)/dt)||) x ((P - B(t))/||P - B(t)||) |
    return fabs(detNorm.crossProduct(dispNorm));
}

// Classic quadratic
std::vector<float> quadraticSolver(const float a, const float b, const float c) {
    std::vector<float> roots;

    float discriminant = b * b - 4 * a * c;

    if (discriminant > 0) {
        roots.push_back((-b + sqrt(discriminant)) / (2 * a));
        roots.push_back((-b - sqrt(discriminant)) / (2 * a));
    } else if (discriminant == 0) {
        roots.push_back(-b / (2 * a));
    } else {
        // for our purposes the complex root is not needed
        assert(false && "only complex solution to quadratic");
    }
    return roots;
}

// Cardano's formula
std::vector<float> cubicSolver(const float a, const float b, const float c, const float d) {
    if (a == 0) return quadraticSolver(b, c, d);  // not a cubic
    std::vector<float> roots;

    // we need to depress this happy boy
    // ie.: make the quadratic part 0
    float p = (3.0f * a * c - b * b) / (3.0f * a * a);
    float q = (2.0f * b * b * b - 9.0f * a * b * c + 27.0f * a * a * d) / (27.0f * a * a * a);

    float discriminant = (q * q) / 4.0f + (p * p * p) / 27.0f;

    const float eps = 1e-12;

    // if discriminant is positive there is only one root
    if (discriminant > eps) {  // one real root
        float sqrt_disc = sqrt(discriminant);
        float u = cbrt(-q / 2.0f + sqrt_disc);
        float v = cbrt(-q / 2.0f - sqrt_disc);
        roots.push_back(u + v - b / (3.0f * a));

        // If discrimanant is negative it has 3 roots
        // and if its 0 it has two or 3, but
        // if thats the case we just get some duplicates
    } else {  // three real roots
        float r = sqrt(-p * p * p / 27.0f);
        float phi = acos(std::clamp(-q / (2.0f * r), -1.0f, 1.0f));
        float t = 2 * cbrt(r);
        roots.push_back(t * cos(phi / 3.0f) - b / (3.0f * a));
        roots.push_back(t * cos((phi + 2.0f * std::numbers::pi) / 3.0f) - b / (3.0f * a));
        roots.push_back(t * cos((phi + 4.0f * std::numbers::pi) / 3.0f) - b / (3.0f * a));
    }

    return roots;
}

float Outline::shortestDistanceToBezier(const Vec2f &point, const Bezier &bezier, float *tOut = nullptr) {
    if (bezier.order == LINEAR) {
        Vec2f &P0 = points[bezier.pointsIdx[0]];
        Vec2f &P1 = points[bezier.pointsIdx[1]];

        // TODO: write equality operator for Vec2
        // P = Point
        // t = [(P - P0) * (P1 - P0)] / [(P1 - P0 ) * ( P1 - P0)]
        float t = 0;
        if (P0 != P1)
            t = (point - P0).dotProduct(P1 - P0) / (P1 - P0).dotProduct(P1 - P0);
        t = std::clamp(t, 0.0f, 1.0f);
        Vec2f Pt = pointAtTOnBezier(bezier, t);
        if (tOut != nullptr) *tOut = t;
        return Pt.distance(point);
    } else if (bezier.order == QUADRATIC) {
        Vec2f &P0 = points[bezier.pointsIdx[0]];
        Vec2f &P1 = points[bezier.pointsIdx[1]];
        Vec2f &P2 = points[bezier.pointsIdx[2]];
        Vec2f aux0 = point - P0;
        // NOTE: these should be computed only once per bezier
        Vec2f aux1 = P1 - P0;
        Vec2f aux2 = P2 - P1 * 2.0f + P0;

        // cringe af generic cubic equation
        // ( aux2 · aux2)t^3 + 3( aux1 · aux2 )t^2 + (2aux1 · aux1 − aux2 · aux0)t − aux1 · aux0 = 0
        std::vector<float> tResults = cubicSolver(aux2.dotProduct(aux2), aux1.dotProduct(aux2) * 3.0f, aux1.dotProduct(aux1) * 2.0f - aux2.dotProduct(aux0), -aux1.dotProduct(aux0));
        assert(!tResults.empty() && "t results cannot be empty");

        std::unordered_map<float, float> dists;  // key: t, value: dist
        // Get the distances for every unique t
        for (auto &t : tResults) {
            if (dists.contains(t)) continue;
            t = std::clamp(t, 0.0f, 1.0f);
            Vec2f p = pointAtTOnBezier(bezier, t);
            dists[t] = p.distance(point);
        }
        if (!dists.contains(1.0f)) {
            Vec2f p = pointAtTOnBezier(bezier, 1.0f);
            dists[1.0f] = p.distance(point);
        }
        if (!dists.contains(0.0f)) {
            Vec2f p = pointAtTOnBezier(bezier, 0.0f);
            dists[0.0f] = p.distance(point);
        }

        // find the shortest distance
        float shortestDist = INFINITY;
        float sT = -1;
        for (auto &[t, dist] : dists) {
            if (dist < shortestDist) {
                shortestDist = dist;
                sT = t;
            }
        }
        if (tOut != nullptr) *tOut = sT;
        return shortestDist;
    } else if (bezier.order == CUBIC) {
        Vec2f &P0 = points[bezier.pointsIdx[0]];
        Vec2f &P1 = points[bezier.pointsIdx[1]];
        Vec2f &P2 = points[bezier.pointsIdx[2]];
        Vec2f &P3 = points[bezier.pointsIdx[3]];

        Vec2f aux0 = point - P0;
        // NOTE: these should be computed only once per bezier
        Vec2f aux1 = P1 - P0;
        Vec2f aux2 = P2 - P1 * 2.0f + P0;
        Vec2f aux3 = P3 - P2 * 3.0f + P1 * 3.0f - P0;

        assert(false && "not implemented");
    }

    assert(false && "unreachable");
}

float Outline::signOfDistance(const Vec2f &point, const Bezier &bezier, const float &t) {
    // sign = dB/dt(t) x (B(t) - P)
    return derivativeOfBezier(bezier, t).crossProduct(pointAtTOnBezier(bezier, t) - point) < 0 ? -1.0f : 1.0f;
}

Bezier &Outline::findClosestBez(const Vec2f point, float *distOut = nullptr, float *tOut = nullptr) {
    // TODO: this should be changed so it can fail gracefully
    assert(curvesInContours.size() > 0 && "curvesInContours cannot be 0");
    Bezier *mBez = nullptr;
    float mDist = INFINITY;
    float mOrt = -INFINITY;
    float t = -1;
    for (auto &[contIdx, beziers] : curvesInContours) {
        for (auto &bez : beziers) {
            float dist = shortestDistanceToBezier(point, bez, &t);
            float ort = ortogonality(point, bez, t);

            // TODO: remove
            // std::cout << "dist:" << dist << ", ort:" << ort << " t: " << t << "\n";
            // std::cout << "pIdxs: ";
            // for (auto &pIdx : bez.pointsIdx) {
            //     std::cout << pIdx << " ";
            // }
            // std::cout << "\n";

            if (fabs(dist - mDist) < 1e-6f && ort > mOrt) {
                mBez = &bez;
                mDist = dist;
                mOrt = ort;
            }
            if (dist < mDist) {
                mBez = &bez;
                mDist = dist;
                mOrt = ort;
            }
        }
    }
    if (distOut != nullptr) *distOut = signOfDistance(point, *mBez, t) * mDist;
    if (tOut != nullptr) *tOut = t;
    return *mBez;
};

Vec2f Outline::transformCoord(const float x, const float y) {
    float padding = 64.0f * 64 / 2;

    float xSize = xMax - xMin;
    float ySize = yMax - yMin;
    float xPadding = (xSize < ySize) ? (1 - (xSize / ySize)) * ySize + padding : padding;
    float yPadding = (ySize < xSize) ? (1 - (ySize / xSize)) * xSize + padding : padding;

    return { x * (xMax + xPadding) + (xMin - (xPadding / 2)), (yMax + (yPadding / 2)) - y * ((yMax + yPadding) - (yMin - (yPadding / 2))) };
}

uint8_t Outline::distToColor(const float dist, const float maxDist) {
    return std::clamp((dist / (2 * maxDist) + 0.5f) * 255, 0.0f, 255.0f);
}

void Outline::createBitmap(std::vector<uint8_t> &bmp, const uint16_t width = 64, const uint16_t height = 64) {
    bmp.resize(width * height * 4, 0);
    for (float x = 0; x < width; x++) {
        for (float y = 0; y < height; y++) {
            Vec2f p = transformCoord((x + 0.5f) / width, (y + 0.5f) / height);  // +0.5 so it uses the middle of the pixel
            float t;
            Bezier &bez = findClosestBez(p);
            float dist = shortestDistanceToBezier(p, bez, &t);
            dist *= signOfDistance(p, bez, t) / 8;

            uint8_t color = distToColor(dist, 100);
            uint32_t idx = (y * width + x) * 4;
            bmp[idx + 0] = color;
            bmp[idx + 1] = color;
            bmp[idx + 2] = color;
            bmp[idx + 3] = 255;
        }
    }
    char filename[20] = "precent.bmp";
    FILE *fbmp = fopen(filename, "wb");
    saveAtlasAsBMP(fbmp, bmp, width, height);
    fclose(fbmp);
}

void generateMSDF(std::vector<uint8_t> &bmp, Outline outline, int channel = 1) {
    // std::cout << "generate MSDF\n";
    outline.addImpliedPoints();  // TODO: this can probably be moved to Outline Initialization
    // outline.printOutline();
    outline.populateBounds();
    outline.populateBeziers();  // TODO: this can probably be moved to Outline Initialization
    // outline.printBeziers();
    constexpr float maxDiff = std::numbers::pi / 18;  // 10 degrees
    outline.getSegments(maxDiff);
    // outline.printSegments();
    // Vec2f point{ 1000.0f, 2500.0f };
    // std::cout << "Point: (" << point.x << "," << point.y << ")\n";
    //
    // float t = -1;
    // float dist = INFINITY;
    // Bezier &bez = outline.findClosestBez(point, &dist, &t);
    // std::cout << "closest bezier: \n";
    // std::cout << "pIdxs: ";
    // for (auto &pIdx : bez.pointsIdx) {
    //     std::cout << pIdx << " ";
    // }
    // std::cout << "\n";
    // std::cout << "bez order: " << bezierOrderToString(bez.order) << "\n";
    // std::cout << "distance: " << dist << " t: " << t << "\n";
    float ratio = 1;  //(outline.yMax - outline.yMin) / (outline.xMax - outline.xMin);
    // std::cout << "ratio: " << ratio << "\n";
    outline.createBitmap(bmp, 64, 64 * ratio);
}

void Font::packUnicodeRangeSDF(const uint32_t unicodeStart, const uint32_t unicodeEnd, const Style style, const TextDirection, const int maxCharPerPage, const bool autoPageSize, const uint16_t pSize) {
    if (!ftFace) return;

    FT_Set_Pixel_Sizes(ftFace, 0, 64);
    hb_face_t *hbFace = hb_ft_face_create_referenced(ftFace);

    std::vector<std::pair<unicode_t, hb_codepoint_t>> scriptCodepoints;

    FT_ULong shitToFind = 'u';
    FT_UInt glyphIndex;
    FT_ULong charcode = FT_Get_First_Char(ftFace, &glyphIndex);
    while (glyphIndex != 0) {
        if ((charcode >= unicodeStart && charcode <= unicodeEnd)) {
            scriptCodepoints.push_back({ charcode, glyphIndex });
        }
        if (charcode == shitToFind) {
            std::cout << "found\n";
            // EXPERIMENTAL
            FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_DEFAULT);
            if (ftFace->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
                std::cout << "outline\n";
                std::cout << (char)shitToFind << "\n";
                auto &ol = ftFace->glyph->outline;
                std::cout << "x ppem: " << ftFace->size->metrics.x_ppem;
                std::cout << "x scale: " << ftFace->size->metrics.x_scale;
                std::cout << "y ppem: " << ftFace->size->metrics.y_ppem;
                std::cout << "y scale: " << ftFace->size->metrics.y_scale;
                uint16_t units = ftFace->units_per_EM;
                std::cout << "units: " << units << "\n";
                Outline outline(ol, units);
                std::vector<uint8_t> bitmap;
                generateMSDF(bitmap, outline);
            };
            // EXPERIMENTAL END
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
        // BLIT shit together
        for (size_t i = 0; i < rects.size(); ++i) {
            if (!rects[i].was_packed) {
                failedPacks++;
                scriptCodepoints.push_back(validPageCodepoints[i]);
                continue;
            }

            auto [unicode, cp] = validPageCodepoints[rects[i].id];
            FT_Load_Glyph(ftFace, cp, FT_LOAD_DEFAULT);

            FT_GlyphSlot slot = ftFace->glyph;

            // offset the boxes by padding
            int32_t glyph_x = rects[i].x + (padding / 2);
            int32_t glyph_y = rects[i].y + (padding / 2);

            FT_Load_Glyph(ftFace, cp, FT_LOAD_DEFAULT);

            std::vector<uint8_t> bitmap;
            if (ftFace->glyph->format == FT_GLYPH_FORMAT_OUTLINE) {
                auto &ol = ftFace->glyph->outline;
                uint16_t units = ftFace->units_per_EM;
                std::cout << "unitsPerEM: " << units << "\n";
                Outline outline(ol, units);
                generateMSDF(bitmap, outline);
            };

            if (bitmap.empty()) {
                std::cout << "why\n";
            }
            std::cout << "elp\n";

            for (uint16_t py = 0; py < 64; ++py) {
                for (uint16_t px = 0; px < 64; ++px) {
                    int x = glyph_x + px;
                    int y = glyph_y + py;
                    if ((unsigned)x >= pageWidth || (unsigned)y >= pageHeight) {
                        continue;
                    }

                    uint32_t bmpIdx = (py * 64 + px) * 4;
                    uint32_t idx = (y * pageWidth + x) * 4;
                    textureData[idx + 0] = bitmap[bmpIdx];
                    textureData[idx + 1] = bitmap[bmpIdx + 1];
                    textureData[idx + 2] = bitmap[bmpIdx + 2];
                    textureData[idx + 3] = bitmap[bmpIdx + 3];
                }
            }
            glyphsAdded++;
            Glyph glyph(unicode, cp, glyph_x, glyph_y);
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
