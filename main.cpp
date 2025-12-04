// SnakeVector_Full.cpp — C++17 / Visual Studio (Windows console)
// Reorganized version with better structure

#define NOMINMAX
#include <windows.h>
#include <conio.h>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <map>
#include <iostream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <fstream>
#include <chrono>
#include <cctype>
#include <functional>
#include <algorithm>
#include <mmsystem.h>
#include <SFML/Graphics.hpp>


#include "Tutorial.h"
#include "SnakeGame.h"
#include "Rank.h"
#include "Setting.h"

#pragma comment(lib, "winmm.lib")

using namespace std;
using namespace sf;

// ===== HELPER FUNCTIONS FOR DYNAMIC SNAKE COLOR =====
Color getSnakeBodyColorMain() {
    switch (setting_snakeColor) {
    case 0: return Color::Green;           // Xanh lá
    case 1: return Color(186, 85, 211);    // Tím (Medium Orchid)
    case 2: return Color::Red;             // Đỏ
    case 3: return Color::Yellow;          // Vàng
    default: return Color::Green;
    }
}

Color getSnakeHeadColorMain() {
    switch (setting_snakeColor) {
    case 0: return Color(144, 238, 144);   // Xanh nhạt
    case 1: return Color(221, 160, 221);   // Tím nhạt
    case 2: return Color(255, 99, 71);     // Đỏ nhạt
    case 3: return Color(255, 255, 153);   // Vàng nhạt
    default: return Color(144, 238, 144);
    }
}

Color getSnakeDarkBodyColorMain() {
    switch (setting_snakeColor) {
    case 0: return Color(0, 150, 0);       // Xanh đậm
    case 1: return Color(147, 51, 170);    // Tím đậm
    case 2: return Color(178, 34, 34);     // Đỏ đậm
    case 3: return Color(204, 204, 0);     // Vàng đậm
    default: return Color(0, 150, 0);
    }
}

// ===== CONSTANTS & ENUMS =====
const int MAX_SPEED = 8;
const int FOOD_COUNT = 4;
const string HIGHSCORE_FILE = "highscore.txt";

// Direction constants (thay thế enum Direction)
const int DIR_LEFT = 0;
const int DIR_RIGHT = 1;
const int DIR_UP = 2;
const int DIR_DOWN = 3;

// GameMode constants (thay thế enum GameMode)
const int MODE_CLASSIC = 0;
const int MODE_SURVIVAL = 1;
const int MODE_TIMEATTACK = 2;

// ===== STRUCTS & CLASSES =====
struct GameResources {
    Texture appleTexture;
    Texture contextTexture;
    Texture frameTexture;
    Texture scoreTexture;
    Texture pauseTexture;
    bool loaded = false;
    
    bool loadAll() {
        if (loaded) return true;
        bool success = true;
        success &= appleTexture.loadFromFile("images/Apple.png");
        success &= contextTexture.loadFromFile("images/Context.png");
        success &= frameTexture.loadFromFile("images/Mau.png");
        success &= scoreTexture.loadFromFile("images/Score.png");
        success &= pauseTexture.loadFromFile("images/Pause.png");
        loaded = success;
        return success;
    }
};

struct GameObject {
    POINT position;
    char symbol;
    int color;
    bool active;
    string type;

    GameObject(POINT pos = { 0,0 }, char sym = ' ', int col = 7, string t = "default")
        : position(pos), symbol(sym), color(col), active(true), type(t) {
    }
};

struct SnakeSegment : GameObject {
    SnakeSegment(POINT pos) : GameObject(pos, 'O', 10, "snake") {}
};

struct Food : GameObject {
    int value;
    Food(POINT pos, int val = 10) : GameObject(pos, '@', 12, "food"), value(val) {}
};

struct PowerUp : GameObject {
    string effect;
    int duration;
    PowerUp(POINT pos, string eff, int dur) : GameObject(pos, '*', 14, "powerup"), effect(eff), duration(dur) {}
};

struct MapData {
    int width, height;
    vector<vector<char>> tiles;
    POINT startPos;
    string themeName;
    int backgroundColor;
};


// ===== GLOBAL VARIABLES =====
// Game State
int state = 0;
int moving = DIR_RIGHT;
int locked = DIR_LEFT;
int speedLevel = 1;
bool keepLengthWhenLevelUp = true;
bool directionChanged = false;  // Flag để ngăn multiple direction changes trong 1 frame

// Screen & Map
int WIDTH_CONSOLE = 70;
int HEIGH_CONSOLE = 20;
vector<MapData> levelMaps;
int currentLevelMap = 0;

// Score System
int currentScore = 0;
int highScore = 0;
int currentMode = MODE_CLASSIC;

// Game Objects
vector<POINT> snake;
vector<POINT> foods;
POINT gatePos{ -1,-1 };
bool gateActive = false;
bool foodVisible = true;
int foodIndex = 0;

// ===== THREAD & ASYNC GLOBALS =====
mutex soundMutex;
mutex saveMutex;
queue<string> soundQueue;
bool shouldExit = false;
atomic<bool> needsAutoSave(false);
string currentPlayerName = "";
vector<RectangleShape> lastSafeSnake;
Vector2f lastSafeApplePos;
int lastSafeScore = 0;

// Raw Input globals
map<int, bool> rawKeyStates;
WNDPROC originalWndProc = nullptr;

// ===== THREAD WORKERS =====
void SoundWorker() {
    while (!shouldExit) {
        this_thread::sleep_for(chrono::milliseconds(100));
        lock_guard<mutex> lock(soundMutex);
        while (!soundQueue.empty()) {
            string soundFile = soundQueue.front();
            soundQueue.pop();
            // Play sound nếu cần
        }
    }
}

void AutoSaveWorker() {
    while (!shouldExit) {
        this_thread::sleep_for(chrono::seconds(10));
        if (needsAutoSave.load() && !currentPlayerName.empty()) {
            lock_guard<mutex> lock(saveMutex);
            // Auto save logic sẽ được gọi từ game loop
            needsAutoSave.store(false);
        }
    }
}

// Raw Input Window Procedure
LRESULT CALLBACK RawInputProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
            }
        }
    }
    
    // GỌI LẠI window procedure GỐC của SFML để không làm hỏng SFML
    if (originalWndProc) {
        return CallWindowProc(originalWndProc, hwnd, msg, wParam, lParam);
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Đăng ký Raw Input cho keyboard
void RegisterRawInput(HWND hwnd) {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;  // Generic Desktop
    rid.usUsage = 0x06;      // Keyboard
    // RIDEV_NOLEGACY: Chặn WM_KEYDOWN, WM_KEYUP, WM_CHAR -> bypass Unikey hoàn toàn
    rid.dwFlags = RIDEV_NOLEGACY | RIDEV_INPUTSINK;
    rid.hwndTarget = hwnd;
    
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
        cout << "Failed to register raw input device!\n";
    } else {
        cout << "Raw Input registered with NOLEGACY flag\n";
    }
    
    // Lưu window procedure gốc của SFML và set procedure mới
    originalWndProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_WNDPROC);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)RawInputProc);
}

// Hủy đăng ký Raw Input
void UnregisterRawInput(HWND hwnd) {
    RAWINPUTDEVICE rid;
    rid.usUsagePage = 0x01;  // Generic Desktop
    rid.usUsage = 0x06;      // Keyboard
    rid.dwFlags = RIDEV_REMOVE;  // Gỡ bỏ device
    rid.hwndTarget = nullptr;
    
    RegisterRawInputDevices(&rid, 1, sizeof(rid));
    
    // Khôi phục window procedure gốc
    if (originalWndProc) {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)originalWndProc);
        originalWndProc = nullptr;
    }
    
    // Clear key states
    rawKeyStates.clear();
}

// Check phím từ Raw Input
bool IsRawKeyPressed(int vkey) {
    return rawKeyStates.count(vkey) && rawKeyStates[vkey];
}

// ===== FORWARD DECLARATIONS =====
void InitializeLevelMaps();
MapData& GetCurrentMap();
bool Occupied(const POINT& p);
void GenerateFoods();
void DrawMapObstacles();
void ResetData();
void GameLoop();
bool Opposite(int dir1, int dir2);
void ShowHighScores(RenderWindow& window);

// ===== SAFE TILE ACCESS HELPERS =====
bool IsValidTilePos(const MapData& map, int x, int y);
void SetTile(MapData& map, int x, int y, char tile);
char GetTile(const MapData& map, int x, int y);

// ===== CONSOLE UTILITIES =====
// Cố định kích thước cửa sổ console, không cho phép thay đổi kích thước
void FixConsoleWindow() {
    HWND wnd = GetConsoleWindow();
    LONG style = GetWindowLong(wnd, GWL_STYLE);
    style &= ~(WS_MAXIMIZEBOX) & ~(WS_THICKFRAME);
    SetWindowLong(wnd, GWL_STYLE, style);
}

// Ẩn hoặc hiển thị con trỏ chuột trong console
void HideCursor(bool hide = true) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO info{ 25, hide };
    SetConsoleCursorInfo(h, &info);
}

// Tắt chế độ Quick Edit để tránh game bị pause khi click chuột
void DisableQuickEdit() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hIn, &mode);
    mode &= ~ENABLE_QUICK_EDIT_MODE;
    SetConsoleMode(hIn, mode);
}

// Di chuyển con trỏ console đến vị trí (x, y) để vẽ
void GotoXY(int x, int y) {
    COORD c; c.X = (SHORT)x; c.Y = (SHORT)y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), c);
}

// Đổi màu chữ trong console
void SetColor(int color) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(h, color);
}

// Vẽ text có màu tại vị trí (x, y)
void DrawColoredText(int x, int y, const string& text, int color) {
    SetColor(color);
    GotoXY(x, y);
    cout << text;
    SetColor(7); // Reset về màu trắng
}

