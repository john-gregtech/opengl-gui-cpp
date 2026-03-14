#include "RenderManager.hpp"
#include "InputManager.hpp"
#include "TextManager.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <vector>
#include <stb_image.h>

void Triangle::setPosVec(int x[3], int y[3]) {
    this->p1.x = (float)x[0]; this->p1.y = (float)y[0];
    this->p2.x = (float)x[1]; this->p2.y = (float)y[1];
    this->p3.x = (float)x[2]; this->p3.y = (float)y[2];
}

const char* sdfRectVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 LocalPos;
uniform mat4 uProjection;
uniform vec2 uPos;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    LocalPos = aPos - uPos;
})";

const char* sdfRectFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 LocalPos;
uniform vec2 uSize;
uniform float uRadius;
uniform vec4 uBgColor;
uniform vec4 uBorderColor;
uniform float uBorderThickness;

float sdRoundedBox(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    vec2 p = LocalPos - uSize * 0.5;
    float dist = sdRoundedBox(p, uSize * 0.5, uRadius);
    
    float smoothing = 1.0; 
    float fillAlpha = 1.0 - smoothstep(-smoothing, smoothing, dist);
    float borderDist = abs(dist + uBorderThickness * 0.5) - uBorderThickness * 0.5;
    float borderAlpha = 1.0 - smoothstep(-smoothing, smoothing, borderDist);
    
    vec4 color = uBgColor;
    color.a *= fillAlpha;
    if (uBorderThickness > 0.0) {
        vec4 bColor = uBorderColor;
        bColor.a *= borderAlpha;
        color = mix(color, bColor, borderAlpha);
    }
    if (color.a <= 0.0) discard;
    FragColor = color;
})";

const char* textureVertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
})";

const char* textureFragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D uTexture;
void main() {
    FragColor = texture(uTexture, TexCoord);
})";

const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
})";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec4 uColor;
void main() {
    FragColor = uColor;
})";

void RenderManager::init() {
    shaderProgram = createShader(vertexShaderSource, fragmentShaderSource);
    textureShaderProgram = createShader(textureVertexShaderSource, textureFragmentShaderSource);
    sdfRectShaderProgram = createShader(sdfRectVertexShaderSource, sdfRectFragmentShaderSource);
    setupUniforms();
    glGenVertexArrays(1, &vao); 
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao); 
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glGenVertexArrays(1, &textureVao); 
    glGenBuffers(1, &textureVbo);
    glBindVertexArray(textureVao);
    glBindBuffer(GL_ARRAY_BUFFER, textureVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0); 
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float))); 
    glEnableVertexAttribArray(1);
    glEnable(GL_BLEND); 
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindVertexArray(0);
}

void RenderManager::setupUniforms() {
    sdfRectUniforms.projection = glGetUniformLocation(sdfRectShaderProgram, "uProjection");
    sdfRectUniforms.pos = glGetUniformLocation(sdfRectShaderProgram, "uPos");
    sdfRectUniforms.size = glGetUniformLocation(sdfRectShaderProgram, "uSize");
    sdfRectUniforms.radius = glGetUniformLocation(sdfRectShaderProgram, "uRadius");
    sdfRectUniforms.bgColor = glGetUniformLocation(sdfRectShaderProgram, "uBgColor");
    sdfRectUniforms.borderColor = glGetUniformLocation(sdfRectShaderProgram, "uBorderColor");
    sdfRectUniforms.borderThickness = glGetUniformLocation(sdfRectShaderProgram, "uBorderThickness");
    textureUniforms.projection = glGetUniformLocation(textureShaderProgram, "uProjection");
    textureUniforms.textureSampler = glGetUniformLocation(textureShaderProgram, "uTexture");
    basicUniforms.projection = glGetUniformLocation(shaderProgram, "uProjection");
    basicUniforms.color = glGetUniformLocation(shaderProgram, "uColor");
}

void RenderManager::clear(Color color) {
    glClearColor(color.r, color.g, color.b, color.a); 
    glClear(GL_COLOR_BUFFER_BIT);
}

