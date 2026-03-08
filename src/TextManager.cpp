#include "TextManager.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <iostream>
#include <GLFW/glfw3.h>
#include <vector>
#include <sstream>

const char* textVertexShader = R"(
#version 330 core
layout (location = 0) in vec4 vertex; 
out vec2 TexCoords;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
})";

const char* textFragmentShader = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec3 textColor;
void main() {    
    float alpha = texture(text, TexCoords).r;
    if (alpha <= 0.0) discard;
    color = vec4(textColor, alpha);
})";

TextManager::TextManager(const std::string& fontPath, unsigned int fontSize) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return;
    }
    FT_Face face;
    if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font: " << fontPath << std::endl;
        FT_Done_FreeType(ft);
        return;
    }
    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // 1. Create Glyph Atlas
    const int atlasSize = 1024;
    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlasSize, atlasSize, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int offsetX = 0;
    int offsetY = 0;
    int rowHeight = 0;

    for (unsigned char c = 0; c < 128; c++) {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;

        if (offsetX + face->glyph->bitmap.width + 1 >= atlasSize) {
            offsetX = 0;
            offsetY += rowHeight + 1;
            rowHeight = 0;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, offsetX, offsetY, 
                        face->glyph->bitmap.width, face->glyph->bitmap.rows, 
                        GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

        Characters[c] = {
            {(float)offsetX / (float)atlasSize, (float)offsetY / (float)atlasSize},
            {(float)(offsetX + face->glyph->bitmap.width) / (float)atlasSize, (float)(offsetY + face->glyph->bitmap.rows) / (float)atlasSize},
            {(float)face->glyph->bitmap.width, (float)face->glyph->bitmap.rows},
            {(float)face->glyph->bitmap_left, (float)face->glyph->bitmap_top},
            (unsigned int)face->glyph->advance.x
        };

        rowHeight = std::max(rowHeight, (int)face->glyph->bitmap.rows);
        offsetX += face->glyph->bitmap.width + 1;
    }

    lineHeight = (float)face->size->metrics.height / 64.0f;
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // 2. Setup Shaders and Buffers
    setupShaders();

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    // Pre-allocate buffer for 2000 glyphs (6 vertices * 4 floats each)
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4 * 2000, nullptr, GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    
    glBindVertexArray(0);
}

TextManager::~TextManager() {
    glDeleteTextures(1, &atlasTexture);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shaderProgram);
}

void TextManager::setupShaders() {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &textVertexShader, NULL);
    glCompileShader(vs);
    checkShaderError(vs, "VS");

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &textFragmentShader, NULL);
    glCompileShader(fs);
    checkShaderError(fs, "FS");

    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);
    checkShaderError(shaderProgram, "PROG");

    glDeleteShader(vs);
    glDeleteShader(fs);

    // Cache Uniforms
    textColorLoc = glGetUniformLocation(shaderProgram, "textColor");
    projectionLoc = glGetUniformLocation(shaderProgram, "uProjection");
    samplerLoc = glGetUniformLocation(shaderProgram, "text");
}

void TextManager::checkShaderError(unsigned int s, std::string t) {
    int ok; char log[1024];
    if (t != "PROG") {
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) { glGetShaderInfoLog(s, 1024, NULL, log); std::cerr << "Text Shader ERR " << t << ": " << log << std::endl; }
    } else {
        glGetProgramiv(s, GL_LINK_STATUS, &ok);
        if (!ok) { glGetProgramInfoLog(s, 1024, NULL, log); std::cerr << "Text Program ERR: " << log << std::endl; }
    }
}

std::vector<std::string> TextManager::wrapText(const std::string& text, float maxWidth, float scale) {
    std::vector<std::string> lines;
    if (maxWidth <= 0) {
        std::string current;
        for (char c : text) {
            if (c == '\n') { lines.push_back(current); current = ""; }
            else current += c;
        }
        lines.push_back(current);
        return lines;
    }

    std::string currentLine;
    float currentW = 0;
    std::string currentWord;
    float wordW = 0;

    for (char c : text) {
        if (c == '\n') {
            lines.push_back(currentLine + currentWord);
            currentLine = ""; currentWord = "";
            currentW = 0; wordW = 0;
            continue;
        }

        float charW = (Characters[(unsigned char)c].Advance >> 6) * scale;

        if (c == ' ') {
            currentLine += currentWord + " ";
            currentW += wordW + charW;
            currentWord = "";
            wordW = 0;
        } else {
            if (currentW + wordW + charW > maxWidth) {
                if (currentLine.empty()) {
                    lines.push_back(currentWord);
                    currentWord = c;
                    wordW = charW;
                } else {
                    lines.push_back(currentLine);
                    currentLine = "";
                    currentW = 0;
                    currentWord += c;
                    wordW = charW;
                }
            } else {
                currentWord += c;
                wordW += charW;
            }
        }
    }
    lines.push_back(currentLine + currentWord);
    if (text.empty()) lines[0] = "";
    return lines;
}

size_t TextManager::getCursorIndexFromCoords(const std::string& text, Vec2 localMousePos, float scale, float maxWidth) {
    std::vector<std::string> lines = wrapText(text, maxWidth, scale);
    float unitH = lineHeight * scale;
    int lineIdx = (int)(localMousePos.y / unitH);
    if (lineIdx < 0) lineIdx = 0;
    if (lineIdx >= (int)lines.size()) lineIdx = (int)lines.size() - 1;

    size_t globalIdx = 0;
    for (int i = 0; i < lineIdx; ++i) globalIdx += lines[i].length() + 1;

    const std::string& targetLine = lines[lineIdx];
    float currentX = 0;
    size_t lineCharIdx = 0;
    for (char c : targetLine) {
        float advance = (Characters[(unsigned char)c].Advance >> 6) * scale;
        if (currentX + advance * 0.5f > localMousePos.x) break;
        currentX += advance;
        lineCharIdx++;
    }
    return globalIdx + lineCharIdx;
}