// Vẽ khung viền trò chơi với ký tự 'X'
void DrawBoard(int x, int y, int w, int h) {
    GotoXY(x, y); cout << 'X'; // Góc trên trái
    for (int i = 1; i < w; i++) cout << 'X'; // Viền trên
    cout << 'X'; // Góc trên phải
    GotoXY(x, h + y); cout << 'X'; // Góc dưới trái
    for (int i = 1; i < w; i++) cout << 'X'; // Viền dưới
    cout << 'X'; // Góc dưới phải
    for (int i = y + 1; i < h + y; i++) {
        GotoXY(x, i); cout << 'X'; // Viền trái
        GotoXY(x + w, i); cout << 'X'; // Viền phải
    }
}

// Hiển thị thông tin ở phía dưới màn hình game
void PrintBottom(const string& s, bool clearLine = true) {
    if (clearLine) {
        GotoXY(0, HEIGH_CONSOLE + 2);
        cout << string(120, ' '); // Xóa dòng cũ
    }
    GotoXY(0, HEIGH_CONSOLE + 2);
    cout << s;
}

// ===== INPUT HANDLING =====
// Chuyển đổi phím bấm thành hướng di chuyển (hỗ trợ WASD và phím mũi tên)
int GetDirectionFromKey(int key) {
    switch (key) {
    case 'A': case 'a': return DIR_LEFT;   // A hoặc a = trái
    case 'D': case 'd': return DIR_RIGHT;  // D hoặc d = phải
    case 'W': case 'w': return DIR_UP;     // W hoặc w = lên
    case 'S': case 's': return DIR_DOWN;   // S hoặc s = xuống
    case 72: return DIR_UP;    // Phím mũi tên lên
    case 80: return DIR_DOWN;  // Phím mũi tên xuống
    case 75: return DIR_LEFT;  // Phím mũi tên trái
    case 77: return DIR_RIGHT; // Phím mũi tên phải
    default: return -1;        // Phím không hợp lệ
    }
}

// Kiểm tra xem có thể thay đổi hướng di chuyển không (tránh đi ngược lại)
bool CanChangeDirection(int newDir, int currentDir, size_t snakeLength) {
    // Rắn quá ngắn (≤2 đoạn) thì có thể đi bất kỳ hướng nào
    if (snakeLength <= 2) return true;

    // Rắn dài thì không cho đi ngược lại hướng hiện tại
    return !Opposite(newDir, currentDir);
}

// ===== SOUND SYSTEM =====
// Phát âm thanh cho các sự kiện trong game (ăn mồi, lên level, chết)
void PlayGameSound(const string& sound) {
    if (sound == "eat") Beep(800, 100);        // Ăn mồi: 800Hz, 100ms
    else if (sound == "levelup") Beep(1000, 200); // Lên level: 1000Hz, 200ms
    else if (sound == "death") Beep(300, 500);     // Chết: 300Hz, 500ms
}

// ===== SCORE SYSTEM =====
// Đọc điểm số cao nhất từ file
void LoadHighScore() {
    ifstream file(HIGHSCORE_FILE);
    if (file) {
        file >> highScore;
    }
}

// Lưu điểm số cao nhất vào file nếu phá kỷ lục
void SaveHighScore() {
    if (currentScore > highScore) {
        highScore = currentScore;
        ofstream file(HIGHSCORE_FILE);
        file << highScore;
    }
}

// Cập nhật điểm số hiện tại và kiểm tra kỷ lục
void UpdateScore(int points) {
    currentScore += points;
    if (currentScore > highScore) {
        highScore = currentScore;
    }
}

// ===== HIGH SCORE SYSTEM =====
// Lưu thông tin game vào bảng xếp hạng

// ===== MAP SYSTEM =====
// Tạo và khởi tạo tất cả các bản đồ cho từng level với độ khó tăng dần
void InitializeLevelMaps() {
    levelMaps.clear();

    // ===== LEVEL 1: PEACEFUL GARDEN ===== 
    MapData map1;
    map1.width = 70; map1.height = 20;
    map1.startPos = { 35, 10 }; // Giữa màn hình
    map1.themeName = "Peaceful Garden";
    map1.backgroundColor = 2; // Xanh lá
    map1.tiles.resize(map1.height, vector<char>(map1.width, ' '));

    // Tường đơn giản ở các góc (TRÁNH dòng 5 - dòng spawn rắn)
    for (int i = 5; i <= 10; i++) {
        SetTile(map1, i, 3, '#');         // Góc trên trái (dòng 3 thay vì 5)
        SetTile(map1, i, 4, '#');         // Góc trên trái (dòng 4)
        SetTile(map1, 60 + i, 14, '#');   // Góc dưới phải  
    }
    levelMaps.push_back(map1);

    // ===== LEVEL 2: ANCIENT RUINS =====
    MapData map2;
    map2.width = 70; map2.height = 20;
    map2.startPos = { 35, 10 };
    map2.themeName = "Ancient Ruins";
    map2.backgroundColor = 8; // Xám
    map2.tiles.resize(map2.height, vector<char>(map2.width, ' '));

    // Các cột đá cổ (sử dụng SetTile an toàn)
    for (int y = 6; y <= 8; y++) {
        SetTile(map2, 15, y, '#'); // Cột trái
        SetTile(map2, 55, y, '#'); // Cột phải
    }
    for (int y = 12; y <= 14; y++) {
        SetTile(map2, 25, y, '#'); // Cột trái dưới
        SetTile(map2, 45, y, '#'); // Cột phải dưới
    }
    // Tường ngang ở giữa (có lỗ hổng)
    for (int x = 20; x <= 30; x++) {
        SetTile(map2, x, 10, '#');
    }
    for (int x = 40; x <= 50; x++) {
        SetTile(map2, x, 10, '#');
    }
    levelMaps.push_back(map2);

    // ===== LEVEL 3: CRYSTAL CAVES =====
    MapData map3;
    map3.width = 70; map3.height = 20;
    map3.startPos = { 35, 10 };
    map3.themeName = "Crystal Caves";
    map3.backgroundColor = 1; // Xanh dương
    map3.tiles.resize(map3.height, vector<char>(map3.width, ' '));

    // Mê cung tinh thể hình chữ thập (TRÁNH dòng 5 - dòng spawn rắn)
    for (int x = 30; x <= 40; x++) {
        SetTile(map3, x, 7, '#');  // Ngang trên
        SetTile(map3, x, 13, '#'); // Ngang dưới
    }
    // Cột dọc KHÔNG đi qua dòng 5
    for (int y = 2; y <= 4; y++) {       // Phần trên dòng 5
        SetTile(map3, 20, y, '#'); // Dọc trái
        SetTile(map3, 50, y, '#'); // Dọc phải
    }
    for (int y = 6; y <= 16; y++) {      // Phần dưới dòng 5
        SetTile(map3, 20, y, '#'); // Dọc trái
        SetTile(map3, 50, y, '#'); // Dọc phải
    }
    // Các khối crystal nhỏ (TRÁNH dòng 5)
    SetTile(map3, 35, 3, '#');     // Thay vì dòng 5 → dòng 3
    SetTile(map3, 35, 15, '#');
    SetTile(map3, 25, 10, '#');
    SetTile(map3, 45, 10, '#');
    levelMaps.push_back(map3);

    // ===== LEVEL 4: LAVA TEMPLE =====
    MapData map4;
    map4.width = 70; map4.height = 20;
    map4.startPos = { 35, 10 };
    map4.themeName = "Lava Temple";
    map4.backgroundColor = 4; // Đỏ
    map4.tiles.resize(map4.height, vector<char>(map4.width, ' '));

    // Mê cung phức tạp hình kim cương (TRÁNH dòng 5 - dòng spawn rắn)
    for (int i = 0; i < 10; i++) {
        // Kim cương trên - BẮT ĐẦU TỪ dòng 6 thay vì 5
        if (6 + i != 5) {  // Đảm bảo không chạm dòng 5
            SetTile(map4, 35 - i, 6 + i, '#');
            SetTile(map4, 35 + i, 6 + i, '#');
        }
        // Kim cương dưới (ngược lại)
        if (15 - i != 5) {  // Đảm bảo không chạm dòng 5
            SetTile(map4, 35 - i, 15 - i, '#');
            SetTile(map4, 35 + i, 15 - i, '#');
        }
    }
    // Tường chắn ở các cạnh (sử dụng SetTile an toàn)
    for (int x = 10; x <= 15; x++) {
        SetTile(map4, x, 8, '#');
        SetTile(map4, x, 12, '#');
        SetTile(map4, 55 + x - 10, 8, '#');
        SetTile(map4, 55 + x - 10, 12, '#');
    }
    levelMaps.push_back(map4);

    // ===== LEVEL 5: NIGHTMARE DIMENSION =====
    MapData map5;
    map5.width = 70; map5.height = 20;
    map5.startPos = { 35, 10 };
    map5.themeName = "Nightmare Dimension";
    map5.backgroundColor = 5; // Tím
    map5.tiles.resize(map5.height, vector<char>(map5.width, ' '));

    // Mê cung cực khó - Spiral of Death (simplified)
    for (int layer = 0; layer < 3; layer++) {  // Giảm từ 7 xuống 3 layers
        int size = 4 + layer * 3;              // Giảm size
        int centerX = 35, centerY = 10;

        // Vẽ hình vuông xoắn ốc (TRÁNH dòng 5 - dòng spawn rắn)
        for (int i = 0; i < size; i++) {
            int topY = centerY - size / 2;
            int bottomY = centerY + size / 2;
            int leftX = centerX - size / 2;
            int rightX = centerX + size / 2;

            // Trên (tránh dòng 5)
            if (topY != 5) {
                SetTile(map5, leftX + i, topY, '#');
            }

            // Dưới (tránh dòng 5) 
            if (bottomY != 5) {
                SetTile(map5, leftX + i, bottomY, '#');
            }

            // Trái (tránh dòng 5)
            if (topY + i != 5) {
                SetTile(map5, leftX, topY + i, '#');
            }

            // Phải (tránh dòng 5)
            if (topY + i != 5) {
                SetTile(map5, rightX, topY + i, '#');
            }
        }
    }

    // Thêm các chướng ngại vật nhỏ ở góc (sử dụng SetTile an toàn)
    SetTile(map5, 10, 3, '#');
    SetTile(map5, 11, 3, '#');
    SetTile(map5, 60, 17, '#');
    SetTile(map5, 59, 17, '#');
    SetTile(map5, 65, 6, '#');
    SetTile(map5, 65, 7, '#');
    SetTile(map5, 5, 14, '#');
    SetTile(map5, 5, 15, '#');

    levelMaps.push_back(map5);
}