void RenderManager::updateMatrices(int width, int height) {
    projection = Mat4::Ortho(0, (float)width, (float)height, 0);
    screenWidth = width;
    screenHeight = height;
}

static Vec2 transformPosInternal(Vec2 pos, CoordinateSystem sys, int sw, int sh) {
    if (sys == CoordinateSystem::NDC) return { (pos.x + 1.0f) * 0.5f * (float)sw, (1.0f - pos.y) * 0.5f * (float)sh };
    return pos;
}

static Vec2 transformSizeInternal(Vec2 size, CoordinateSystem sys, int sw, int sh) {
    if (sys == CoordinateSystem::NDC) return { size.x * 0.5f * (float)sw, size.y * 0.5f * (float)sh };
    return size;
}

void RenderManager::drawRect(const PrimitiveConfig& config) {
    Vec2 p = transformPosInternal(config.pos, config.coordSystem, screenWidth, screenHeight);
    Vec2 s = transformSizeInternal(config.size, config.coordSystem, screenWidth, screenHeight);
    float px = p.x; float py = p.y;
    if (config.posMode == PositionMode::Static) { 
        px += scrollOffset.x; 
        py += scrollOffset.y; 
    }
    float w = s.x; 
    float h = s.y;
    float vertices[] = { px, py, px+w, py, px+w, py+h, px, py, px+w, py+h, px, py+h };
    glUseProgram(sdfRectShaderProgram);
    glUniformMatrix4fv(sdfRectUniforms.projection, 1, GL_FALSE, projection.data.data());
    glUniform2f(sdfRectUniforms.pos, px, py);
    glUniform2f(sdfRectUniforms.size, w, h);
    glUniform1f(sdfRectUniforms.radius, config.borderRadius);
    glUniform4f(sdfRectUniforms.bgColor, config.backgroundColor.r, config.backgroundColor.g, config.backgroundColor.b, config.backgroundColor.a);
    glUniform4f(sdfRectUniforms.borderColor, config.borderColor.r, config.borderColor.g, config.borderColor.b, config.borderColor.a);
    glUniform1f(sdfRectUniforms.borderThickness, config.borderThickness);
    glBindVertexArray(vao); glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6); glBindVertexArray(0);
}

void RenderManager::drawCircle(const PrimitiveConfig& config, float radius, int segments) {
    PrimitiveConfig c = config; 
    c.coordSystem = CoordinateSystem::Pixels;
    Vec2 p = transformPosInternal(config.pos, config.coordSystem, screenWidth, screenHeight);
    c.pos = {p.x - radius, p.y - radius};
    c.size = {radius*2, radius*2};
    c.borderRadius = radius; 
    drawRect(c);
}

void RenderManager::drawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color, PositionMode mode) {
    Vec2 tp1 = transformPosInternal(p1, CoordinateSystem::Pixels, screenWidth, screenHeight);
    Vec2 tp2 = transformPosInternal(p2, CoordinateSystem::Pixels, screenWidth, screenHeight);
    Vec2 tp3 = transformPosInternal(p3, CoordinateSystem::Pixels, screenWidth, screenHeight);
    
    float x1 = tp1.x, y1 = tp1.y, x2 = tp2.x, y2 = tp2.y, x3 = tp3.x, y3 = tp3.y;
    if (mode == PositionMode::Static) { 
        x1 += scrollOffset.x; 
        y1 += scrollOffset.y; 
        x2 += scrollOffset.x; 
        y2 += scrollOffset.y; 
        x3 += scrollOffset.x; 
        y3 += scrollOffset.y; 
    }
    float vertices[] = { x1, y1, x2, y2, x3, y3 };
    glUseProgram(shaderProgram); 
    glUniformMatrix4fv(basicUniforms.projection, 1, GL_FALSE, projection.data.data());
    glUniform4f(basicUniforms.color, color.r, color.g, color.b, color.a);
    glBindVertexArray(vao); 
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 3); 
    glBindVertexArray(0);
}

