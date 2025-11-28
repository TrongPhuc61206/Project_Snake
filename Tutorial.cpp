// Tutorial.cpp

#include "Tutorial.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <algorithm> // Cho std::max


void ShowTutorial()
{
    // Tạo cửa sổ hướng dẫn với kích thước 1550x1050
    sf::RenderWindow window(
        sf::VideoMode(1550, 1050),
        "Tutorial / How To Play",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    // Tải nền hướng dẫn
    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile(TUTORIAL_GUIDE_PATH)) {
        std::cout << "ERROR: Khong the tai anh: " << TUTORIAL_GUIDE_PATH << "\n";
        return;
    }

    sf::Sprite bgSprite;
    bgSprite.setTexture(bgTexture);

    float currentScale = 1.0f; // Khai báo biến để lưu trữ hệ số scale

    // Scale ảnh để phủ đầy cửa sổ, giữ tỉ lệ, canh giữa (giống cover)
    {
        float winW = static_cast<float>(window.getSize().x);
        float winH = static_cast<float>(window.getSize().y);

        float texW = static_cast<float>(bgTexture.getSize().x);
        float texH = static_cast<float>(bgTexture.getSize().y);

        float scale = std::max(winW / texW, winH / texH);
        bgSprite.setScale(scale, scale);
        currentScale = scale; // Lưu lại scale factor

        float displayedW = texW * scale;
        float displayedH = texH * scale;

        float offsetX = (winW - displayedW) / 2.0f;
        float offsetY = (winH - displayedH) / 2.0f;

        bgSprite.setPosition(offsetX, offsetY);
    }

    // Vòng lặp chính của cửa sổ Tutorial
    while (window.isOpen())
    {
        sf::Event event;
        while (window.pollEvent(event))
        {
            // Xử lý đóng bằng nút X hoặc nhấn ESC
            if (event.type == sf::Event::Closed) {
                window.close();
            }
            if (event.type == sf::Event::KeyPressed &&
                event.key.code == sf::Keyboard::Escape)
            {
                window.close();
            }

            // --- Xử lý Click Chuột vào vùng nút BACK ---
            if (event.type == sf::Event::MouseButtonReleased)
            {
                if (event.mouseButton.button == sf::Mouse::Left)
                {
                    sf::Vector2f mouseClickPos = sf::Vector2f(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));

                    sf::FloatRect bgBounds = bgSprite.getGlobalBounds();

                    // TÍNH TOÁN VỊ TRÍ VÀ KÍCH THƯỚC CỦA NÚT BACK TRÊN MÀN HÌNH HIỂN THỊ
                    float buttonScreenX = bgBounds.left + (ORIGINAL_BUTTON_X * currentScale);
                    float buttonScreenY = bgBounds.top + (ORIGINAL_BUTTON_Y * currentScale);
                    float buttonScreenWidth = ORIGINAL_BUTTON_WIDTH * currentScale;
                    float buttonScreenHeight = ORIGINAL_BUTTON_HEIGHT * currentScale;

                    // Tạo hình chữ nhật đại diện cho vùng click
                    sf::FloatRect backButtonRect(buttonScreenX, buttonScreenY, buttonScreenWidth, buttonScreenHeight);

                    // Kiểm tra click
                    if (backButtonRect.contains(mouseClickPos))
                    {
                        window.close(); // Đóng cửa sổ hướng dẫn
                    }
                }
            }
        }

        window.clear();
        window.draw(bgSprite);
        window.display();
    }
}

