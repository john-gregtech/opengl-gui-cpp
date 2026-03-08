#include "InputManager.hpp"

std::map<int, bool> InputManager::keys;
std::map<int, bool> InputManager::keysPressed;
std::map<int, bool> InputManager::mouseButtons;
double InputManager::mouseX = 0;
double InputManager::mouseY = 0;
double InputManager::scrollDeltaY = 0;
std::string InputManager::charBuffer = "";

void InputManager::init(GLFWwindow* window) {
    glfwSetKeyCallback(window, keyCallback);
    glfwSetCharCallback(window, charCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
}

bool InputManager::isKeyDown(int key) { return keys[key]; }
bool InputManager::isMouseButtonDown(int button) { return mouseButtons[button]; }
void InputManager::getMousePos(double& x, double& y) { x = mouseX; y = mouseY; }

double InputManager::getAndClearScrollDelta() {
    double d = scrollDeltaY;
    scrollDeltaY = 0;
    return d;
}

std::string InputManager::getAndClearCharBuffer() {
    std::string b = charBuffer;
    charBuffer = "";
    return b;
}

bool InputManager::isKeyPressed(int key) {
    if (keysPressed[key]) {
        keysPressed[key] = false;
        return true;
    }
    return false;
}

void InputManager::keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        keys[key] = true;
        keysPressed[key] = true;
    } else if (action == GLFW_RELEASE) {
        keys[key] = false;
    }
}

void InputManager::charCallback(GLFWwindow* window, unsigned int codepoint) {
    charBuffer += (char)codepoint;
}

void InputManager::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) mouseButtons[button] = true;
    else if (action == GLFW_RELEASE) mouseButtons[button] = false;
}

void InputManager::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    mouseX = xpos;
    mouseY = ypos;
}

void InputManager::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    scrollDeltaY += yoffset;
}