void RenderManager::drawText(
    TextManager* tm, 
    const std::string& text, 
    Vec2 pos, float scale, 
    Color color, 
    HorizontalAlignment hAlign, 
    VerticalAlignment vAlign, 
    Vec2 boxSize, 
    PositionMode mode
) {
    if (tm) {
        Vec2 fp = transformPosInternal(pos, CoordinateSystem::Pixels, screenWidth, screenHeight); 
        Vec2 fs = transformSizeInternal(boxSize, CoordinateSystem::Pixels, screenWidth, screenHeight);
        if (mode == PositionMode::Static) { 
            fp.x += scrollOffset.x; 
            fp.y += scrollOffset.y; 
        }
        tm->renderText(text, fp, scale, color, hAlign, vAlign, fs);
    }
}

void RenderManager::drawImage(unsigned int textureID, Vec2 pos, Vec2 size, PositionMode mode) {
    Vec2 p = transformPosInternal(pos, CoordinateSystem::Pixels, screenWidth, screenHeight);
    Vec2 s = transformSizeInternal(size, CoordinateSystem::Pixels, screenWidth, screenHeight);
    float px = p.x; 
    float py = p.y; 
    if (mode == PositionMode::Static) { 
        px += scrollOffset.x; 
        py += scrollOffset.y; 
    }

    float w = s.x, h = s.y;
    float vertices[] = { px, py, 0, 0, px+w, py, 1, 0, px+w, py+h, 1, 1, px, py, 0, 0, px+w, py+h, 1, 1, px, py+h, 0, 1 };
    glUseProgram(textureShaderProgram); 
    glUniformMatrix4fv(textureUniforms.projection, 1, GL_FALSE, projection.data.data());
    glUniform1i(textureUniforms.textureSampler, 0);
    glBindVertexArray(textureVao); 
    glBindBuffer(GL_ARRAY_BUFFER, textureVbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindTexture(GL_TEXTURE_2D, textureID); 
    glDrawArrays(GL_TRIANGLES, 0, 6); 
    glBindTexture(GL_TEXTURE_2D, 0); 
    glBindVertexArray(0);
}

unsigned int RenderManager::loadTexture(const char* path) {
    unsigned int id; 
    glGenTextures(1, &id); 
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    int w, h, c; 
    stbi_set_flip_vertically_on_load(true);
    unsigned char* d = stbi_load(path, &w, &h, &c, 0);
    if (d) { 
        GLenum f = (c == 4) ? GL_RGBA : GL_RGB; 
        glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, d); 
        glGenerateMipmap(GL_TEXTURE_2D); stbi_image_free(d); 
    }
    return id;
}

unsigned int RenderManager::createShader(const char* v, const char* f) {
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER); 
    glShaderSource(vs, 1, &v, NULL); 
    glCompileShader(vs); 
    checkShaderError(vs, "VS");
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER); 
    glShaderSource(fs, 1, &f, NULL); 
    glCompileShader(fs); 
    checkShaderError(fs, "FS");
    unsigned int p = glCreateProgram(); 
    glAttachShader(p, vs); 
    glAttachShader(p, fs); 
    glLinkProgram(p); 
    checkShaderError(p, "PROG");
    glDeleteShader(vs); 
    glDeleteShader(fs); 
    return p;
}

void RenderManager::checkShaderError(unsigned int s, std::string t) {
    int ok; char log[1024];
    if (t != "PROG") { 
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok); 
        if (!ok) { 
            glGetShaderInfoLog(s, 1024, NULL, log); 
            std::cerr << "Shader ERR " << t << ": " << log << std::endl; 
        } 
    }
    else { 
        glGetProgramiv(s, GL_LINK_STATUS, &ok); 
        if (!ok) { 
            glGetProgramInfoLog(s, 1024, NULL, log); 
            std::cerr << "Program ERR: " << log << std::endl; 
        } 
    }
}

