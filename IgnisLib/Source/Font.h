#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "IgnisLib.h"
using pageNum_t = size_t;
using fontSize_t = uint16_t;
using hb_position_t = int32_t;
using hb_codepoint_t = uint32_t;
struct hb_buffer_t;
struct hb_font_t;

struct FT_LibraryRec_;
using FT_Library = FT_LibraryRec_ *;
struct FT_FaceRec_;
using FT_Face = FT_FaceRec_ *;
struct FT_Bitmap_;
using FT_Bitmap = FT_Bitmap_;
struct stbrp_rect;

namespace Ignis {
namespace Font {

class Font;

std::u32string sToU32s(const std::string_view &utf8);

struct Glyph {
    Glyph(uint32_t codepoint, uint16_t _x, uint16_t _y) : glyphIndex(codepoint), x(_x), y(_y) {}
    Glyph() = default;
    stbrp_rect toRect();
    const hb_codepoint_t getIdx() const noexcept { return glyphIndex; }

    hb_codepoint_t glyphIndex;

    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    int advance;

    float u0, u1;
    float v0, v1;
};

struct ShapedGlyph {
    ShapedGlyph(const Glyph *g, const uint32_t pageId, const int xOff, const int yOff, const int xAdv, const int yAdv, const uint32_t clustering) : glyph(g), pId(pageId), xOffset(xOff), yOffset(yOff), xAdvance(xAdv), yAdvance(xAdv), cluster(clustering) {};

    const Glyph *glyph;
    uint32_t pId;
    int xOffset, yOffset;
    int xAdvance, yAdvance;
    uint32_t cluster;
};

class Page {
   public:
    Page(uint16_t width, uint16_t height, uint16_t fSize, Style s) : w(width), h(height), fontSize(fSize), style(s) {};

    void addGlyph(Glyph &glyph);
    void addBitmap(FT_Bitmap &bmp);

    // private:
    int textureId;
    uint16_t fontSize;
    Style style;
    uint16_t w;
    uint16_t h;
    uint32_t gS, gE;
    std::unordered_map<hb_codepoint_t, Glyph> glyphs;
};

/*
struct TextFontData {
    TextFontData(const std::string text, const Font *fnt, TextAlign textAlign = LEFT, TextDirection textDirection = LTR) : string(text), font(fnt), align(textAlign), direction(textDirection){};

    TextFontData(const Font *fnt, TextAlign textAlign = LEFT, TextDirection textDirection = LTR) : font(fnt), align(textAlign), direction(textDirection){};

    void setString(std::string s) noexcept { string = s;};

    std::string string;
    const Font *font;

    struct Bounds{
        int x,y;
    };
    uint size{16};
    Bounds bounds;
    TextDirection direction{LTR};
    TextAlign align{LEFT};
    // Script script{LATIN};
    // Style style{REGULAR};
    // Color fill;
    // Color outline;
    int outlineThickness{-1};
    // std::vector<int> clustering;
    // std::vector<ShapedGlyph> shapedGlyphs;

    bool changed{true};
};
*/

class Font {
   public:
    ~Font();

    void preloadPageByRange(const uint32_t UnicodeStart, const uint32_t UnicodeEnd);
    void initializeFont(const std::string filename);

    void createBitmapFromText(const std::string text);

    std::vector<ShapedGlyph> shapeText(const std::u32string &text, const float x, const float y, const int FontSize = -1, TextAlign align = TextAlign::GUESS, TextDirection direction = TextDirection::GUESS, const Style style = Style::REGULAR);
    void generateTextVertecies(const std::string text, const float x, const float y, const TextAlign align = TextAlign::LEFT, const Style style = Style::REGULAR, const int fontSize = -1, const TextDirection direction = TextDirection::GUESS);

    // dont set maxCharPerPage and autoPageSize if you want behaviour to be optimized
    // not autoPageSize is not implemented
    //
    void packUnicodeRange(const uint32_t UnicodeStart, const uint32_t UnicodeEnd, int16_t fontSize = -1, const Style style = Style::REGULAR, const TextDirection direction = TextDirection::GUESS, const int maxCharPerPage = -1, const bool autoPageSize = true, const uint16_t pSize = 64);

    //   private:
    Render *renderer;
    uint defaultSize = 16;

    hb_buffer_t *buf;

    FT_Library lib{};
    FT_Face ftFace{};

    std::vector<Page> pages;

    // The fontSize thing is only needed until sdf or msdf is set up
    std::unordered_map<fontSize_t, std::unordered_map<hb_codepoint_t, pageNum_t>> pagePosition;

    void cleanup();
};

}  // namespace Font
}  // namespace Ignis