// Lấy bản đồ tương ứng với level hiện tại
MapData& GetCurrentMap() {
    if (levelMaps.empty()) InitializeLevelMaps();
    int mapIndex = (speedLevel - 1) % levelMaps.size(); // Lặp lại map khi hết
    return levelMaps[mapIndex];
}

// Vẽ các chướng ngại vật của bản đồ hiện tại
void DrawMapObstacles() {
    MapData& currentMap = GetCurrentMap();
    SetColor(currentMap.backgroundColor);

    // Sử dụng GetTile an toàn
    for (int y = 0; y < currentMap.height; y++) {
        for (int x = 0; x < currentMap.width; x++) {
            char tile = GetTile(currentMap, x, y);
            if (tile == '#') {
                GotoXY(x, y); putchar('#'); // Vẽ chướng ngại vật
            }
        }
    }
    SetColor(7); // Reset màu về trắng
}

// ===== SAFE TILE ACCESS HELPERS =====
// Kiểm tra vị trí có hợp lệ trong tiles array không
bool IsValidTilePos(const MapData& map, int x, int y) {
    return (y >= 0 && y < (int)map.tiles.size() &&
        x >= 0 && x < (int)map.tiles[y].size() &&
        x < map.width && y < map.height);
}

// Set tile an toàn
void SetTile(MapData& map, int x, int y, char tile) {
    if (IsValidTilePos(map, x, y)) {
        map.tiles[y][x] = tile;
    }
}

// Get tile an toàn
char GetTile(const MapData& map, int x, int y) {
    if (IsValidTilePos(map, x, y)) {
        return map.tiles[y][x];
    }
    return ' '; // Trả về space nếu ngoài phạm vi
}

// ===== GAME UTILITIES =====
// Kiểm tra xem hai hướng có ngược nhau không
bool Opposite(int a, int b) {
    return (a == DIR_LEFT && b == DIR_RIGHT) ||
        (a == DIR_RIGHT && b == DIR_LEFT) ||
        (a == DIR_UP && b == DIR_DOWN) ||
        (a == DIR_DOWN && b == DIR_UP);
}

// ===== MOVEMENT FUNCTIONS =====
// Di chuyển rắn sang trái (thêm đầu mới bên trái)
void MoveLeft() {
    POINT head = snake.back(); // Lấy đầu rắn hiện tại
    head.x--;                  // Giảm tọa độ x (sang trái)
    snake.push_back(head);     // Thêm đầu mới vào cuối vector
}

// Di chuyển rắn sang phải (thêm đầu mới bên phải)
void MoveRight() {
    POINT head = snake.back(); // Lấy đầu rắn hiện tại
    head.x++;                  // Tăng tọa độ x (sang phải)
    snake.push_back(head);     // Thêm đầu mới vào cuối vector
}

// Di chuyển rắn lên trên (thêm đầu mới phía trên)
void MoveUp() {
    POINT head = snake.back(); // Lấy đầu rắn hiện tại
    head.y--;                  // Giảm tọa độ y (lên trên)
    snake.push_back(head);     // Thêm đầu mới vào cuối vector
}

// Di chuyển rắn xuống dưới (thêm đầu mới phía dưới)
void MoveDown() {
    POINT head = snake.back(); // Lấy đầu rắn hiện tại
    head.y++;                  // Tăng tọa độ y (xuống dưới)
    snake.push_back(head);     // Thêm đầu mới vào cuối vector
}

// Tính toán vị trí đầu rắn mới theo hướng di chuyển (không thực sự di chuyển)
POINT NextHead(int dir) {
    POINT head = snake.back(); // Lấy đầu rắn hiện tại
    if (dir == DIR_LEFT)  head.x--;  // Trái: giảm x
    if (dir == DIR_RIGHT) head.x++;  // Phải: tăng x
    if (dir == DIR_UP)    head.y--;  // Lên: giảm y
    if (dir == DIR_DOWN)  head.y++;  // Xuống: tăng y
    return head; // Trả về vị trí đầu mới (chỉ để kiểm tra)
}

// Alternative: Move using direction functions
// Di chuyển rắn theo hướng bằng cách gọi các hàm MoveXXX()
void MoveByDirection(int dir) {
    if (dir == DIR_LEFT) MoveLeft();
    else if (dir == DIR_RIGHT) MoveRight();
    else if (dir == DIR_UP) MoveUp();
    else if (dir == DIR_DOWN) MoveDown();
}

bool HitWall(const POINT& p) {
    MapData& currentMap = GetCurrentMap();
    if (p.x <= 0 || p.x >= currentMap.width || p.y <= 0 || p.y >= currentMap.height)
        return true;

    // Sử dụng GetTile an toàn
    char tile = GetTile(currentMap, p.x, p.y);
    return (tile == '#');
}

bool HitSelf(const POINT& p) {
    // Kiểm tra va chạm với thân rắn (không kiểm tra đầu hiện tại)
    if (snake.size() <= 1) return false; // Rắn quá ngắn thì không thể tự cắn

    // Kiểm tra tất cả các đoạn trừ đầu rắn (phần tử cuối cùng)
    for (size_t i = 0; i < snake.size() - 1; ++i)
        if (snake[i].x == p.x && snake[i].y == p.y) return true;
    return false;
}

bool Occupied(const POINT& p) {
    for (auto& s : snake)
        if (s.x == p.x && s.y == p.y) return true;

    MapData& currentMap = GetCurrentMap();
    // Sử dụng GetTile an toàn
    char tile = GetTile(currentMap, p.x, p.y);
    return (tile == '#');
}

// ===== DRAWING FUNCTIONS =====
void DrawChar(int x, int y, char c) {
    GotoXY(x, y);
    putchar(c);
}

void DrawSnake(char c) {
    for (auto& p : snake) DrawChar(p.x, p.y, c);
}

void DrawFood() {
    if (!foodVisible) return;
    if (foods.empty()) return;
    if (foodIndex < 0 || foodIndex >= (int)foods.size()) return;
    POINT f = foods[foodIndex];
    DrawChar(f.x, f.y, '@');
}

void DrawGate() {
    if (!gateActive) return;
    DrawChar(gatePos.x, gatePos.y, 'G');
}

// ===== GAME OBJECT MANAGEMENT =====
void GenerateFoods() {
    foods.clear();
    MapData& currentMap = GetCurrentMap();
    srand((unsigned)time(nullptr));
    while ((int)foods.size() < FOOD_COUNT) {
        POINT f{ (short)(rand() % (currentMap.width - 1) + 1),
                (short)(rand() % (currentMap.height - 1) + 1) };
        if (!Occupied(f)) foods.push_back(f);
    }
    foodIndex = 0;
    foodVisible = true;
}

POINT RandomGateOnBorder() {
    MapData& currentMap = GetCurrentMap();
    int edge = rand() % 4;
    POINT g{};
    if (edge == 0) g = { (short)(rand() % (currentMap.width - 1) + 1), 1 };
    if (edge == 1) g = { (short)(rand() % (currentMap.width - 1) + 1), (short)(currentMap.height - 1) };
    if (edge == 2) g = { 1, (short)(rand() % (currentMap.height - 1) + 1) };
    if (edge == 3) g = { (short)(currentMap.width - 1), (short)(rand() % (currentMap.height - 1) + 1) };
    return g;
}

void SpawnGate() {
    POINT g{};
    do { g = RandomGateOnBorder(); } while (Occupied(g));
    gatePos = g;
    gateActive = true;
    foodVisible = false;
    DrawGate();
}

// ===== ANIMATIONS =====
void BlinkSnake(int times = 4, int delayMs = 80) {
    for (int i = 0; i < times; i++) {
        DrawSnake(' '); Sleep(delayMs);
        DrawSnake('O'); Sleep(delayMs);
    }
}

void GateWave(int times = 3, int delayMs = 70) {
    for (int i = 0; i < times; i++) {
        DrawChar(gatePos.x, gatePos.y, '#'); Sleep(delayMs);
        DrawChar(gatePos.x, gatePos.y, 'G'); Sleep(delayMs);
    }
}

// ===== GAME LOGIC =====
void Eat() {
    PlayGameSound("eat");
    UpdateScore(speedLevel * 10);

    if (foodIndex == FOOD_COUNT - 1) {
        SpawnGate();
    }
    else {
        foodIndex++;
    }
}