bool Shape::checkHover(const PrimitiveConfig& c) {
    double mx, my; 
    InputManager::getMousePos(mx, my);
    
    Vec2 p = transformPosInternal(c.pos, c.coordSystem, RenderManager::getInstance().getScreenWidth(), RenderManager::getInstance().getScreenHeight());
    Vec2 s = transformSizeInternal(c.size, c.coordSystem, RenderManager::getInstance().getScreenWidth(), RenderManager::getInstance().getScreenHeight());
    
    float px = p.x; 
    float py = p.y;
    if (c.posMode == PositionMode::Static) { 
        px += RenderManager::getInstance().getScrollOffset().x; 
        py += RenderManager::getInstance().getScrollOffset().y; 
    }
    return (mx >= px && mx <= px + s.x && my >= py && my <= py + s.y);
}

void Button::update(TextManager* tm) {
    bool h = checkHover(config); 
    bool d = InputManager::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    config.isHovered = h; 
    config.isPressed = h && d;
    if (h && !wasHoveredLastFrame && config.onHover) config.onHover();
    if (h && d && !wasMouseDownLastFrame && config.onPressDown) config.onPressDown();
    if (wasMouseDownLastFrame && !d) { 
        if (config.onPressUp) config.onPressUp(); 
        if (h && config.onClick) config.onClick(); 
    }
    wasHoveredLastFrame = h; 
    wasMouseDownLastFrame = d;
}

void Button::draw(TextManager* tm) {
    PrimitiveConfig dCfg = config;
    if (config.useAutoColors) {
        if (config.isPressed) dCfg.backgroundColor = config.pressedColor;
        else if (config.isHovered) dCfg.backgroundColor = config.hoverColor;
    }
    RenderManager::getInstance().drawRect(dCfg);
    RenderManager::getInstance().drawText(tm, config.text, config.pos, config.textScale, config.foregroundColor, config.hAlign, config.vAlign, config.size, config.posMode);
}

void InputField::handleKeyPress(int key, std::string& d) {
    if (key == GLFW_KEY_LEFT && cursorIndex > 0) cursorIndex--;
    if (key == GLFW_KEY_RIGHT && cursorIndex < d.length()) cursorIndex++;
    if (key == GLFW_KEY_BACKSPACE && cursorIndex > 0) { d.erase(cursorIndex - 1, 1); cursorIndex--; }
    if (key == GLFW_KEY_DELETE && cursorIndex < d.length()) d.erase(cursorIndex, 1);
    if (config.isMultiline) {
        if (key == GLFW_KEY_ENTER) { d.insert(cursorIndex, "\n"); cursorIndex++; }
        if (key == GLFW_KEY_UP) {
            size_t ls = d.find_last_of('\n', cursorIndex > 0 ? cursorIndex - 1 : 0); 
            if (ls == std::string::npos) ls = 0; 
            else ls++;
            size_t col = cursorIndex - ls;
            if (ls > 0) { 
                size_t pe = ls - 1; 
                size_t ps = d.find_last_of('\n', pe > 0 ? pe - 1 : 0); 
                if (ps == std::string::npos) ps = 0; 
                else ps++; 
                cursorIndex = ps + std::min(col, pe - ps); 
            }
        }
        if (key == GLFW_KEY_DOWN) {
            size_t le = d.find('\n', cursorIndex);
            if (le != std::string::npos) {
                size_t ls = d.find_last_of('\n', cursorIndex > 0 ? cursorIndex - 1 : 0); 
                if (ls == std::string::npos) ls = 0; 
                else ls++;
                size_t col = cursorIndex - ls; 
                size_t ns = le + 1; 
                size_t ne = d.find('\n', ns); 
                if (ne == std::string::npos) ne = d.length(); 
                cursorIndex = ns + std::min(col, ne - ns);
            }
        }
    }
}