Vec2 TextManager::getTextSize(const std::string& text, float scale) {
    float totalW = 0;
    std::vector<std::string> lines = wrapText(text, 0, scale); 
    for (const auto& l : lines) {
        float lw = 0;
        for (char c : l) lw += (Characters[(unsigned char)c].Advance >> 6) * scale;
        totalW = std::max(totalW, lw);
    }
    return {totalW, (float)lines.size() * lineHeight * scale};
}

Vec2 TextManager::getCursorCoords(const std::string& text, size_t cursorIndex, Vec2 pos, float scale,
                                HorizontalAlignment hAlign, VerticalAlignment vAlign, Vec2 boxSize) {
    std::vector<std::string> lines = wrapText(text, boxSize.x, scale);
    float unitH = lineHeight * scale;
    float totalH = lines.size() * unitH;
    float startY = pos.y;
    
    if (boxSize.y > 0) {
        if (vAlign == VerticalAlignment::Center) startY = pos.y + (boxSize.y - totalH) * 0.5f;
        else if (vAlign == VerticalAlignment::Bottom) startY = pos.y + (boxSize.y - totalH);
    }

    size_t curIdx = 0;
    float curY = startY;
    for (const auto& l : lines) {
        if (cursorIndex >= curIdx && cursorIndex <= curIdx + l.length()) {
            float curX = pos.x;
            if (boxSize.x > 0) {
                float lw = 0;
                for(char c : l) lw += (Characters[(unsigned char)c].Advance >> 6) * scale;
                if (hAlign == HorizontalAlignment::Center) curX = pos.x + (boxSize.x - lw) * 0.5f;
                else if (hAlign == HorizontalAlignment::Right) curX = pos.x + (boxSize.x - lw);
            }
            float xOff = 0;
            std::string sub = l.substr(0, cursorIndex - curIdx);
            for(char c : sub) xOff += (Characters[(unsigned char)c].Advance >> 6) * scale;
            return {curX + xOff, curY};
        }
        curIdx += l.length() + 1;
        curY += unitH;
    }
    return {pos.x, curY};
}

void TextManager::renderText(const std::string& text, Vec2 pos, float scale, Color color, 
                             HorizontalAlignment hAlign, VerticalAlignment vAlign, Vec2 boxSize) {
    if (text.empty()) return;

    glUseProgram(shaderProgram);
    glUniform3f(textColorLoc, color.r, color.g, color.b);
    const Mat4& proj = RenderManager::getInstance().getProjection();
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, proj.data.data());
    glUniform1i(samplerLoc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glBindVertexArray(vao);

    std::vector<std::string> lines = wrapText(text, boxSize.x, scale);
    float unitH = lineHeight * scale;
    float totalH = lines.size() * unitH;
    float startY = pos.y;
    
    if (boxSize.y > 0) {
        if (vAlign == VerticalAlignment::Center) startY = pos.y + (boxSize.y - totalH) * 0.5f;
        else if (vAlign == VerticalAlignment::Bottom) startY = pos.y + (boxSize.y - totalH);
    }

    std::vector<float> vertices;
    vertices.reserve(text.length() * 24);

    float curY = startY;
    for (const auto& l : lines) {
        float curX = pos.x;
        if (boxSize.x > 0) {
            float lw = 0;
            for(char c : l) lw += (Characters[(unsigned char)c].Advance >> 6) * scale;
            if (hAlign == HorizontalAlignment::Center) curX = pos.x + (boxSize.x - lw) * 0.5f;
            else if (hAlign == HorizontalAlignment::Right) curX = pos.x + (boxSize.x - lw);
        }

        float x = curX;
        for (char c : l) {
            const Character& ch = Characters[(unsigned char)c];
            float xpos = x + ch.Bearing.x * scale;
            float ypos = curY + (lineHeight - ch.Bearing.y) * scale;
            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            // Tri 1
            vertices.push_back(xpos);     vertices.push_back(ypos);     vertices.push_back(ch.uvMin.x); vertices.push_back(ch.uvMin.y);
            vertices.push_back(xpos);     vertices.push_back(ypos + h); vertices.push_back(ch.uvMin.x); vertices.push_back(ch.uvMax.y);
            vertices.push_back(xpos + w); vertices.push_back(ypos + h); vertices.push_back(ch.uvMax.x); vertices.push_back(ch.uvMax.y);
            
            // Tri 2
            vertices.push_back(xpos);     vertices.push_back(ypos);     vertices.push_back(ch.uvMin.x); vertices.push_back(ch.uvMin.y);
            vertices.push_back(xpos + w); vertices.push_back(ypos + h); vertices.push_back(ch.uvMax.x); vertices.push_back(ch.uvMax.y);
            vertices.push_back(xpos + w); vertices.push_back(ypos);     vertices.push_back(ch.uvMax.x); vertices.push_back(ch.uvMin.y);

            x += (ch.Advance >> 6) * scale;
        }
        curY += unitH;
    }

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(vertices.size() / 4));

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}