void LevelUp() {
    GateWave();
    PlayGameSound("levelup");
    UpdateScore(speedLevel * 50);
    gateActive = false;
    gatePos = { -1,-1 };

    if (speedLevel == MAX_SPEED) speedLevel = 1;
    else speedLevel++;

    MapData& newMap = GetCurrentMap();

    // Chỉ cập nhật kích thước nếu thực sự khác
    if (WIDTH_CONSOLE != newMap.width || HEIGH_CONSOLE != newMap.height) {
        WIDTH_CONSOLE = newMap.width;
        HEIGH_CONSOLE = newMap.height;
        system("cls");
        DrawBoard(0, 0, WIDTH_CONSOLE, HEIGH_CONSOLE);
        DrawMapObstacles();
    }
    else {
        // Chỉ xóa và vẽ lại obstacles
        system("cls");
        DrawBoard(0, 0, WIDTH_CONSOLE, HEIGH_CONSOLE);
        DrawMapObstacles();
    }

    DrawColoredText(WIDTH_CONSOLE / 2 - 8, HEIGH_CONSOLE / 2, "LEVEL " + to_string(speedLevel), 14);
    DrawColoredText(WIDTH_CONSOLE / 2 - 10, HEIGH_CONSOLE / 2 + 1, "Theme: " + newMap.themeName, 11);
    Sleep(1500);

    // Chỉ reset độ dài nếu setting bật, KHÔNG reset vị trí
    if (!keepLengthWhenLevelUp) {
        while ((int)snake.size() > 6) snake.erase(snake.begin());
    }

    // Kiểm tra vị trí rắn có hợp lệ với map mới không
    MapData& currentMap = GetCurrentMap();
    bool needReposition = false;

    for (auto& segment : snake) {
        if (segment.x <= 0 || segment.x >= currentMap.width ||
            segment.y <= 0 || segment.y >= currentMap.height ||
            (segment.y < (int)currentMap.tiles.size() &&
                segment.x < (int)currentMap.tiles[segment.y].size() &&
                currentMap.tiles[segment.y][segment.x] == '#')) {
            needReposition = true;
            break;
        }
    }

    // LUÔN đặt rắn ở vị trí an toàn khi qua màn mới
    int oldLen = (int)snake.size();
    snake.clear();

    // Spawn rắn ở vị trí an toàn khi lên level
    int len;
    if (keepLengthWhenLevelUp) {
        len = max(3, oldLen); // Giữ nguyên độ dài thực, tối thiểu 3 đoạn
    }
    else {
        len = 6; // Reset về 6 đoạn nếu không giữ độ dài
    }

    // Tính vị trí spawn an toàn dựa trên độ dài rắn
    int safeX = len + 2;  // Đảm bảo có đủ chỗ cho rắn dài
    int safeY = 5;        // Vị trí an toàn cố định

    // Đảm bảo vị trí trong phạm vi map
    if (safeX + len >= currentMap.width) {
        safeX = currentMap.width - len - 2;
        if (safeX < 1) safeX = 1; // Tối thiểu ở vị trí 1
    }
    if (safeY >= currentMap.height) {
        safeY = currentMap.height - 2;
        if (safeY < 1) safeY = 1; // Tối thiểu ở vị trí 1
    }

    // Tạo rắn tại vị trí an toàn
    for (int i = 0; i < len; i++) {
        POINT newSegment;
        newSegment.x = (short)(safeX + i);  // Rắn nằm ngang từ trái sang phải
        newSegment.y = (short)safeY;
        snake.push_back(newSegment);
    }

    // Reset hướng di chuyển về phải để tránh đụng thân ngay lập tức
    moving = DIR_RIGHT;
    locked = DIR_LEFT;

    GenerateFoods();
}

void ProcessDead() {
    state = 0;
    PlayGameSound("death");
    SaveHighScore();

    BlinkSnake();

    // Nhập tên để lưu vào bảng xếp hạng
    if (currentScore > 0) {
        PrintBottom("Enter your name for high score table: ");
        string playerName;

        // Hiện cursor để nhập tên
        HideCursor(false);
        getline(cin, playerName);
        HideCursor(true);

        // Giới hạn độ dài tên
        if (playerName.length() > 14) {
            playerName = playerName.substr(0, 14);
        }
        if (playerName.empty()) {
            playerName = "Anonymous";
        }


        PrintBottom("Score saved! Final Score: " + to_string(currentScore) +
            (currentScore == highScore ? " NEW HIGH SCORE!" : "") +
            " Press Y to restart or any key to return menu.");
    }
    else {
        PrintBottom("Dead! Final Score: " + to_string(currentScore) +
            " Press Y to restart or any key to return menu.");
    }
}

void Step(int dir) {
    POINT nh = NextHead(dir);

    if (HitWall(nh) || HitSelf(nh)) {
        ProcessDead();
        return;
    }

    bool eat = false;
    if (foodIndex >= 0 && foodIndex < (int)foods.size() && foodVisible) {
        eat = (nh.x == foods[foodIndex].x && nh.y == foods[foodIndex].y);
    }
    bool hitGate = (gateActive && nh.x == gatePos.x && nh.y == gatePos.y);

    // Thêm đầu mới (đã được tính toán ở NextHead)
    snake.push_back(nh);

    if (eat) Eat();
    else snake.erase(snake.begin()); // Xóa đuôi nếu không ăn mồi

    // Cập nhật hướng bị khóa chỉ khi rắn đủ dài
    if (snake.size() > 2) {
        if (dir == DIR_LEFT)  locked = DIR_RIGHT;
        if (dir == DIR_RIGHT) locked = DIR_LEFT;
        if (dir == DIR_UP)    locked = DIR_DOWN;
        if (dir == DIR_DOWN)  locked = DIR_UP;
    }
    moving = dir;

    if (hitGate) LevelUp();
}

void ResetData() {
    moving = DIR_RIGHT;
    locked = DIR_LEFT;
    speedLevel = 1;
    foodIndex = 0;
    gateActive = false;
    gatePos = { -1,-1 };
    currentScore = 0;
    directionChanged = false;  // Reset input flag

    InitializeLevelMaps();
    MapData& currentMap = GetCurrentMap();
    WIDTH_CONSOLE = currentMap.width;
    HEIGH_CONSOLE = currentMap.height;

    snake.clear();

    // Spawn rắn ở vị trí an toàn đơn giản
    int initLen = 6;
    int safeX = 10;  // Vị trí an toàn cố định
    int safeY = 5;   // Vị trí an toàn cố định

    // Đảm bảo vị trí trong phạm vi map
    if (safeX + initLen >= currentMap.width) {
        safeX = currentMap.width - initLen - 2;
    }
    if (safeY >= currentMap.height) {
        safeY = currentMap.height - 2;
    }

    // Tạo rắn tại vị trí an toàn
    for (int i = 0; i < initLen; i++) {
        POINT newSegment;
        newSegment.x = (short)(safeX + i);  // Rắn nằm ngang từ trái sang phải
        newSegment.y = (short)safeY;
        snake.push_back(newSegment);
    }

    GenerateFoods();
}

// ===== SAVE/LOAD SYSTEM =====
bool SaveToFile(const string& filename) {
    ofstream fo(filename);
    if (!fo) return false;
    fo << WIDTH_CONSOLE << ' ' << HEIGH_CONSOLE << '\n';
    fo << (int)moving << ' ' << (int)locked << ' ' << speedLevel << ' ' << state << '\n';
    fo << keepLengthWhenLevelUp << ' ' << foodIndex << '\n';
    fo << gateActive << ' ' << gatePos.x << ' ' << gatePos.y << '\n';

    fo << snake.size() << '\n';
    for (auto& p : snake) fo << p.x << ' ' << p.y << '\n';

    fo << foods.size() << '\n';
    for (auto& f : foods) fo << f.x << ' ' << f.y << '\n';
    return true;
}

bool LoadFromFile(const string& filename) {
    ifstream fi(filename);
    if (!fi) return false;

    int mv, lk;
    fi >> WIDTH_CONSOLE >> HEIGH_CONSOLE;
    fi >> mv >> lk >> speedLevel >> state;
    moving = mv;
    locked = lk;
    fi >> keepLengthWhenLevelUp >> foodIndex;
    fi >> gateActive >> gatePos.x >> gatePos.y;

    size_t n; fi >> n; snake.clear(); snake.reserve(n);
    for (size_t i = 0; i < n; i++) {
        POINT p; fi >> p.x >> p.y;
        snake.push_back(p);
    }

    size_t m; fi >> m; foods.clear(); foods.reserve(m);
    for (size_t i = 0; i < m; i++) {
        POINT f; fi >> f.x >> f.y;
        foods.push_back(f);
    }
    return true;
}

// ===== GAME LOOP =====
void GameLoop() {
    using clock = std::chrono::steady_clock;
    const double baseMove = 220.0;
    auto last = clock::now();
    double accMs = 0.0;

    while (state == 1) {
        auto now = clock::now();
        double dt = std::chrono::duration<double, std::milli>(now - last).count();
        last = now;
        accMs += dt;

        int lvl = std::min(speedLevel, MAX_SPEED);
        double accel = 1.0 + 0.4 * (lvl - 1);
        double moveInterval = baseMove / accel;

        PrintBottom("Level: " + to_string(speedLevel) + "  Length: " + to_string(snake.size()) +
            "  Score: " + to_string(currentScore) + "  High: " + to_string(highScore) +
            (gateActive ? "   Gate: ON" : "   Gate: OFF"), false);

        if (_kbhit()) {
            int key = _getch();

            // Xử lý phím đặc biệt
            if (key == 224) { // Extended key prefix
                key = _getch(); // Get the actual arrow key code
            }

            key = std::toupper(key);

            if (key == 27) { state = 0; break; }
            else if (key == 'P') {
                PrintBottom("Paused. Press any key to resume...");
                _getch(); PrintBottom("");
            }
            else if (key == 'L') {
                PrintBottom("Save as (filename.txt): ");
                string fn; cin >> fn;
                if (SaveToFile(fn)) PrintBottom("Saved to " + fn);
                else PrintBottom("Save failed!");
            }
            else if (key == 'T') {
                PrintBottom("Load file: ");
                string fn; cin >> fn;
                if (LoadFromFile(fn)) {
                    system("cls");
                    DrawBoard(0, 0, WIDTH_CONSOLE, HEIGH_CONSOLE);
                    DrawMapObstacles();
                    PrintBottom("Loaded " + fn);
                }
                else PrintBottom("Load failed!");
            }
            else {
                // Xử lý phím điều hướng - chỉ cho phép 1 lần đổi hướng mỗi frame
                int newDir = GetDirectionFromKey(key);
                if (newDir != -1 && !directionChanged &&
                    CanChangeDirection(newDir, moving, snake.size()) &&
                    newDir != moving) {  // Chỉ đổi khi thực sự khác hướng hiện tại
                    moving = newDir;
                    directionChanged = true;  // Đánh dấu đã đổi hướng
                }
                // Nếu cố đi ngược lại hoặc đã đổi hướng rồi thì bỏ qua
            }
        }

        if (accMs >= moveInterval) {
            // Reset direction change flag mỗi frame
            directionChanged = false;

            // Kiểm tra rắn có hợp lệ không
            if (snake.empty()) {
                state = 0;
                break;
            }

            DrawFood();
            DrawSnake(' ');
            if (gateActive) DrawGate();
            Step(moving);
            if (state != 1) break;

            // Kiểm tra lại sau khi Step
            if (!snake.empty()) {
                DrawFood();
                DrawSnake('O');
                if (gateActive) DrawGate();
            }
            accMs = 0.0;
        }

        Sleep(1);
    }
}