void InputField::update(TextManager* tm) {
    int sw = RenderManager::getInstance().getScreenWidth();
    int sh = RenderManager::getInstance().getScreenHeight();
    Vec2 p = transformPosInternal(config.pos, config.coordSystem, sw, sh);
    Vec2 s = transformSizeInternal(config.size, config.coordSystem, sw, sh);
    Vec2 sOff = RenderManager::getInstance().getScrollOffset();
    
    bool hovered = checkHover(config); 
    double mx, my; 
    InputManager::getMousePos(mx, my); 
    bool mouseDown = InputManager::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);

    float m = config.internalMargin; 
    float sbW = enableScrollbar ? scrollbarWidth : 0.0f; 
    float cw = s.x - 2*m - sbW;

    if (mouseDown && !wasMouseDownLastFrame) {
        bool previouslyFocused = isFocused; 
        isFocused = hovered;
        if (isFocused && config.dataField && tm) {
            std::string& d = *(config.dataField); 
            std::string ds = d;
            if (config.maskChar != '\0') ds = std::string(d.length(), config.maskChar);
            
            bool firstInteraction = !previouslyFocused; 
            bool useCursor = (clickBehavior == CursorClickBehavior::GotoCursor) || (clickBehavior == CursorClickBehavior::GotoCursorOnClickButEndFirst && !firstInteraction);
            if (useCursor) { 
                float ox = (config.posMode == PositionMode::Static ? sOff.x : 0);
                float oy = (config.posMode == PositionMode::Static ? sOff.y : 0);
                Vec2 localMouse = {(float)mx - p.x - ox - m, (float)my - p.y - oy - m + scrollAmount}; 
                cursorIndex = tm->getCursorIndexFromCoords(ds, localMouse, config.textScale, cw, config.hAlign); 
            }
            else if (firstInteraction) cursorIndex = d.length();
        }
    }
    if (isFocused && config.dataField) {
        std::string& d = *(config.dataField); 
        std::string input = InputManager::getAndClearCharBuffer(); 
        if (!input.empty()) { 
            d.insert(cursorIndex, input); 
            cursorIndex += input.length(); 
        }
        int ks[] = {
            GLFW_KEY_LEFT, 
            GLFW_KEY_RIGHT, 
            GLFW_KEY_UP, 
            GLFW_KEY_DOWN, 
            GLFW_KEY_BACKSPACE, 
            GLFW_KEY_DELETE, 
            GLFW_KEY_ENTER
        }; 
        double now = glfwGetTime();
        for (int k : ks) {
            if (InputManager::isKeyPressed(k)) { 
                handleKeyPress(k, d); 
                lastKey = k; 
                lastKeyTime = now; 
                lastRepeatTime = now; 
            }
            else if (InputManager::isKeyDown(k) && lastKey == k) { 
                if (now - lastKeyTime > repeatDelay && now - lastRepeatTime > repeatRate) { 
                    handleKeyPress(k, d); 
                    lastRepeatTime = now; 
                } 
            }
        }
        if (lastKey != -1 && !InputManager::isKeyDown(lastKey)) lastKey = -1;
    }
    if (hovered) { 
        double scroll = InputManager::getAndClearScrollDelta(); 
        if (scroll != 0) { 
            scrollAmount -= (float)scroll * 20.0f; 
            if (scrollAmount < 0) scrollAmount = 0; 
        } 
    }
    if (enableScrollbar && tm && config.dataField) {
        float ox = (config.posMode == PositionMode::Static ? sOff.x : 0);
        float oy = (config.posMode == PositionMode::Static ? sOff.y : 0);
        float sbX = p.x + ox + s.x - scrollbarWidth - m; 
        bool mouseOverSB = (
            mx >= sbX && 
            mx <= sbX + scrollbarWidth && 
            my >= p.y + oy + m && 
            my <= p.y + oy + s.y - m
        );
        Vec2 totalSize = tm->getTextSize(*config.dataField, config.textScale, cw); 
        float contentH = totalSize.y; 
        float trackH = s.y - 2*m; 
        float maxScroll = std::max(0.0f, contentH - trackH); 
        float thumbH = std::max(20.0f, trackH * (trackH / std::max(contentH, trackH)));
        if (mouseDown) { 
            if (!wasMouseDownLastFrame && mouseOverSB) { 
                isDraggingScrollbar = true; 
                dragMouseStart = (float)my; 
                dragScrollStart = scrollAmount; 
            } 
        } else isDraggingScrollbar = false;
        if (isDraggingScrollbar && maxScroll > 0) { 
            float dy = (float)my - dragMouseStart; 
            float scrollRatioPerPixel = maxScroll / (trackH - thumbH); 
            scrollAmount = dragScrollStart + dy * scrollRatioPerPixel; 
            scrollAmount = std::clamp(scrollAmount, 0.0f, maxScroll); 
        }
        if (scrollAmount > maxScroll) scrollAmount = maxScroll;
    }
    wasMouseDownLastFrame = mouseDown;
}

