#include "Setting.h"
#include <iostream>
#include <string>

using namespace sf;
using namespace std;

// === ĐỊNH NGHĨA BIẾN TOÀN CỤC TẠI ĐÂY ===
bool setting_musicOn = true;
int setting_difficulty = 1;
int setting_snakeColor = 0;

void showSettings(RenderWindow& window) {
    // ===== FONT =====
    Font font;
    if (!font.loadFromFile("fonts/arial.ttf")) {
        cout << "Error loading font inside Settings\n";
    }

    // ===== BACKGROUND =====
    Texture bgTex;
    if (!bgTex.loadFromFile("images/Setting.jpg")) {
        cout << "Error loading Setting.jpg\n";
    }
    Sprite bg(bgTex);
    if (bgTex.getSize().x > 0 && bgTex.getSize().y > 0) {
        bg.setScale(
            (float)window.getSize().x / bgTex.getSize().x,
            (float)window.getSize().y / bgTex.getSize().y
        );
    }

    // ===== BUTTON TEXTURES =====
    Texture texMusicOn, texMusicOff;
    Texture texEasy, texNormal, texHard;
    Texture texColor[4];

    // Load textures (có thể thêm check lỗi nếu cần)
    texMusicOn.loadFromFile("images/ON.png");
    texMusicOff.loadFromFile("images/OFF.png");
    texEasy.loadFromFile("images/EASY.png");
    texNormal.loadFromFile("images/NORMAL.png");
    texHard.loadFromFile("images/HARD.png");

    string colorNames[4] = { "Green", "Purple", "Red", "Yellow" };

    for (int i = 0; i < 4; i++) {
        if (!texColor[i].loadFromFile("images/" + colorNames[i] + ".png")) {
            cout << "Missing images/" << colorNames[i] << ".png\n";
        }
    }

    // ===== SPRITES =====
    Sprite btnMusicOn(texMusicOn), btnMusicOff(texMusicOff);
    Sprite btnEasy(texEasy), btnNormal(texNormal), btnHard(texHard);
    Sprite btnColor[4];

    for (int i = 0; i < 4; i++)
        btnColor[i].setTexture(texColor[i]);

    // ===== SCALE HELPER =====
    auto scaleBtn = [](Sprite& s, float h) {
        if (s.getTexture() && s.getTexture()->getSize().y > 0) {
            float scale = h / s.getTexture()->getSize().y;
            s.setScale(scale, scale);
        }
        };

    scaleBtn(btnMusicOn, 100);
    scaleBtn(btnMusicOff, 100);
    scaleBtn(btnEasy, 150);
    scaleBtn(btnNormal, 150);
    scaleBtn(btnHard, 150);

    for (int i = 0; i < 4; i++)
        scaleBtn(btnColor[i], 90);

    // ===== POSITIONS =====
    float cx = window.getSize().x / 2.f;

    // MUSIC
    btnMusicOn.setPosition(cx - 180, 230);
    btnMusicOff.setPosition(cx + 60, 230);

    // DIFFICULTY
    btnEasy.setPosition(cx - 260, 400);
    btnNormal.setPosition(cx - 50, 400);
    btnHard.setPosition(cx + 160, 400);

    // COLOR ROW
    float startX = cx - 200;
    for (int i = 0; i < 4; i++)
        btnColor[i].setPosition(startX + i * 140, 600);

    // ===== HOVER EFFECTS =====
    auto applyHover = [](Sprite& s, bool active) {
        if (active) s.setColor(Color(255, 255, 255));
        else s.setColor(Color(200, 200, 200, 230));
        };

    // SELECTED EFFECT
    auto applySelected = [](Sprite& s) {
        s.setColor(Color(255, 255, 120));
        };

    while (window.isOpen()) {
        Event event;
        Vector2f mp = window.mapPixelToCoords(Mouse::getPosition(window));
        bool mousePressed = false;

        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) {
                window.close();
                return;
            }
            if (event.type == Event::KeyPressed && event.key.code == Keyboard::Escape)
                return;

            if (event.type == Event::MouseButtonPressed &&
                event.mouseButton.button == Mouse::Left)
            {
                mousePressed = true;
            }
        }

        // HOVER CHECK
        bool hoverMusicOn = btnMusicOn.getGlobalBounds().contains(mp);
        bool hoverMusicOff = btnMusicOff.getGlobalBounds().contains(mp);
        bool hoverEasy = btnEasy.getGlobalBounds().contains(mp);
        bool hoverNormal = btnNormal.getGlobalBounds().contains(mp);
        bool hoverHard = btnHard.getGlobalBounds().contains(mp);

        bool hoverColor[4] = { false };
        for (int i = 0; i < 4; i++)
            hoverColor[i] = btnColor[i].getGlobalBounds().contains(mp);

        // Hover visual
        applyHover(btnMusicOn, hoverMusicOn);
        applyHover(btnMusicOff, hoverMusicOff);
        applyHover(btnEasy, hoverEasy);
        applyHover(btnNormal, hoverNormal);
        applyHover(btnHard, hoverHard);

        for (int i = 0; i < 4; i++)
            applyHover(btnColor[i], hoverColor[i]);

        // CLICK & COUT LOGIC
        if (mousePressed) {
            // --- MUSIC ---
            if (hoverMusicOn) {
                setting_musicOn = true;
                cout << "Music turned ON" << endl;
            }
            if (hoverMusicOff) {
                setting_musicOn = false;
                cout << "Music turned OFF" << endl;
            }

            // --- DIFFICULTY ---
            if (hoverEasy) {
                setting_difficulty = 0;
                cout << "Difficulty set to EASY" << endl;
            }
            if (hoverNormal) {
                setting_difficulty = 1;
                cout << "Difficulty set to NORMAL" << endl;
            }
            if (hoverHard) {
                setting_difficulty = 2;
                cout << "Difficulty set to HARD" << endl;
            }

            // --- COLORS ---
            for (int i = 0; i < 4; i++) {
                if (hoverColor[i]) {
                    setting_snakeColor = i;
                    cout << "Snake Color changed to: " << colorNames[i] << endl;
                }
            }
        }

        // SELECTED HIGHLIGHT
        (setting_musicOn ? applySelected(btnMusicOn) : applySelected(btnMusicOff));

        if (setting_difficulty == 0) applySelected(btnEasy);
        if (setting_difficulty == 1) applySelected(btnNormal);
        if (setting_difficulty == 2) applySelected(btnHard);

        applySelected(btnColor[setting_snakeColor]);

        // DRAW
        window.clear();
        window.draw(bg);

        window.draw(btnMusicOn);
        window.draw(btnMusicOff);

        window.draw(btnEasy);
        window.draw(btnNormal);
        window.draw(btnHard);

        for (int i = 0; i < 4; i++)
            window.draw(btnColor[i]);

        window.display();
    }
}