// ===== MENU SYSTEM =====
int Menu() {
    system("cls");
    DrawBoard(0, 0, WIDTH_CONSOLE, HEIGH_CONSOLE);
    HideCursor();
    PrintBottom("");
    GotoXY(3, 3);  cout << "HUNTING SNAKE";
    GotoXY(3, 5);  cout << "1) New Game";
    GotoXY(3, 6);  cout << "2) Load Game";
    GotoXY(3, 7);  cout << "3) High Scores";
    GotoXY(3, 8);  cout << "4) Settings";
    GotoXY(3, 9);  cout << "5) Quit";
    GotoXY(3, 11); cout << "Choose (1-5): ";
    int ch = _getch();
    return ch;
}

void Settings() {
    while (true) {
        system("cls");
        DrawBoard(0, 0, WIDTH_CONSOLE, HEIGH_CONSOLE);
        DrawColoredText(3, 3, "SETTINGS", 14);
        GotoXY(3, 5); cout << "A) Keep length on level up: " << (keepLengthWhenLevelUp ? "ON" : "OFF");
        GotoXY(3, 6); cout << "B) Board Size (current " << WIDTH_CONSOLE << "x" << HEIGH_CONSOLE << ")";
        GotoXY(3, 7); cout << "C) Game Mode: " << (currentMode == MODE_CLASSIC ? "Classic" :
            currentMode == MODE_SURVIVAL ? "Survival" : "Time Attack");
        GotoXY(3, 8); cout << "ESC) Back";

        int k = std::toupper(_getch());
        if (k == 27) return;
        if (k == 'A') { keepLengthWhenLevelUp = !keepLengthWhenLevelUp; }
        else if (k == 'B') {
            PrintBottom("Enter WIDTH HEIGHT: ");
            cin >> WIDTH_CONSOLE >> HEIGH_CONSOLE;
            PrintBottom("Applied.");
        }
        else if (k == 'C') {
            int mode = currentMode;
            mode = (mode + 1) % 3;
            currentMode = mode;
        }
    }
}


using namespace std;
using namespace sf;

void spawnApple(Sprite& appleSprite, const vector<RectangleShape>& snake, float frameWidth, float frameHeight, float posX_frame, float posY_frame, float blockSize) {
    int gridWidth = static_cast<int>(frameWidth / blockSize);
    int gridHeight = static_cast<int>(frameHeight / blockSize);
    Vector2f applePos;
    bool onSnake;
    do {
        onSnake = false;
        float randX_grid = static_cast<float>(rand() % gridWidth);
        float randY_grid = static_cast<float>(rand() % gridHeight);
        applePos.x = posX_frame + randX_grid * blockSize;
        applePos.y = posY_frame + randY_grid * blockSize;
        for (const auto& segment : snake) {
            if (segment.getPosition() == applePos) {
                onSnake = true;
                break;
            }
        }
    } while (onSnake);
    appleSprite.setPosition(applePos);
}

void resetGame(vector<RectangleShape>& snake, Vector2f& direction, Vector2f& lastDirection, Sprite& appleSprite, float frameWidth, float frameHeight, float posX_frame, float posY_frame, float blockSize) {
    snake.clear();
    direction = Vector2f(blockSize, 0.f);
    lastDirection = direction;
    for (int i = 0; i < 3; ++i) {
        RectangleShape segment;
        segment.setSize(Vector2f(blockSize, blockSize));
        segment.setFillColor(Color(0, 150, 0));
        segment.setOutlineColor(Color::Black);
        segment.setOutlineThickness(1.f);
        segment.setPosition(posX_frame + (2 - i) * blockSize, posY_frame + 5 * blockSize);
        snake.push_back(segment);
    }
    snake[0].setFillColor(getSnakeBodyColorMain());
    spawnApple(appleSprite, snake, frameWidth, frameHeight, posX_frame, posY_frame, blockSize);
}

// Hiển thị hộp thoại nhập text
string showInputDialog(RenderWindow& window, const string& prompt, bool isResume = false) {
    Font font;
    if (!font.loadFromFile("fonts/arial.ttf")) return "";

    string inputText = "";
    RectangleShape dialogBox(Vector2f(600, 300));
    dialogBox.setFillColor(Color(50, 50, 50, 230));
    dialogBox.setOutlineColor(Color::White);
    dialogBox.setOutlineThickness(3);
    dialogBox.setPosition(
        (window.getSize().x - dialogBox.getSize().x) / 2,
        (window.getSize().y - dialogBox.getSize().y) / 2
    );

    Text promptText(prompt, font, 24);
    promptText.setFillColor(Color::White);
    promptText.setPosition(
        dialogBox.getPosition().x + 50,
        dialogBox.getPosition().y + 50
    );

    Text inputDisplay("", font, 28);
    inputDisplay.setFillColor(Color::Yellow);
    inputDisplay.setPosition(
        dialogBox.getPosition().x + 50,
        dialogBox.getPosition().y + 120
    );

    Text instruction("Press Enter to confirm, Esc to cancel", font, 18);
    instruction.setFillColor(Color(200, 200, 200));
    instruction.setPosition(
        dialogBox.getPosition().x + 50,
        dialogBox.getPosition().y + 240
    );

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) {
                window.close();
                return "";
            }

            if (event.type == Event::KeyPressed) {
                if (event.key.code == Keyboard::Escape) {
                    return "";
                }
                if (event.key.code == Keyboard::Enter && !inputText.empty()) {
                    return inputText;
                }
                if (event.key.code == Keyboard::BackSpace && !inputText.empty()) {
                    inputText.pop_back();
                }
            }

            if (event.type == Event::TextEntered) {
                if (event.text.unicode < 128 && event.text.unicode != 13 && event.text.unicode != 8) {
                    char c = static_cast<char>(event.text.unicode);
                    if (isalnum(c) || c == '_' || c == '-') {
                        if (inputText.length() < 20) {
                            inputText += c;
                        }
                    }
                }
            }
        }

        inputDisplay.setString(inputText + "_");

        window.clear(Color::Black);
        window.draw(dialogBox);
        window.draw(promptText);
        window.draw(inputDisplay);
        window.draw(instruction);
        window.display();
    }

    return "";
}

// Hiển thị thông báo
void showMessage(RenderWindow& window, const string& message) {
    Font font;
    if (!font.loadFromFile("fonts/arial.ttf")) return;

    RectangleShape msgBox(Vector2f(500, 200));
    msgBox.setFillColor(Color(50, 50, 50, 230));
    msgBox.setOutlineColor(Color::Red);
    msgBox.setOutlineThickness(3);
    msgBox.setPosition(
        (window.getSize().x - msgBox.getSize().x) / 2,
        (window.getSize().y - msgBox.getSize().y) / 2
    );

    Text msgText(message, font, 22);
    msgText.setFillColor(Color::White);
    FloatRect textBounds = msgText.getLocalBounds();
    msgText.setPosition(
        msgBox.getPosition().x + (msgBox.getSize().x - textBounds.width) / 2,
        msgBox.getPosition().y + 60
    );

    Text closeText("Press any key to close", font, 18);
    closeText.setFillColor(Color(200, 200, 200));
    FloatRect closeBounds = closeText.getLocalBounds();
    closeText.setPosition(
        msgBox.getPosition().x + (msgBox.getSize().x - closeBounds.width) / 2,
        msgBox.getPosition().y + 140
    );

    window.clear(Color::Black);
    window.draw(msgBox);
    window.draw(msgText);
    window.draw(closeText);
    window.display();

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) {
                window.close();
                return;
            }
            if (event.type == Event::KeyPressed || event.type == Event::MouseButtonPressed) {
                return;
            }
        }
    }
}

// Lưu trạng thái game vào file với tên người chơi
void saveGameState(const string& playerName, const vector<RectangleShape>& snake, const Vector2f& direction, const Vector2f& applePos, float blockSize, int score = 0) {
    const string SAVE_FILE = "savegames.txt";
    
    // Đọc toàn bộ file hiện có
    map<string, vector<string>> allSaves;
    ifstream readFile(SAVE_FILE);
    if (readFile.is_open()) {
        string line;
        string currentPlayer;
        vector<string> currentData;
        
        while (getline(readFile, line)) {
            if (line.find("PLAYER:") == 0) {
                if (!currentPlayer.empty()) {
                    allSaves[currentPlayer] = currentData;
                }
                currentPlayer = line.substr(7); // Bỏ "PLAYER:"
                currentData.clear();
            } else {
                currentData.push_back(line);
            }
        }
        if (!currentPlayer.empty()) {
            allSaves[currentPlayer] = currentData;
        }
        readFile.close();
    }
    
    // Cập nhật hoặc thêm mới data cho player
    vector<string> newData;
    newData.push_back(to_string(blockSize));
    newData.push_back(to_string(direction.x) + " " + to_string(direction.y));
    newData.push_back(to_string(applePos.x) + " " + to_string(applePos.y));
    newData.push_back(to_string(score));
    newData.push_back(to_string(snake.size()));
    for (const auto& segment : snake) {
        newData.push_back(to_string(segment.getPosition().x) + " " + to_string(segment.getPosition().y));
    }
    allSaves[playerName] = newData;
    
    // Ghi lại toàn bộ file
    ofstream writeFile(SAVE_FILE);
    if (writeFile.is_open()) {
        for (const auto& pair : allSaves) {
            writeFile << "PLAYER:" << pair.first << "\n";
            for (const auto& line : pair.second) {
                writeFile << line << "\n";
            }
        }
        writeFile.close();
    }
    
    // Cập nhật rank nếu không phải save tạm
    if (playerName != "__TEMP_SAVE__" && score > 0) {
        // Dùng độ dài rắn làm level (phản ánh tiến trình thực tế)
        int level = static_cast<int>(snake.size());
        SaveHighScoreEntry(playerName, score, level);
        cout << "Rank updated for " << playerName << ": " << score << " pts (Snake Length: " << level << ")\n";
    }
}