void InputField::draw(TextManager* tm) {
    int sw = RenderManager::getInstance().getScreenWidth();
    int sh = RenderManager::getInstance().getScreenHeight();
    Vec2 p = transformPosInternal(config.pos, config.coordSystem, sw, sh);
    Vec2 s = transformSizeInternal(config.size, config.coordSystem, sw, sh);

    PrimitiveConfig dCfg = config; 
    if (isFocused) { 
        dCfg.borderColor = {0.2f, 0.5f, 1.0f, 1.0f}; 
        if (dCfg.borderThickness < 1.0f) dCfg.borderThickness = 2.0f; 
    }
    RenderManager::getInstance().drawRect(dCfg);

    float m = config.internalMargin; 
    float sbW = enableScrollbar ? scrollbarWidth : 0.0f;
    float cx = p.x + m, cy = p.y + m, cw = s.x - 2*m - sbW, ch = s.y - 2*m;
    Vec2 sOff = RenderManager::getInstance().getScrollOffset(); 
    float scx = cx, scy = cy; 
    if (config.posMode == PositionMode::Static) { 
        scx += sOff.x; 
        scy += sOff.y; 
    }
    glEnable(GL_SCISSOR_TEST); 
    glScissor((int)scx, (int)((float)sh - (scy + ch)), (int)cw, (int)ch);
    std::string text = (config.dataField) ? *config.dataField : "";
    std::string ds = text; 
    if (config.maskChar != '\0') ds = std::string(text.length(), config.maskChar);
    
    Vec2 tPos = {cx, cy - scrollAmount}; 
    Vec2 tSize = enableTextWrap ? Vec2{cw, 0} : Vec2{0, 0};
    
    PrimitiveConfig textCfg;
    textCfg.pos = tPos;
    textCfg.size = tSize;
    textCfg.coordSystem = CoordinateSystem::Pixels;
    textCfg.posMode = config.posMode;
    
    RenderManager::getInstance().drawText(
        tm, ds.empty() ? config.placeholder : ds, 
        textCfg.pos, config.textScale, ds.empty() ? Color{0.5f,0.5f,0.5f,1.0f} : config.foregroundColor, 
        config.hAlign, config.vAlign, textCfg.size, textCfg.posMode
    );

    if (isFocused && tm) {
        Vec2 cc = tm->getCursorCoords(ds, cursorIndex, tPos, config.textScale, config.hAlign, config.vAlign, tSize);
        if (config.posMode == PositionMode::Static) {
            cc.x += sOff.x;
            cc.y += sOff.y;
        }

        float unitH = tm->getLineHeight() * config.textScale;
        PrimitiveConfig cCfg; 
        cCfg.pos = {cc.x, cc.y};
        cCfg.size = {2.0f, unitH}; 
        cCfg.backgroundColor = config.foregroundColor; 
        cCfg.posMode = PositionMode::Fixed; 
        cCfg.coordSystem = CoordinateSystem::Pixels;
        RenderManager::getInstance().drawRect(cCfg);
    }
    glDisable(GL_SCISSOR_TEST);

    if (enableScrollbar && tm && config.dataField) {
        PrimitiveConfig sbCfg; 
        sbCfg.pos = {p.x + s.x - sbW - m, p.y + m}; 
        sbCfg.size = {sbW, s.y - 2*m};
        sbCfg.backgroundColor = {0.2f, 0.2f, 0.2f, 1.0f}; 
        sbCfg.borderRadius = sbW * 0.5f; 
        sbCfg.posMode = config.posMode; 
        sbCfg.coordSystem = CoordinateSystem::Pixels;
        RenderManager::getInstance().drawRect(sbCfg);

        Vec2 totalSize = tm->getTextSize(*config.dataField, config.textScale, cw);
        float trackH = sbCfg.size.y;
        float thumbH = std::max(20.0f, trackH * (trackH / std::max(totalSize.y, trackH)));
        float maxScroll = std::max(0.0f, totalSize.y - trackH); 
        float scrollRatio = (maxScroll > 0) ? (scrollAmount / maxScroll) : 0;

        PrimitiveConfig thumb; 
        thumb.pos = {sbCfg.pos.x + 2, sbCfg.pos.y + 2 + scrollRatio * (trackH - thumbH - 4)};
        thumb.size = {sbW - 4, thumbH}; 
        thumb.backgroundColor = isDraggingScrollbar ? Color{0.7f,0.7f,0.7f,1.0f} : Color{0.5f,0.5f,0.5f,1.0f};
        thumb.borderRadius = thumb.size.x * 0.5f; 
        thumb.posMode = config.posMode; 
        thumb.coordSystem = CoordinateSystem::Pixels;
        RenderManager::getInstance().drawRect(thumb);
    }
}

