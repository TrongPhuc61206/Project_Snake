// SnakeGame.cpp
#include "SnakeGame.h"
#include "Setting.h"
#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <ctime>
#include <cmath> // Thêm thư viện này để dùng hàm sin
#include <Windows.h>
#include <map>

using namespace sf;
using namespace std;

// Raw Input để bypass Unikey
static map<int, bool> rawKeyStates;
static WNDPROC originalWndProc = nullptr;

LRESULT CALLBACK RawInputProcGame(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INPUT) {
        UINT size = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));
        vector<BYTE> buffer(size);
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer.data(), &size, sizeof(RAWINPUTHEADER)) == size) {
            RAWINPUT* raw = (RAWINPUT*)buffer.data();
            if (raw->header.dwType == RIM_TYPEKEYBOARD) {
                int vkey = raw->data.keyboard.VKey;
                bool isDown = !(raw->data.keyboard.Flags & RI_KEY_BREAK);
                rawKeyStates[vkey] = isDown;
                if (vkey == 'W' && isDown) {
                    cout << "Raw Input detected W key press!" << endl;
                }
            }
        }
    }
    if (originalWndProc) {
        return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void RegisterRawInputGame(HWND hwnd) {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x06;
    // Chỉ dùng INPUTSINK, không dùng NOLEGACY để không chặn SFML
    rid.dwFlags = RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;
    if (RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        cout << "Raw Input registered for SnakeGame (bypass Unikey)" << endl;
        originalWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)RawInputProcGame);
    } else {
        cout << "Failed to register Raw Input!" << endl;
    }
}

void UnregisterRawInputGame(HWND hwnd) {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x06;
    rid.dwFlags = RIDEV_REMOVE;
    rid.hwndTarget = nullptr;
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
    if (originalWndProc) {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
        originalWndProc = nullptr;
    }
    rawKeyStates.clear();
}

bool IsRawKeyPressedGame(int vkey) {
    return rawKeyStates.count(vkey) && rawKeyStates[vkey];
}

// --- HÀM LẤY MÀU RẮN ---
Color getSnakeBodyColor() {
    switch (setting_snakeColor) {
        case 0: return Color::Green;
        case 1: return Color(186, 85, 211);  // Purple (Medium Orchid - sáng hơn nhiều)
        case 2: return Color::Red;
        case 3: return Color::Yellow;
        default: return Color::Green;
    }
}

Color getSnakeHeadColor() {
    switch (setting_snakeColor) {
        case 0: return Color::Yellow;        // Green snake -> Yellow head
        case 1: return Color(200, 100, 255); // Purple snake -> Light purple head
        case 2: return Color(255, 100, 100); // Red snake -> Light red head
        case 3: return Color(255, 255, 150); // Yellow snake -> Light yellow head
        default: return Color::Yellow;
    }
}

// --- CÁC HÀM HỖ TRỢ (Move, Reset, Spawn) ---

void MoveLeft(std::vector<sf::RectangleShape>& snake, float snakeSize)
{
    sf::Vector2f headPos = snake.back().getPosition();
    sf::RectangleShape newHead(sf::Vector2f(snakeSize, snakeSize));
    newHead.setPosition(headPos.x - snakeSize, headPos.y);
    newHead.setFillColor(getSnakeHeadColor());

    snake.back().setFillColor(getSnakeBodyColor());
    snake.push_back(newHead);
    snake.erase(snake.begin());
}

void MoveRight(std::vector<sf::RectangleShape>& snake, float snakeSize)
{
    sf::Vector2f headPos = snake.back().getPosition();
    sf::RectangleShape newHead(sf::Vector2f(snakeSize, snakeSize));
    newHead.setPosition(headPos.x + snakeSize, headPos.y);
    newHead.setFillColor(getSnakeHeadColor());

    snake.back().setFillColor(getSnakeBodyColor());
    snake.push_back(newHead);
    snake.erase(snake.begin());
}