// Xóa save game của player
void deleteSaveState(const string& playerName) {
    const string SAVE_FILE = "savegames.txt";
    
    // Đọc toàn bộ file hiện có
    map<string, vector<string>> allSaves;
    ifstream readFile(SAVE_FILE);
    if (readFile.is_open()) {
        string line;
        string currentPlayer;
        vector<string> currentData;
        
        while (getline(readFile, line)) {
            if (line.find("PLAYER:") == 0) {
                if (!currentPlayer.empty()) {
                    allSaves[currentPlayer] = currentData;
                }
                currentPlayer = line.substr(7); // Bỏ "PLAYER:"
                currentData.clear();
            } else {
                currentData.push_back(line);
            }
        }
        if (!currentPlayer.empty()) {
            allSaves[currentPlayer] = currentData;
        }
        readFile.close();
    }
    
    // Xóa player khỏi map
    allSaves.erase(playerName);
    
    // Ghi lại file (không có player đã xóa)
    ofstream writeFile(SAVE_FILE);
    if (writeFile.is_open()) {
        for (const auto& pair : allSaves) {
            writeFile << "PLAYER:" << pair.first << "\n";
            for (const auto& line : pair.second) {
                writeFile << line << "\n";
            }
        }
        writeFile.close();
    }
}

// Load trạng thái game từ file với tên người chơi
bool loadGameState(const string& playerName, vector<RectangleShape>& snake, Vector2f& direction, Vector2f& lastDirection, Sprite& appleSprite, float blockSize, int& score) {
    const string SAVE_FILE = "savegames.txt";
    ifstream file(SAVE_FILE);
    if (!file.is_open()) return false;

    string line;
    bool foundPlayer = false;
    
    while (getline(file, line)) {
        if (line == "PLAYER:" + playerName) {
            foundPlayer = true;
            break;
        }
    }
    
    if (!foundPlayer) {
        file.close();
        return false;
    }

    float savedBlockSize;
    Vector2f applePos;
    size_t snakeSize;

    // Đọc thông tin cơ bản
    file >> savedBlockSize;
    file >> direction.x >> direction.y;
    file >> applePos.x >> applePos.y;
    file >> score;
    file >> snakeSize;

    lastDirection = direction;
    appleSprite.setPosition(applePos);

    // Đọc vị trí từng đốt rắn
    snake.clear();
    for (size_t i = 0; i < snakeSize; i++) {
        float x, y;
        file >> x >> y;

        RectangleShape segment;
        segment.setSize(Vector2f(blockSize, blockSize));
        segment.setFillColor(i == 0 ? getSnakeBodyColorMain() : getSnakeDarkBodyColorMain());
        segment.setOutlineColor(Color::Black);
        segment.setOutlineThickness(1.f);
        segment.setPosition(x, y);
        snake.push_back(segment);
    }

    file.close();
    return snakeSize > 0;
}

