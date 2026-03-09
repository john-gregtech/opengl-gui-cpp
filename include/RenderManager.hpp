#pragma once
#include <glad/gl.h>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <array>

class TextManager;

struct Color { 
    float r, g, b, a; 
    static Color White() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static Color Black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static Color Gray() { return {0.5f, 0.5f, 0.5f, 1.0f}; }
    static Color Transparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    static Color Red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    static Color Green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    static Color Blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    
    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
};

struct Vec2 { float x, y; };

struct Mat4 {
    std::array<float, 16> data = {0};
    static Mat4 Ortho(float left, float right, float bottom, float top, float zNear = -1.0f, float zFar = 1.0f) {
        Mat4 m;
        m.data[0] = 2.0f / (right - left);
        m.data[5] = 2.0f / (top - bottom);
        m.data[10] = -2.0f / (zFar - zNear);
        m.data[12] = -(right + left) / (right - left);
        m.data[13] = -(top + bottom) / (top - bottom);
        m.data[14] = -(zFar + zNear) / (zFar - zNear);
        m.data[15] = 1.0f;
        return m;
    }
};

enum class PositionMode { Static, Absolute, Fixed };
enum class HorizontalAlignment { Left, Center, Right };
enum class VerticalAlignment { Top, Center, Bottom };
enum class CoordinateSystem { NDC, Pixels }; 

enum class CursorClickBehavior {
    GotoEnd,
    GotoCursorOnClickButEndFirst,
    GotoCursor
};

struct PrimitiveConfig {
    Vec2 pos = {0, 0};
    Vec2 size = {0, 0};
    PositionMode posMode = PositionMode::Static;
    CoordinateSystem coordSystem = CoordinateSystem::Pixels; 
    float zIndex = 0.0f;
    
    Color foregroundColor = Color::Black();
    Color backgroundColor = Color::Gray();
    Color hoverColor = {0.6f, 0.6f, 0.6f, 1.0f};
    Color pressedColor = {0.4f, 0.4f, 0.4f, 1.0f};
    bool useAutoColors = true;

    float borderThickness = 0.0f;
    Color borderColor = Color::Transparent();
    float borderRadius = 0.0f;
    float internalMargin = 0.0f;
    
    std::string text = "";
    float textScale = 1.0f;
    HorizontalAlignment hAlign = HorizontalAlignment::Left;
    VerticalAlignment vAlign = VerticalAlignment::Center;
    
    std::function<void()> onClick = nullptr;
    std::function<void()> onPressDown = nullptr;
    std::function<void()> onPressUp = nullptr;
    std::function<void()> onHover = nullptr;
    
    bool isPressed = false;
    bool isHovered = false;
    
    std::string placeholder = "";
    std::string* dataField = nullptr;
    unsigned int textureID = 0;
    bool isMultiline = false;
    char maskChar = '\0';
    Vec2 internalScroll = {0, 0};
};

struct ShaderUniforms {
    int projection = -1;
    int pos = -1;
    int size = -1;
    int radius = -1;
    int bgColor = -1;
    int borderColor = -1;
    int borderThickness = -1;
    int color = -1; // Generic color
    int textureSampler = -1;
};

class RenderManager {
public:
    static RenderManager& getInstance() {
        static RenderManager instance;
        return instance;
    }
    void init();
    void clear(Color color);
    void updateMatrices(int width, int height);
    
    void drawRect(const PrimitiveConfig& config);
    void drawCircle(const PrimitiveConfig& config, float radius, int segments = 32);
    void drawTriangle(Vec2 p1, Vec2 p2, Vec2 p3, Color color, PositionMode mode = PositionMode::Static);
    void drawText(TextManager* tm, const std::string& text, Vec2 pos, float scale, Color color, 
                  HorizontalAlignment hAlign = HorizontalAlignment::Left, 
                  VerticalAlignment vAlign = VerticalAlignment::Top,
                  Vec2 boxSize = {0,0}, PositionMode mode = PositionMode::Static);
    void drawImage(unsigned int textureID, Vec2 pos, Vec2 size, PositionMode mode = PositionMode::Static);
    
    unsigned int loadTexture(const char* path);
    void setScrollOffset(Vec2 offset) { scrollOffset = offset; }
    Vec2 getScrollOffset() const { return scrollOffset; }
    const Mat4& getProjection() const { return projection; }
    int getScreenWidth() const { return screenWidth; }
    int getScreenHeight() const { return screenHeight; }

private:
    RenderManager() = default;
    unsigned int shaderProgram, textureShaderProgram, sdfRectShaderProgram;
    unsigned int vao, vbo, textureVao, textureVbo;
    
    ShaderUniforms sdfRectUniforms;
    ShaderUniforms textureUniforms;
    ShaderUniforms basicUniforms;

    Vec2 scrollOffset = {0, 0};
    Mat4 projection;
    int screenWidth = 0, screenHeight = 0;
    
    unsigned int createShader(const char* vertexSrc, const char* fragmentSrc);
    void checkShaderError(unsigned int shader, std::string type);
    void setupUniforms();
};

class Shape {
public:
    PrimitiveConfig config;
    virtual void update(TextManager* tm) {}
    virtual void draw(TextManager* tm) = 0;
    virtual ~Shape() = default;
    static bool checkHover(const PrimitiveConfig& config);
protected:
    bool wasMouseDownLastFrame = false;
    bool wasHoveredLastFrame = false;
};

class Rect : public Shape {
public:
    void draw(TextManager* tm) override { RenderManager::getInstance().drawRect(config); }
};

class Circle : public Shape {
public:
    float radius = 10.0f;
    void draw(TextManager* tm) override { RenderManager::getInstance().drawCircle(config, radius); }
};

class Triangle : public Shape {
public:
    Vec2 p1, p2, p3;
    void setPosVec(int x[3], int y[3]);
    void draw(TextManager* tm) override { RenderManager::getInstance().drawTriangle(p1, p2, p3, config.backgroundColor, config.posMode); }
};

class Image : public Shape {
public:
    void draw(TextManager* tm) override { RenderManager::getInstance().drawImage(config.textureID, config.pos, config.size, config.posMode); }
};

class Button : public Shape {
public:
    void update(TextManager* tm) override;
    void draw(TextManager* tm) override;
};

class InputField : public Shape {
public:
    bool isFocused = false;
    size_t cursorIndex = 0;

    bool enableScrollbar = false;
    bool enableTextWrap = false;
    float scrollbarWidth = 12.0f;
    float scrollAmount = 0.0f; 
    CursorClickBehavior clickBehavior = CursorClickBehavior::GotoEnd;

    void update(TextManager* tm) override;
    void draw(TextManager* tm) override;
    void drawCursor(TextManager* tm);

private:
    double lastKeyTime = 0;
    int lastKey = -1;
    float repeatDelay = 0.5f;
    float repeatRate = 0.05f;
    float lastRepeatTime = 0;

    bool isDraggingScrollbar = false;
    float dragScrollStart = 0.0f;
    float dragMouseStart = 0.0f;

    void handleKeyPress(int key, std::string& d);
};

class Dropdown : public Shape {
public:
    std::vector<std::string> options;
    int selectedIndex = -1;
    bool isOpen = false;
    std::function<void(int)> onSelect = nullptr;
    void update(TextManager* tm) override;
    void draw(TextManager* tm) override;
private:
    float originalZIndex = 0.0f;
};
