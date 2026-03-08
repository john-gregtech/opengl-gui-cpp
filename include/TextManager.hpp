#pragma once
#include <glad/gl.h>
#include <array>
#include <string>
#include <memory>
#include <vector>
#include "RenderManager.hpp"

struct Character {
    Vec2 uvMin;      // Top-left UV in atlas
    Vec2 uvMax;      // Bottom-right UV in atlas
    Vec2 Size;       // Size of glyph
    Vec2 Bearing;    // Offset from baseline to left/top of glyph
    unsigned int Advance; // Offset to advance to next glyph
};

class TextManager {
public:
    TextManager(const std::string& fontPath, unsigned int fontSize = 48);
    ~TextManager();

    void renderText(const std::string& text, Vec2 pos, float scale, Color color, 
                    HorizontalAlignment hAlign = HorizontalAlignment::Left, 
                    VerticalAlignment vAlign = VerticalAlignment::Top,
                    Vec2 boxSize = {0,0});
    
    Vec2 getTextSize(const std::string& text, float scale);
    
    Vec2 getCursorCoords(const std::string& text, size_t cursorIndex, Vec2 pos, float scale,
                         HorizontalAlignment hAlign = HorizontalAlignment::Left,
                         VerticalAlignment vAlign = VerticalAlignment::Top,
                         Vec2 boxSize = {0,0});

    size_t getCursorIndexFromCoords(const std::string& text, Vec2 localMousePos, float scale, float maxWidth);

    float getLineHeight() const { return lineHeight; }

    std::vector<std::string> wrapText(const std::string& text, float maxWidth, float scale);

private:
    std::array<Character, 128> Characters;
    unsigned int vao, vbo;
    unsigned int shaderProgram;
    unsigned int atlasTexture;
    float lineHeight;
    
    // Cached Uniforms
    int textColorLoc = -1;
    int projectionLoc = -1;
    int samplerLoc = -1;

    void setupShaders();
    void checkShaderError(unsigned int shader, std::string type);
};