void startGame(RenderWindow& window, GameResources& resources, bool resumeMode = false, const string& playerName = "") {
    const float blockSize = 25.f;
    const float frameWidth = 950.f;
    const float frameHeight = 550.f;
    const float posX_frame = 270.f;
    const float posY_frame = (static_cast<float>(window.getSize().y) - frameHeight) / 2.f;
    const int gridWidth = static_cast<int>(floor(frameWidth / blockSize));
    const int gridHeight = static_cast<int>(floor(frameHeight / blockSize));

    // Chuỗi ID cho con rắn
    const string snakeID = "2412749524127493241274982412724624127431";
    int idIndex = 3; // Bắt đầu với 3 ký tự (vì có 3 đốt ban đầu)

    vector<RectangleShape> snake;
    Sprite appleSprite, spriteContext, frameSprite;
    Vector2f direction(blockSize, 0.f), lastDirection = direction;
    int score = 0;
    
    // Load font cho số trên rắn
    Font font;
    bool fontLoaded = false;
    
    // Thử nhiều font đẹp (ưu tiên font game/số đẹp)
    if (font.loadFromFile("C:\\Windows\\Fonts\\impact.ttf")) {
        fontLoaded = true;
        cout << "Font loaded: impact.ttf\n";
    }
    else if (font.loadFromFile("C:\\Windows\\Fonts\\arialbd.ttf")) {
        fontLoaded = true;
        cout << "Font loaded: arialbd.ttf (Arial Bold)\n";
    }
    else if (font.loadFromFile("C:\\Windows\\Fonts\\comic.ttf")) {
        fontLoaded = true;
        cout << "Font loaded: comic.ttf\n";
    }
    else if (font.loadFromFile("C:\\Windows\\Fonts\\arial.ttf")) {
        fontLoaded = true;
        cout << "Font loaded: arial.ttf\n";
    }
    else if (font.loadFromFile("fonts/arial.ttf")) {
        fontLoaded = true;
        cout << "Font loaded: fonts/arial.ttf\n";
    }
    else {
        cout << "ERROR: Cannot load any font!\n";
    }

    // Dùng texture đã load sẵn
    spriteContext.setTexture(resources.contextTexture);
    spriteContext.setScale(
        static_cast<float>(window.getSize().x) / resources.contextTexture.getSize().x,
        static_cast<float>(window.getSize().y) / resources.contextTexture.getSize().y
    );

    frameSprite.setTexture(resources.frameTexture);
    frameSprite.setScale(frameWidth / resources.frameTexture.getSize().x, frameHeight / resources.frameTexture.getSize().y);
    frameSprite.setPosition(posX_frame, posY_frame);

    appleSprite.setTexture(resources.appleTexture);
    float scaleApple = blockSize / resources.appleTexture.getSize().x;
    appleSprite.setScale(scaleApple, scaleApple);

    // Score icon sprite (góc trên bên trái)
    Sprite scoreIconSprite(resources.scoreTexture);
    float scoreIconScale = 120.0f / resources.scoreTexture.getSize().y; // Height = 120px (gấp 1.5)
    scoreIconSprite.setScale(scoreIconScale, scoreIconScale);
    scoreIconSprite.setPosition(20.f, 10.f);
    
    // Pause button sprite (góc phải trên - có thể click)
    Sprite pauseButtonSprite(resources.pauseTexture);
    float pauseButtonScale = 130.0f / resources.pauseTexture.getSize().y; // Tăng từ 60 lên 80
    pauseButtonSprite.setScale(pauseButtonScale, pauseButtonScale);
    float pauseButtonWidth = resources.pauseTexture.getSize().x * pauseButtonScale;
    pauseButtonSprite.setPosition(window.getSize().x - pauseButtonWidth + 50.f, 1.f); // +10 để dịch sang phải
    
    // Pause icon lớn (hiển thị giữa màn hình khi pause)
    Sprite pauseIconSprite(resources.pauseTexture);
    float pauseIconScale = 80.0f / resources.pauseTexture.getSize().y;
    pauseIconSprite.setScale(pauseIconScale, pauseIconScale);
    pauseIconSprite.setPosition(
        (window.getSize().x - resources.pauseTexture.getSize().x * pauseIconScale) / 2.f,
        (window.getSize().y - resources.pauseTexture.getSize().y * pauseIconScale) / 2.f - 100.f
    );

    // Nếu là resume mode thì load game, không thì reset game mới
    bool actuallyResumed = false;
    if (resumeMode && !playerName.empty()) {
        bool loaded = loadGameState(playerName, snake, direction, lastDirection, appleSprite, blockSize, score);
        if (loaded) {
            actuallyResumed = true;
            cout << "Successfully loaded save for: " << playerName << "\n";
        } else {
            // Nếu không load được thì reset game mới
            cout << "Failed to load save, starting new game\n";
            resetGame(snake, direction, lastDirection, appleSprite, frameWidth, frameHeight, posX_frame, posY_frame, blockSize);
        }
    }
    else {
        resetGame(snake, direction, lastDirection, appleSprite, frameWidth, frameHeight, posX_frame, posY_frame, blockSize);
    }

    // Đăng ký Raw Input để bypass Unikey
    HWND hwnd = window.getSystemHandle();
    RegisterRawInput(hwnd);
    cout << "Raw Input registered for Unikey bypass\n";
    
    // Đảm bảo window có focus để nhận input
    window.requestFocus();
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    Clock clock;
    
    // Áp dụng độ khó từ Settings
    // setting_difficulty: 0 = EASY (chậm), 1 = NORMAL (bth), 2 = HARD (nhanh)
    int moveSpeed;
    if (setting_difficulty == 0) {
        moveSpeed = 150; // EASY: chậm
    } else if (setting_difficulty == 1) {
        moveSpeed = 100; // NORMAL: bình thường
    } else { // setting_difficulty == 2
        moveSpeed = 60;  // HARD: nhanh
    }
    
    Time timePerMove = milliseconds(moveSpeed), timeSinceLastMove = Time::Zero;
    Time autoSaveInterval = seconds(5), timeSinceAutoSave = Time::Zero;
    // Chỉ dùng tên thật nếu THỰC SỰ load thành công
    string tempSaveName = actuallyResumed ? playerName : "__TEMP_SAVE__";
    bool isPaused = actuallyResumed; // Chỉ pause nếu thực sự load được save

    cout << "=== ENTERING GAME LOOP ===\n";
    cout << "Snake initial size: " << snake.size() << endl;
    cout << "Font loaded: " << (fontLoaded ? "YES" : "NO") << endl;
    cout << "Auto-save name: " << tempSaveName << endl;
    if (isPaused) cout << "Game PAUSED - Press WASD to start\n";

    while (window.isOpen()) {
        Time dt = clock.restart();
        timeSinceLastMove += dt;
        timeSinceAutoSave += dt;
        
        // Auto-save mỗi 5 giây
        if (timeSinceAutoSave >= autoSaveInterval) {
            timeSinceAutoSave = Time::Zero;
            saveGameState(tempSaveName, snake, direction, appleSprite.getPosition(), blockSize, score);
            cout << "Auto-saved to: " << tempSaveName << endl;
        }
        
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) window.close();
            
            // Xử lý click pause button
            if (event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Left) {
                Vector2f mousePos(static_cast<float>(event.mouseButton.x), static_cast<float>(event.mouseButton.y));
                FloatRect pauseButtonBounds = pauseButtonSprite.getGlobalBounds();
                
                if (pauseButtonBounds.contains(mousePos)) {
                    bool wasUnpaused = isPaused && true; // Đang pause và sẽ unpause
                    isPaused = !isPaused;
                    cout << (isPaused ? "Game PAUSED" : "Game RESUMED") << endl;
                    
                    // Nếu vừa unpause, kiểm tra ngay phím để responsive
                    if (wasUnpaused && !isPaused) {
                        // Đợi một chút để người dùng thả chuột và bấm phím
                        sf::sleep(sf::milliseconds(10));
                    }
                }
            }
        }

        // Xử lý ESC bằng Raw Input (bypass Unikey)
        if (IsRawKeyPressed(VK_ESCAPE)) {
            rawKeyStates[VK_ESCAPE] = false; // Clear ngay sau khi xử lý
            // Chỉ xóa save tạm nếu là New Game
            if (!actuallyResumed && tempSaveName == "__TEMP_SAVE__") {
                deleteSaveState("__TEMP_SAVE__");
                cout << "Temp save deleted on ESC\n";
            }
            // Resume: giữ nguyên save, không xóa
            UnregisterRawInput(hwnd);
            return;
        }

        // Lambda để kiểm tra và cập nhật hướng (có thể gọi nhiều lần)
        auto checkAndUpdateDirection = [&]() {
            bool directionChanged = false;
            Vector2f newDirection = direction;
            
            // Ưu tiên phím được bấm sau cùng
            if (IsRawKeyPressed('W') && lastDirection.y == 0) {
                newDirection = { 0.f, -blockSize };
                directionChanged = true;
            }
            if (IsRawKeyPressed('S') && lastDirection.y == 0) {
                newDirection = { 0.f, blockSize };
                directionChanged = true;
            }
            if (IsRawKeyPressed('A') && lastDirection.x == 0) {
                newDirection = { -blockSize, 0.f };
                directionChanged = true;
            }
            if (IsRawKeyPressed('D') && lastDirection.x == 0) {
                newDirection = { blockSize, 0.f };
                directionChanged = true;
            }
            
            // Cập nhật direction ngay lập tức để responsive
            if (directionChanged) {
                direction = newDirection;
                
                // Nếu có thay đổi hướng và game đang pause thì bỏ pause
                if (isPaused) {
                    isPaused = false;
                    cout << "Game STARTED!\n";
                }
            }
            return directionChanged;
        };
        
        // Kiểm tra phím lần 1 (bình thường)
        checkAndUpdateDirection();

        if (timeSinceLastMove >= timePerMove && !isPaused) {
            timeSinceLastMove = Time::Zero;
            lastDirection = direction;
            Vector2f newHeadPos = snake[0].getPosition() + direction;
            bool gameOver = false;

            if (newHeadPos.x < posX_frame || newHeadPos.x >= (posX_frame + gridWidth * blockSize) ||
                newHeadPos.y < posY_frame || newHeadPos.y >= (posY_frame + gridHeight * blockSize))
                gameOver = true;

            for (size_t i = 1; i < snake.size(); ++i)
                if (newHeadPos == snake[i].getPosition()) gameOver = true;

            if (gameOver) {
                // Hủy Raw Input trước khi hiện dialog
                UnregisterRawInput(hwnd);
                
                // Nếu THỰC SỰ resume (load thành công) thì tự động lưu luôn
                if (actuallyResumed && !playerName.empty()) {
                    // Resume: tự động lưu với tên đã có
                    saveGameState(playerName, snake, direction, appleSprite.getPosition(), blockSize, score);
                    cout << "Game saved automatically for: " << playerName << endl;
                } else {
                    // New Game: hỏi nhập tên
                    string name = showInputDialog(window, "Game Over! Enter your name to save (ESC to skip):");
                    if (!name.empty()) {
                        // Xóa save tạm nếu có
                        if (tempSaveName == "__TEMP_SAVE__") {
                            deleteSaveState("__TEMP_SAVE__");
                        }
                        // Lưu với tên mới
                        saveGameState(name, snake, direction, appleSprite.getPosition(), blockSize, score);
                    } else {
                        // Skip nhập tên -> xóa save tạm
                        deleteSaveState(tempSaveName);
                    }
                }
                // Quay về menu
                return;
            }

            RectangleShape newHead;
            newHead.setSize({ blockSize, blockSize });
            newHead.setFillColor(getSnakeBodyColorMain());
            newHead.setOutlineColor(Color::Black);
            newHead.setOutlineThickness(1.f);
            newHead.setPosition(newHeadPos);
            snake.insert(snake.begin(), newHead);

            if (newHeadPos == appleSprite.getPosition()) {
                score += 10;
                spawnApple(appleSprite, snake, frameWidth, frameHeight, posX_frame, posY_frame, blockSize);
                // Tăng idIndex khi ăn táo, lặp lại nếu hết chuỗi
                idIndex = (idIndex + 1) % snakeID.length();
            }
            else
                snake.pop_back();
        }

        window.clear(Color::Black);
        window.draw(spriteContext);
        window.draw(frameSprite);
        window.draw(appleSprite);
        
        // Vẽ rắn với số ID trên mỗi đốt
        for (int i = (int)snake.size() - 1; i >= 0; --i) {
            window.draw(snake[i]);
            
            // Debug: in ra console
            static bool firstRun = true;
            if (firstRun && i == 0) {
                cout << "Snake size: " << snake.size() << endl;
                cout << "Font loaded: " << (fontLoaded ? "YES" : "NO") << endl;
                cout << "snakeID length: " << snakeID.length() << endl;
                firstRun = false;
            }
            
            // Vẽ số ID trên đốt rắn (chỉ khi font đã load)
            if (fontLoaded && i < (int)snakeID.length()) {
                Text idText;
                idText.setFont(font);
                idText.setString(string(1, snakeID[i]));
                idText.setCharacterSize(20); // Tăng size lên
                idText.setFillColor(Color::Yellow); // Đổi màu vàng dễ thấy
                idText.setStyle(Text::Bold);
                
                // Outline cho text dễ nhìn hơn
                idText.setOutlineThickness(2.f);
                idText.setOutlineColor(Color::Black);
                
                // Center text trong block
                FloatRect textBounds = idText.getLocalBounds();
                idText.setOrigin(textBounds.left + textBounds.width / 2.f, 
                                 textBounds.top + textBounds.height / 2.f);
                Vector2f blockCenter = snake[i].getPosition() + Vector2f(blockSize / 2.f, blockSize / 2.f);
                idText.setPosition(blockCenter);
                
                window.draw(idText);
            }
        }
        
        // Vẽ pause button ở góc phải trên (luôn hiển thị)
        window.draw(pauseButtonSprite);
        
        // Vẽ Score icon và score text
        window.draw(scoreIconSprite);
        if (fontLoaded) {
            Text scoreText;
            scoreText.setFont(font);
            scoreText.setString(to_string(score));
            scoreText.setCharacterSize(45);
            scoreText.setFillColor(Color::White);
            scoreText.setStyle(Text::Bold);
            scoreText.setOutlineThickness(1.5f);
            scoreText.setOutlineColor(Color::Black);
            // Đặt số vào giữa ảnh score (căn giữa cả ngang và dọc)
            FloatRect textBounds = scoreText.getLocalBounds();
            scoreText.setOrigin(textBounds.left + textBounds.width / 2.f, 
                               textBounds.top + textBounds.height / 2.f);
            scoreText.setPosition(170.f, 70.f); // Dịch sang phải thêm
            window.draw(scoreText);
        }
        
        // Hiển thị text khi game đang pause (không vẽ icon nữa vì đã có button)
        if (isPaused && fontLoaded) {
            Text pausedText;
            pausedText.setFont(font);
            pausedText.setString("Press WASD to start");
            pausedText.setCharacterSize(35);
            pausedText.setFillColor(Color::Yellow);
            pausedText.setStyle(Text::Bold);
            pausedText.setOutlineThickness(3.f);
            pausedText.setOutlineColor(Color::Black);
            
            FloatRect textBounds = pausedText.getLocalBounds();
            pausedText.setOrigin(textBounds.left + textBounds.width / 2.f, 
                                 textBounds.top + textBounds.height / 2.f);
            pausedText.setPosition(window.getSize().x / 2.f, window.getSize().y / 2.f);
            
            window.draw(pausedText);
        }
        
        window.display();
    }
}
void showMenu(RenderWindow& window) {
    // Preload game resources 1 lần
    static GameResources gameResources;
    if (!gameResources.loaded) {
        if (!gameResources.loadAll()) {
            cout << "Failed to load game resources!\n";
            return;
        }
    }
    
    Texture bg, newGameTex, resumeTex, tutorialTex, settingsTex, rankTex, quitTex;

    // Đảm bảo tất cả các tệp này tồn tại trong thư mục "images/"
    if (!bg.loadFromFile("images/Menu.png")) {
        cout << "Lỗi: Không tìm thấy images/NewGame.png\n";
        return;
    }

    if (!newGameTex.loadFromFile("images/NewGame.png")) {
        cout << "Lỗi: Không tìm thấy images/NewGame.png\n";
        return;
    }
    if (!resumeTex.loadFromFile("images/Resume.png")) {
        cout << "Lỗi: Không tìm thấy images/Resume.png\n";
        return;
    }
    if (!tutorialTex.loadFromFile("images/Tutorial.png")) {
        cout << "Lỗi: Không tìm thấy images/Tutorial.png\n";
        return;
    }
    if (!settingsTex.loadFromFile("images/Settings.png")) {
        cout << "Lỗi: Không tìm thấy images/Settings.png\n";
        return;
    }
    if (!rankTex.loadFromFile("images/Rank.png")) {
        cout << "Lỗi: Không tìm thấy images/Rank.png\n";
        return;
    }
    if (!quitTex.loadFromFile("images/Quit.png")) {
        cout << "Lỗi: Không tìm thấy images/Quit.png\n";
        return;
    }

    Sprite bgSprite(bg);
    bgSprite.setScale(
        static_cast<float>(window.getSize().x) / bg.getSize().x,
        static_cast<float>(window.getSize().y) / bg.getSize().y
    );

    Sprite newGame(newGameTex), resume(resumeTex), tutorial(tutorialTex), settings(settingsTex), rank(rankTex), quit(quitTex);

    vector<Sprite*> buttons = { &newGame, &resume, &tutorial, &settings, &rank, &quit };

    // Căn giữa màn hình
    float centerX = window.getSize().x / 2.f;
    float centerY = window.getSize().y / 2.f;

    // Hàm helper để crop vùng trong suốt và scale nút
    auto setupButton = [](Sprite& btn, const Texture& tex, float targetHeight) {
        Vector2u texSize = tex.getSize();

        // Phát hiện vùng có nội dung thực (bỏ phần trong suốt)
        Image img = tex.copyToImage();
        int minY = texSize.y, maxY = 0;
        int minX = texSize.x, maxX = 0;

        for (unsigned int y = 0; y < texSize.y; y++) {
            for (unsigned int x = 0; x < texSize.x; x++) {
                if (img.getPixel(x, y).a > 10) { // Pixel không trong suốt
                    if (y < minY) minY = y;
                    if (y > maxY) maxY = y;
                    if (x < minX) minX = x;
                    if (x > maxX) maxX = x;
                }
            }
        }

        // Thêm padding nhỏ
        int padding = 5;
        minX = max(0, minX - padding);
        minY = max(0, minY - padding);
        maxX = min((int)texSize.x - 1, maxX + padding);
        maxY = min((int)texSize.y - 1, maxY + padding);

        // Crop texture
        int cropWidth = maxX - minX + 1;
        int cropHeight = maxY - minY + 1;
        btn.setTextureRect(IntRect(minX, minY, cropWidth, cropHeight));

        // Scale để có chiều cao mong muốn
        float scale = targetHeight / cropHeight;
        btn.setScale(scale, scale);

        // Set origin ở giữa
        btn.setOrigin(cropWidth / 2.f, cropHeight / 2.f);
        };

    // Chiều cao đồng nhất cho tất cả các nút
    float buttonHeight = 80.f;

    // Setup từng nút với chiều cao đồng nhất
    setupButton(newGame, newGameTex, buttonHeight);
    setupButton(resume, resumeTex, buttonHeight);
    setupButton(tutorial, tutorialTex, buttonHeight);
    setupButton(settings, settingsTex, buttonHeight);
    setupButton(rank, rankTex, buttonHeight);
    setupButton(quit, quitTex, buttonHeight);

    // Khoảng cách đồng nhất giữa các nút
    float gap = 10.f;  // Khoảng cách giữa các nút

    // ===== TÙY CHỈNH VỊ TRÍ CÁC NÚT Ở ĐÂY =====
    float offsetX = 80.f;  // Dời sang phải (giá trị dương) hoặc trái (giá trị âm)
    float offsetY = 160.f;  // Dời xuống dưới (giá trị dương) hoặc lên trên (giá trị âm)
    // ===========================================

    // Tính tổng chiều cao của tất cả nút + khoảng cách
    float totalHeight = buttonHeight * 6 + gap * 5;

    // Vị trí bắt đầu để căn giữa theo chiều dọc
    float startY = centerY - (totalHeight / 2.f) + buttonHeight / 2.f + offsetY;
    float posX = centerX + offsetX;

    // Đặt vị trí từng nút với khoảng cách đều nhau
    newGame.setPosition(posX, startY);
    resume.setPosition(posX, startY + buttonHeight + gap);
    tutorial.setPosition(posX, startY + (buttonHeight + gap) * 2);
    settings.setPosition(posX, startY + (buttonHeight + gap) * 3);
    rank.setPosition(posX, startY + (buttonHeight + gap) * 4);
    quit.setPosition(posX, startY + (buttonHeight + gap) * 5);

    // Debug: In ra bounds của từng nút
    cout << "=== VUNG CLICK CUA CAC NUT ===\n";
    cout << "NEW GAME: " << newGame.getGlobalBounds().left << ", " << newGame.getGlobalBounds().top
        << " -> " << (newGame.getGlobalBounds().left + newGame.getGlobalBounds().width)
        << ", " << (newGame.getGlobalBounds().top + newGame.getGlobalBounds().height) << "\n";
    cout << "RESUME: " << resume.getGlobalBounds().left << ", " << resume.getGlobalBounds().top
        << " -> " << (resume.getGlobalBounds().left + resume.getGlobalBounds().width)
        << ", " << (resume.getGlobalBounds().top + resume.getGlobalBounds().height) << "\n";
    cout << "TUTORIAL: " << tutorial.getGlobalBounds().left << ", " << tutorial.getGlobalBounds().top
        << " -> " << (tutorial.getGlobalBounds().left + tutorial.getGlobalBounds().width)
        << ", " << (tutorial.getGlobalBounds().top + tutorial.getGlobalBounds().height) << "\n";
    cout << "SETTINGS: " << settings.getGlobalBounds().left << ", " << settings.getGlobalBounds().top
        << " -> " << (settings.getGlobalBounds().left + settings.getGlobalBounds().width)
        << ", " << (settings.getGlobalBounds().top + settings.getGlobalBounds().height) << "\n";
    cout << "RANK: " << rank.getGlobalBounds().left << ", " << rank.getGlobalBounds().top
        << " -> " << (rank.getGlobalBounds().left + rank.getGlobalBounds().width)
        << ", " << (rank.getGlobalBounds().top + rank.getGlobalBounds().height) << "\n";
    cout << "QUIT: " << quit.getGlobalBounds().left << ", " << quit.getGlobalBounds().top
        << " -> " << (quit.getGlobalBounds().left + quit.getGlobalBounds().width)
        << ", " << (quit.getGlobalBounds().top + quit.getGlobalBounds().height) << "\n";
    cout << "==============================\n";

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) { window.close(); return; }
            if (event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Left) {
                Vector2f mousePos = window.mapPixelToCoords(Mouse::getPosition(window));

                // Debug: In vị trí click
                cout << "Click tai vi tri: (" << mousePos.x << ", " << mousePos.y << ")\n";

                // Hàm helper kiểm tra click trong vùng thu nhỏ của nút (loại bỏ phần trong suốt)
                auto isInButtonCore = [](const Sprite& btn, const Vector2f& pos) {
                    FloatRect bounds = btn.getGlobalBounds();
                    // Thu nhỏ vùng click: bỏ 25% mỗi bên
                    float margin = 0.1f;
                    float left = bounds.left + bounds.width * margin;
                    float right = bounds.left + bounds.width * (1.f - margin);
                    float top = bounds.top + bounds.height * margin;
                    float bottom = bounds.top + bounds.height * (1.f - margin);
                    return pos.x >= left && pos.x <= right && pos.y >= top && pos.y <= bottom;
                    };

                // Kiểm tra từ dưới lên trên để tránh overlap
                if (isInButtonCore(quit, mousePos)) {
                    cout << "Da click vao nut QUIT\n";
                    window.close();
                    return;
                }
                else if (isInButtonCore(rank, mousePos)) {
                    cout << "Da click vao nut RANK\n";
                    ShowHighScores(window);                   
                }
                else if (isInButtonCore(settings, mousePos)) {
                    cout << "Da click vao nut SETTINGS\n";
                    showSettings(window);
                }
                else if (isInButtonCore(tutorial, mousePos)) {
                    cout << "Da click vao nut TUTORIAL\n";
                    ShowTutorial(window);
                }
                else if (isInButtonCore(resume, mousePos)) {
                    cout << "Da click vao nut RESUME\n";
                    
                    // Kiểm tra xem có save tạm không (từ New Game đang chơi dở)
                    const string SAVE_FILE = "savegames.txt";
                    ifstream checkFile(SAVE_FILE);
                    bool hasTempSave = false;
                    if (checkFile.is_open()) {
                        string line;
                        while (getline(checkFile, line)) {
                            if (line == "PLAYER:__TEMP_SAVE__") {
                                hasTempSave = true;
                                break;
                            }
                        }
                        checkFile.close();
                    }
                    
                    // Nếu có save tạm, hỏi user muốn tiếp tục không
                    if (hasTempSave) {
                        string choice = showInputDialog(window, "Continue last game? (Y/N) or enter name for saved game:");
                        if (choice == "Y" || choice == "y" || choice == "yes" || choice == "YES") {
                            // Tiếp tục game tạm
                            startGame(window, gameResources, true, "__TEMP_SAVE__");
                        }
                        else if (!choice.empty()) {
                            // Tìm save với tên khác
                            ifstream checkFile2(SAVE_FILE);
                            bool foundPlayer = false;
                            if (checkFile2.is_open()) {
                                string line;
                                while (getline(checkFile2, line)) {
                                    if (line == "PLAYER:" + choice) {
                                        foundPlayer = true;
                                        break;
                                    }
                                }
                                checkFile2.close();
                            }
                            
                            if (foundPlayer) {
                                startGame(window, gameResources, true, choice);
                            }
                            else {
                                showMessage(window, "Save game not found for: " + choice);
                            }
                        }
                    }
                    else {
                        // Không có save tạm, nhập tên bình thường
                        string name = showInputDialog(window, "Enter your name to resume:");
                        if (!name.empty()) {
                            ifstream checkFile2(SAVE_FILE);
                            bool foundPlayer = false;
                            if (checkFile2.is_open()) {
                                string line;
                                while (getline(checkFile2, line)) {
                                    if (line == "PLAYER:" + name) {
                                        foundPlayer = true;
                                        break;
                                    }
                                }
                                checkFile2.close();
                            }
                            
                            if (foundPlayer) {
                                startGame(window, gameResources, true, name);
                            }
                            else {
                                showMessage(window, "Save game not found for: " + name);
                            }
                        }
                    }
                }
                else if (isInButtonCore(newGame, mousePos)) {
                    cout << "Da click vao nut NEW GAME\n";
                    startGame(window, gameResources, false, "");
                }
                else {
                    cout << "Click ngoai vung nut\n";
                }
            }
        }

        window.clear();
        window.draw(bgSprite);
        for (auto btn : buttons) window.draw(*btn);
        window.display();
    }
}

int main() {
    // Khởi tạo threads
    thread soundThread(SoundWorker);
    thread autoSaveThread(AutoSaveWorker);
    
    RenderWindow window(VideoMode(1550, 1050), "Snake Game Menu");
    window.setFramerateLimit(60);
    srand(static_cast<unsigned>(time(0)));
    showMenu(window);
    
    // Dừng threads trước khi thoát
    shouldExit = true;
    if (soundThread.joinable()) soundThread.join();
    if (autoSaveThread.joinable()) autoSaveThread.join();
    
    return 0;
}