void Dropdown::update(TextManager* tm) {
    bool c = checkHover(config); 
    bool d = InputManager::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    if (d && !wasMouseDownLastFrame) {
        if (c) { isOpen = !isOpen; 
            if (isOpen) { 
                originalZIndex = config.zIndex; 
                config.zIndex = 1000.0f; 
            } 
            else config.zIndex = originalZIndex; 
        }
        else if (isOpen) {
            int sw = RenderManager::getInstance().getScreenWidth();
            int sh = RenderManager::getInstance().getScreenHeight();
            Vec2 s = transformSizeInternal(config.size, config.coordSystem, sw, sh);
            float oh = s.y; 
            bool cl = false;
            for (int i = 0; i < (int)options.size(); ++i) {
                PrimitiveConfig o = config; 
                o.pos.y = config.pos.y + (config.coordSystem == CoordinateSystem::NDC ? (i + 1) * oh / (sh * 0.5f) : (i + 1) * oh); 
                if (checkHover(o)) { 
                    selectedIndex = i; 
                    config.text = options[i]; 
                    if (onSelect) onSelect(i); 
                    cl = true; 
                    break; 
                }
            }
            if (cl || !c) { 
                isOpen = false; 
                config.zIndex = originalZIndex; 
            }
        }
    }
    wasMouseDownLastFrame = d;
}

void Dropdown::draw(TextManager* tm) {
    RenderManager::getInstance().drawRect(config);
    RenderManager::getInstance().drawText(
        tm, config.text, config.pos, config.textScale, 
        config.foregroundColor, config.hAlign, config.vAlign, config.size, config.posMode
    );
    if (isOpen) {
        int sw = RenderManager::getInstance().getScreenWidth();
        int sh = RenderManager::getInstance().getScreenHeight();
        Vec2 s = transformSizeInternal(config.size, config.coordSystem, sw, sh);
        float oh = s.y;
        for (int i = 0; i < (int)options.size(); ++i) {
            PrimitiveConfig o = config; 
            if (config.coordSystem == CoordinateSystem::NDC) {
                 o.pos.y = config.pos.y - (i + 1) * (oh / (sh * 0.5f)); // NDC Y is up
            } else {
                 o.pos.y = config.pos.y + (i + 1) * oh;
            }
            o.text = options[i]; 
            o.zIndex = config.zIndex + 1; // Ensure options are above the main box
            o.backgroundColor = (selectedIndex == i) ? Color{0.3f, 0.3f, 0.3f, 1.0f} : config.backgroundColor;
            RenderManager::getInstance().drawRect(o);
            RenderManager::getInstance().drawText(
                tm, options[i], o.pos, o.textScale, 
                o.foregroundColor, o.hAlign, o.vAlign, o.size, o.posMode
            );
        }
    }
}
