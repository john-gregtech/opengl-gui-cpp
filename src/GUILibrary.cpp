#include "GUILibrary.hpp"
#include "InputManager.hpp"
#include <glad/gl.h>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "TextManager.hpp"
#include <stb_image.h>

GUILibrary::GUILibrary(int width, int height, const std::string& title, bool decorated, const std::string& fontPath) {
    if (!glfwInit()) { std::cerr << "Failed to initialize GLFW" << std::endl; return; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, decorated ? GLFW_TRUE : GLFW_FALSE);
    window = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
    if (!window) { glfwTerminate(); return; }
    glfwMakeContextCurrent(window);
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) { std::cerr << "Failed to initialize GLAD" << std::endl; return; }
    InputManager::init(window);
    RenderManager::getInstance().init();
    RenderManager::getInstance().updateMatrices(width, height);
    if (!fontPath.empty()) textManager = std::make_unique<TextManager>(fontPath, 48);
    initialized = true; isDragging = false; decorationConfig.title = title;
    decorationConfig.height = 40.0f; menuBarConfig.height = 30.0f;
    arrowCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
    textCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    handCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
}

void GUILibrary::setWindowIcon(const std::string& path) {
    if (!window) return;
    GLFWimage images[1]; int width, height, channels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (pixels) {
        images[0].width = width; images[0].height = height; images[0].pixels = pixels;
        glfwSetWindowIcon(window, 1, images); stbi_image_free(pixels);
    }
}

GUILibrary::~GUILibrary() {
    glfwDestroyCursor(arrowCursor); glfwDestroyCursor(textCursor); glfwDestroyCursor(handCursor);
    glfwTerminate();
}

void GUILibrary::hideConsole() {
#ifdef _WIN32
    HWND hWnd = GetConsoleWindow(); if (hWnd) ShowWindow(hWnd, SW_HIDE);
#endif
}

void GUILibrary::setControlsVisible(bool visible) { decorationConfig.showControls = visible; decorationElements.clear(); if (visible) setupDefaultControls(); }
void GUILibrary::setMenuBarVisible(bool visible) { menuBarConfig.visible = visible; }
void GUILibrary::addMenuItem(const std::string& name, std::function<void()> action) { menuItems.push_back({name, action}); updateMenuBarItems(); }

void GUILibrary::updateMenuBarItems() {
    menuBarButtons.clear(); if (!textManager) return;
    float paddingY = menuBarConfig.height * 0.2f;
    float targetTextHeight = menuBarConfig.height - (paddingY * 2.0f);
    float scale = targetTextHeight / textManager->getLineHeight();
    for (const auto& item : menuItems) {
        auto btn = std::make_unique<Button>();
        Vec2 textSize = textManager->getTextSize(item.name, scale);
        float paddingX = 20.0f;
        btn->config.text = item.name; btn->config.textScale = scale;
        btn->config.backgroundColor = Color::Transparent(); btn->config.foregroundColor = menuBarConfig.textColor;
        btn->config.posMode = PositionMode::Fixed; btn->config.size = {textSize.x + paddingX, menuBarConfig.height};
        btn->config.hAlign = HorizontalAlignment::Center; btn->config.vAlign = VerticalAlignment::Center;
        btn->config.onClick = item.action;
        menuBarButtons.push_back(std::move(btn));
    }
}

void GUILibrary::enableCustomDecoration(const WindowDecoration& config) {
    decorationConfig = config; decorationConfig.enabled = true; if (decorationConfig.height < 1.0f) decorationConfig.height = 40.0f; setupDefaultControls();
}

void GUILibrary::setupDefaultControls() {
    if (!decorationConfig.showControls) return;
    for (int i = 0; i < 3; ++i) {
        auto btn = std::make_unique<Button>(); btn->config.posMode = PositionMode::Fixed; btn->config.textScale = 0.35f;
        btn->config.hAlign = HorizontalAlignment::Center; btn->config.vAlign = VerticalAlignment::Center;
        if (i == 0) { btn->config.text = "X"; btn->config.backgroundColor = {0.8f, 0.2f, 0.2f, 1.0f}; btn->config.onClick = [this](){ glfwSetWindowShouldClose(window, GLFW_TRUE); }; }
        else if (i == 1) { btn->config.text = "[]"; btn->config.backgroundColor = {0.4f, 0.4f, 0.4f, 1.0f}; btn->config.onClick = [this](){ if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) glfwRestoreWindow(window); else glfwMaximizeWindow(window); }; }
        else { btn->config.text = "_"; btn->config.backgroundColor = {0.4f, 0.4f, 0.4f, 1.0f}; btn->config.onClick = [this](){ glfwIconifyWindow(window); }; }
        decorationElements.push_back(std::move(btn));
    }
    updateDecorationPositions();
}

void GUILibrary::updateDecorationPositions() {
    int w, h; glfwGetWindowSize(window, &w, &h);
    float btnW = 40.0f; float btnH = decorationConfig.height * 0.8f;
    if (decorationElements.size() >= 3) {
        float margin = (decorationConfig.height - btnH) / 2.0f;
        decorationElements[0]->config.pos = {(float)w - btnW - 5.0f, margin}; decorationElements[0]->config.size = {btnW, btnH};
        decorationElements[1]->config.pos = {(float)w - 2*btnW - 10.0f, margin}; decorationElements[1]->config.size = {btnW, btnH};
        decorationElements[2]->config.pos = {(float)w - 3*btnW - 15.0f, margin}; decorationElements[2]->config.size = {btnW, btnH};
    }
    float menuY = decorationConfig.height;
    if (menuBarConfig.visible) {
        float currentX = 5.0f;
        for (auto& btn : menuBarButtons) { btn->config.pos = {currentX, menuY}; currentX += btn->config.size.x + 5.0f; }
    }
}