void MoveUp(std::vector<sf::RectangleShape>& snake, float snakeSize)
{
    sf::Vector2f headPos = snake.back().getPosition();
    sf::RectangleShape newHead(sf::Vector2f(snakeSize, snakeSize));
    newHead.setPosition(headPos.x, headPos.y - snakeSize);
    newHead.setFillColor(getSnakeHeadColor());

    snake.back().setFillColor(getSnakeBodyColor());
    snake.push_back(newHead);
    snake.erase(snake.begin());
}

void MoveDown(std::vector<sf::RectangleShape>& snake, float snakeSize)
{
    sf::Vector2f headPos = snake.back().getPosition();
    sf::RectangleShape newHead(sf::Vector2f(snakeSize, snakeSize));
    newHead.setPosition(headPos.x, headPos.y + snakeSize);
    newHead.setFillColor(getSnakeHeadColor());

    snake.back().setFillColor(getSnakeBodyColor());
    snake.push_back(newHead);
    snake.erase(snake.begin());
}

void resetSnake(vector<RectangleShape>& snake, const FloatRect& playArea, float size)
{
    snake.clear();

    Vector2f start(playArea.left + playArea.width / 2,
        playArea.top + playArea.height / 2);

    RectangleShape t(Vector2f(size, size));
    t.setFillColor(getSnakeBodyColor());
    t.setPosition(start.x - 2 * size, start.y);
    snake.push_back(t);

    RectangleShape b(Vector2f(size, size));
    b.setFillColor(getSnakeBodyColor());
    b.setPosition(start.x - size, start.y);
    snake.push_back(b);

    RectangleShape h(Vector2f(size, size));
    h.setFillColor(getSnakeHeadColor());
    h.setPosition(start);
    snake.push_back(h);
}

void spawnApple(Sprite& apple, const FloatRect& area, float blockSize,
    const vector<RectangleShape>& snake)
{
    bool valid = false;

    while (!valid)
    {
        float gridX = (rand() % (int)(area.width / blockSize)) * blockSize;
        float gridY = (rand() % (int)(area.height / blockSize)) * blockSize;

        Vector2f pos(area.left + gridX, area.top + gridY);

        valid = true;
        for (const auto& s : snake)
        {
            if (s.getGlobalBounds().intersects(FloatRect(pos.x, pos.y, blockSize, blockSize)))
            {
                valid = false;
                break;
            }
        }

        if (valid)
            apple.setPosition(pos);
    }
}

