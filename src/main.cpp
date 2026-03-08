#include "GUILibrary.hpp"
#include <iostream>
#include <string>

int main() {
    // 1. Console is back (removed hideConsole)
    std::string fontPath = "C:/Users/linuxnoob/Documents/c++/openGL_GUI/res/fonts/NotoSans-Regular.ttf";
    
    // 2. Initialize window (Pixels: 1000x800)
    GUILibrary gui(1000, 800, "OpenGL GUI Showcase", false, fontPath);

    // 3. Configure Title Bar
    WindowDecoration deco;
    deco.enabled = true;
    deco.height = 40.0f;
    deco.title = "Studio OpenGL v1.1 - Showcase";
    deco.backgroundColor = {0.1f, 0.1f, 0.1f, 1.0f};
    deco.titleColor = {0.9f, 0.9f, 0.9f, 1.0f};
    gui.enableCustomDecoration(deco);

    // 4. MAIN FEATURE: Multi-line Enhanced Textbox
    static std::string noteData = "NEW FEATURES SHOWCASE:\n\n"
                                  "1. INTERACTIVE SCROLLBAR: Use your mouse wheel or click and drag the grey thumb on the right!\n\n"
                                  "2. TEXT WRAPPING: This text automatically wraps when it hits the scrollbar margin.\n\n"
                                  "3. CURSOR PRECISION: Click anywhere in this text to place the cursor exactly where you clicked.\n\n"
                                  "4. KEY AUTO-REPEAT: Hold down the arrow keys or backspace to see smooth repeating motion.\n\n"
                                  "Try typing a very long paragraph here to see the scrollbar thumb adjust and the wrapping in action!";
    
    auto noteInput = std::make_unique<InputField>();
    noteInput->config.pos = {50, 100};
    noteInput->config.size = {500, 400};
    noteInput->config.dataField = &noteData;
    noteInput->config.isMultiline = true;
    noteInput->config.backgroundColor = {0.12f, 0.12f, 0.12f, 1.0f};
    noteInput->config.foregroundColor = {0.9f, 0.9f, 0.9f, 1.0f};
    noteInput->config.internalMargin = 15.0f;
    noteInput->config.textScale = 0.45f; 
    
    // Toggles
    noteInput->enableScrollbar = true;
    noteInput->enableTextWrap = true;
    noteInput->clickBehavior = CursorClickBehavior::GotoCursor; // Always go to click point
    
    gui.addElement(std::move(noteInput));

    // 5. SINGLE LINE INPUT + PRINT BUTTON
    static std::string singleLineData = "Type something here...";
    auto singleInput = std::make_unique<InputField>();
    singleInput->config.pos = {600, 100};
    singleInput->config.size = {350, 40};
    singleInput->config.dataField = &singleLineData;
    singleInput->config.isMultiline = false;
    singleInput->config.backgroundColor = {0.2f, 0.2f, 0.2f, 1.0f};
    singleInput->config.foregroundColor = Color::White();
    singleInput->config.internalMargin = 8.0f;
    singleInput->config.textScale = 0.45f;
    singleInput->clickBehavior = CursorClickBehavior::GotoCursorOnClickButEndFirst; // Go to end first, then precision
    
    gui.addElement(std::move(singleInput));

    auto printBtn = std::make_unique<Button>();
    printBtn->config.pos = {600, 150};
    printBtn->config.size = {120, 40};
    printBtn->config.text = "Print to Console";
    printBtn->config.textScale = 0.4f;
    printBtn->config.backgroundColor = {0.2f, 0.5f, 0.3f, 1.0f};
    printBtn->config.onClick = []() {
        std::cout << "[App Console] Single Line Content: " << singleLineData << std::endl;
    };
    gui.addElement(std::move(printBtn));

    // 6. OTHER CONTROLS (Z-Indexing Test)
    auto dropdown = std::make_unique<Dropdown>();
    dropdown->config.pos = {600, 250};
    dropdown->config.size = {300, 40};
    dropdown->config.text = "Behavior Toggle";
    dropdown->options = {"Rect Mode", "Circle Mode", "Triangle Mode"};
    dropdown->config.zIndex = 10.0f;
    dropdown->config.textScale = 0.45f;
    gui.addElement(std::move(dropdown));

    auto infoLabel = std::make_unique<Rect>();
    infoLabel->config.pos = {50, 520};
    infoLabel->config.size = {900, 100};
    infoLabel->config.backgroundColor = {0.1f, 0.1f, 0.1f, 0.5f};
    infoLabel->config.text = "TIP: The multi-line box (left) is in 'GotoCursor' mode.\n"
                             "The single-line box (right) is in 'EndFirst' mode.";
    infoLabel->config.textScale = 0.4f;
    infoLabel->config.hAlign = HorizontalAlignment::Center;
    infoLabel->config.vAlign = VerticalAlignment::Center;
    gui.addElement(std::move(infoLabel));

    // 7. Run the app
    gui.run();

    return 0;
}