void GUILibrary::handleWindowDragging() {
    if (!decorationConfig.enabled) return;
    double mx, my; InputManager::getMousePos(mx, my);
    bool isDown = InputManager::isMouseButtonDown(GLFW_MOUSE_BUTTON_LEFT);
    if (isDown && !isDragging) {
        if (my >= 0 && my <= decorationConfig.height) {
             if (glfwGetWindowAttrib(window, GLFW_MAXIMIZED)) {
                 int winW, winH; glfwGetWindowSize(window, &winW, &winH);
                 float xPct = (float)mx / (float)winW;
                 glfwRestoreWindow(window);
                 int newW, newH; glfwGetWindowSize(window, &newW, &newH);
                 double newMX, newMY; glfwGetCursorPos(window, &newMX, &newMY);
                 int screenX, screenY; // We need global mouse pos
                 // Simplify: just center the restored window on the mouse
                 // But better: use the pct to keep it under cursor
                 glfwSetWindowPos(window, (int)(mx - xPct * newW), (int)(my - newMY));
                 glfwGetCursorPos(window, &dragStartX, &dragStartY);
             } else {
                 glfwGetCursorPos(window, &dragStartX, &dragStartY);
             }
             isDragging = true;
        }
    }
    if (isDragging) {
        if (isDown) {
            double cx, cy; glfwGetCursorPos(window, &cx, &cy);
            int wx, wy; glfwGetWindowPos(window, &wx, &wy);
            int dx = (int)(cx - dragStartX); int dy = (int)(cy - dragStartY);
            if (dx != 0 || dy != 0) glfwSetWindowPos(window, wx + dx, wy + dy);
        } else isDragging = false;
    }
}

void GUILibrary::addElement(std::unique_ptr<Shape> element) { elements.push_back(std::move(element)); }

void GUILibrary::run() {
    if (!initialized) return;
    while (!glfwWindowShouldClose(window)) {
        int width, height; glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        RenderManager::getInstance().updateMatrices(width, height);
        updateDecorationPositions();
        RenderManager::getInstance().clear({0.15f, 0.15f, 0.15f, 1.0f});
        RenderManager::getInstance().setScrollOffset({0, 0});
        for (auto& element : elements) element->update(textManager.get());
        for (auto& deco : decorationElements) deco->update(textManager.get());
        for (auto& menuBtn : menuBarButtons) if (menuBarConfig.visible) menuBtn->update(textManager.get());
        bool cursorSet = false;
        for (auto& deco : decorationElements) { if (deco->config.isHovered) { glfwSetCursor(window, handCursor); cursorSet = true; break; } }
        if (!cursorSet && menuBarConfig.visible) { for (auto& menuBtn : menuBarButtons) { if (menuBtn->config.isHovered) { glfwSetCursor(window, handCursor); cursorSet = true; break; } } }
        if (!cursorSet) {
            for (int i = (int)elements.size() - 1; i >= 0; --i) {
                if (elements[i]->config.isHovered) {
                    if (dynamic_cast<InputField*>(elements[i].get())) glfwSetCursor(window, textCursor);
                    else if (dynamic_cast<Button*>(elements[i].get()) || dynamic_cast<Dropdown*>(elements[i].get())) glfwSetCursor(window, handCursor);
                    else glfwSetCursor(window, arrowCursor);
                    cursorSet = true; break;
                }
            }
        }
        if (!cursorSet) glfwSetCursor(window, arrowCursor);
        handleWindowDragging();
        std::stable_sort(elements.begin(), elements.end(), [](const std::unique_ptr<Shape>& a, const std::unique_ptr<Shape>& b) { return a->config.zIndex < b->config.zIndex; });
        for (auto& element : elements) element->draw(textManager.get());
        if (decorationConfig.enabled) {
            PrimitiveConfig bar; bar.pos = {0, 0}; bar.size = {(float)width, decorationConfig.height};
            bar.backgroundColor = decorationConfig.backgroundColor; bar.posMode = PositionMode::Fixed;
            RenderManager::getInstance().drawRect(bar);
            RenderManager::getInstance().drawText(textManager.get(), decorationConfig.title, {10, 0}, 0.4f, decorationConfig.titleColor, HorizontalAlignment::Left, VerticalAlignment::Center, {(float)width, decorationConfig.height}, PositionMode::Fixed);
            if (menuBarConfig.visible) {
                PrimitiveConfig menuBar; menuBar.pos = {0, decorationConfig.height}; menuBar.size = {(float)width, menuBarConfig.height};
                menuBar.backgroundColor = menuBarConfig.backgroundColor; menuBar.posMode = PositionMode::Fixed;
                RenderManager::getInstance().drawRect(menuBar);
                for (auto& menuBtn : menuBarButtons) menuBtn->draw(textManager.get());
            }
        }
        for (auto& deco : decorationElements) deco->draw(textManager.get());
        glfwSwapBuffers(window); glfwPollEvents();
    }
}