// --- HÀM CHÍNH CỦA GAME ---
void newGameStart()
{
    cout << "=== Starting New Game ===" << endl;
    cout << "Snake Color Setting: " << setting_snakeColor << endl;
    srand((unsigned)time(NULL));

    unsigned int windowWidth = 1545;
    unsigned int windowHeight = 1085;
    RenderWindow window(VideoMode(windowWidth, windowHeight), "Snake Game");
    
    // Đăng ký Raw Input để bypass Unikey
    HWND hwnd = window.getSystemHandle();
    RegisterRawInputGame(hwnd);

    Texture backgroundTexture;
    if (!backgroundTexture.loadFromFile("images/Mau.png"))
    {
        cout << "Error loading background\n";
        return; // Thay return -1 bằng return void
    }

    Sprite backgroundSprite(backgroundTexture);
    Vector2u texSize = backgroundTexture.getSize();
    backgroundSprite.setScale(
        (float)windowWidth / texSize.x,
        (float)windowHeight / texSize.y
    );

    FloatRect playArea(300.f, 150.f, 860.f, 660.f);

    RectangleShape debugBoundary;
    debugBoundary.setPosition(playArea.left, playArea.top);
    debugBoundary.setSize(Vector2f(playArea.width, playArea.height));
    debugBoundary.setFillColor(Color::Transparent);
    debugBoundary.setOutlineColor(Color::Red);
    debugBoundary.setOutlineThickness(2.f);

    float snakeSize = 20.f;
    vector<RectangleShape> snake;

    // Khởi tạo rắn ban đầu
    resetSnake(snake, playArea, snakeSize);

    // ====== Load Apple Image ======
    Texture appleTex;
    if (!appleTex.loadFromFile("images/Apple.png"))
    {
        cout << "Error loading Apple.png\n";
        return;
    }
    Sprite appleSprite(appleTex);
    float appleScaleFactor = 1.5f;
    appleSprite.setScale(
        snakeSize / appleTex.getSize().x * appleScaleFactor,
        snakeSize / appleTex.getSize().y * appleScaleFactor
    );

    spawnApple(appleSprite, playArea, snakeSize, snake);

    // ====== SCORE IMAGE ======
    Texture scoreTex;
    if (!scoreTex.loadFromFile("images/Score.png")) {
        cout << "Error loading Score.png\n";
        return;
    }

    Sprite scoreSprite(scoreTex);
    float scoreSize = 300.f;
    float scoreScaleFactor = scoreSize / std::max((float)scoreTex.getSize().x, (float)scoreTex.getSize().y);
    scoreSprite.setScale(scoreScaleFactor, scoreScaleFactor);
    scoreSprite.setPosition(5.f, 5.f);

    // ====== SCORE TEXT ======
    int score = 0;

    Font scoreFont;
    if (!scoreFont.loadFromFile("fonts/Orbitron-VariableFont_wght.ttf")) {
        cout << "Error loading font\n";
        return;
    }

    Text scoreText;
    scoreText.setFont(scoreFont);
    scoreText.setCharacterSize(40);
    scoreText.setFillColor(Color::White);
    scoreText.setStyle(Text::Bold);

    scoreText.setPosition(
        scoreSprite.getPosition().x + scoreSprite.getGlobalBounds().width / 2 - 15.f,
        scoreSprite.getPosition().y + scoreSprite.getGlobalBounds().height / 2 - 25.f
    );
    scoreText.setString("0");

    // ====== PAUSE BUTTON ======
    Texture pauseTex;
    if (!pauseTex.loadFromFile("images/Pause.png")) {
        cout << "Error loading Pause.png\n";
        return;
    }

    Sprite pauseSprite(pauseTex);
    float pauseSize = 250.f;
    float baseScale = pauseSize / std::max(pauseTex.getSize().x, pauseTex.getSize().y);
    pauseSprite.setScale(baseScale, baseScale);

    pauseSprite.setPosition(windowWidth - pauseSize - 5 + 10.f, 5);

    bool isPaused = false;
    Vector2f direction(snakeSize, 0.f);
    Vector2f pendingDirection = direction; // Hướng chờ xử lý
    Clock clock;
    Time updateInterval = milliseconds(150);
    bool isGameOver = false;
    Clock unpauseClock; // Clock để xử lý delay sau unpause
    bool justUnpaused = false;

    // Thêm clock riêng cho animation để không bị reset bởi game loop
    Clock animClock;

    while (window.isOpen())
    {
        Event event;
        while (window.pollEvent(event))
        {
            if (event.type == Event::Closed)
                window.close();

            if (event.type == Event::KeyPressed)
            {
                if (event.key.code == Keyboard::Escape) {
                    UnregisterRawInputGame(hwnd); // Hủy Raw Input trước khi thoát
                    window.close(); // Đóng window trước khi return
                    return; // Thoát về menu chính
                }
            }
            if (event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Left) {
                Vector2f mousePos(event.mouseButton.x, event.mouseButton.y);
                if (pauseSprite.getGlobalBounds().contains(mousePos)) {
                    bool wasPaused = isPaused;
                    isPaused = !isPaused;
                    // Nếu vừa unpause, đợi người dùng thả tất cả các phím trước khi nhận input mới
                    if (wasPaused && !isPaused) {
                        justUnpaused = true;
                        unpauseClock.restart();
                    }
                }
            }
        }

        // Kiểm tra phím bấm trực tiếp (real-time input) - dùng Raw Input bypass Unikey
        if (!isPaused && !isGameOver) {
            // Nếu vừa unpause, chờ người dùng thả hết phím trước
            if (justUnpaused) {
                if (!IsRawKeyPressedGame('W') && 
                    !IsRawKeyPressedGame('A') && 
                    !IsRawKeyPressedGame('S') && 
                    !IsRawKeyPressedGame('D')) {
                    justUnpaused = false;
                }
            }
            
            // Chỉ nhận input khi không còn trong trạng thái justUnpaused
            if (!justUnpaused) {
                // Kiểm tra từng phím độc lập, dựa vào pendingDirection thay vì direction
                bool wPressed = IsRawKeyPressedGame('W');
                if (wPressed && pendingDirection.y == 0) {
                    cout << "W accepted! Changing direction to UP" << endl;
                    pendingDirection = Vector2f(0, -snakeSize);
                } else if (wPressed) {
                    cout << "W pressed but blocked (pendingDirection.y != 0)" << endl;
                }
                if (IsRawKeyPressedGame('S') && pendingDirection.y == 0) {
                    pendingDirection = Vector2f(0, snakeSize);
                }
                if (IsRawKeyPressedGame('A') && pendingDirection.x == 0) {
                    pendingDirection = Vector2f(-snakeSize, 0);
                }
                if (IsRawKeyPressedGame('D') && pendingDirection.x == 0) {
                    pendingDirection = Vector2f(snakeSize, 0);
                }
            } else {
                cout << "Input blocked - justUnpaused is true" << endl;
            }
        }

        if (!isGameOver && !isPaused && clock.getElapsedTime() > updateInterval)
        {
            // Áp dụng hướng mới
            direction = pendingDirection;
            
            Vector2f newHeadPos = snake.back().getPosition() + direction;
            float margin = debugBoundary.getOutlineThickness();

            if (newHeadPos.x < playArea.left + margin ||
                newHeadPos.x + snakeSize > playArea.left + playArea.width - margin ||
                newHeadPos.y < playArea.top + margin ||
                newHeadPos.y + snakeSize > playArea.top + playArea.height - margin)
            {
                isGameOver = true;

                for (auto& s : snake)
                    s.setFillColor(Color::Red);

                window.clear();
                window.draw(backgroundSprite);
                for (auto& s : snake)
                    window.draw(s);
                window.draw(appleSprite);
                window.draw(debugBoundary);
                window.display();
                sleep(seconds(0.5f));

                resetSnake(snake, playArea, snakeSize);
                score = 0;
                scoreText.setString(to_string(score));
                spawnApple(appleSprite, playArea, snakeSize, snake);
                direction = Vector2f(snakeSize, 0.f);
                pendingDirection = direction;
                isGameOver = false;
            }
            else
            {
                if (direction.x < 0) MoveLeft(snake, snakeSize);
                else if (direction.x > 0) MoveRight(snake, snakeSize);
                else if (direction.y < 0) MoveUp(snake, snakeSize);
                else if (direction.y > 0) MoveDown(snake, snakeSize);

                if (snake.back().getGlobalBounds().intersects(appleSprite.getGlobalBounds()))
                {
                    RectangleShape grow(Vector2f(snakeSize, snakeSize));
                    grow.setFillColor(getSnakeBodyColor());
                    grow.setPosition(snake.front().getPosition());
                    snake.insert(snake.begin(), grow);

                    score += 10;
                    scoreText.setString(to_string(score));

                    spawnApple(appleSprite, playArea, snakeSize, snake);
                }
            }
            clock.restart();
        }

        window.clear();
        window.draw(backgroundSprite);
        window.draw(appleSprite);
        for (auto& s : snake)
            window.draw(s);
        window.draw(debugBoundary);

        Vector2i mousePixelPos = Mouse::getPosition(window);
        Vector2f mouseWorldPos = window.mapPixelToCoords(mousePixelPos);

        // Hiệu ứng hover cho nút Pause (dùng animClock để mượt hơn)
        if (pauseSprite.getGlobalBounds().contains(mouseWorldPos)) {
            float alpha = 200 + 55 * sin(animClock.getElapsedTime().asSeconds() * 5.f);
            pauseSprite.setColor(Color(255, 255, 255, static_cast<sf::Uint8>(alpha)));
        }
        else {
            pauseSprite.setColor(Color::White);
        }

        window.draw(pauseSprite);
        window.draw(scoreSprite);
        window.draw(scoreText);
        window.display();
    }
}