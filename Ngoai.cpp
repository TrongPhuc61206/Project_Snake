// SnakeVector_Full.cpp — C++17 / Visual Studio (Windows console)
// Reorganized version with better structure

#define NOMINMAX
#include <windows.h>
#include <conio.h>
#include <thread>
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
#pragma comment(lib, "winmm.lib")

using namespace std;

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

struct HighScoreEntry {
    string playerName;
    int score;
    int level;
    string date;

    HighScoreEntry(string name = "", int sc = 0, int lv = 1, string dt = "")
        : playerName(name), score(sc), level(lv), date(dt) {
    }
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

// ===== FORWARD DECLARATIONS =====
void InitializeLevelMaps();
MapData& GetCurrentMap();
bool Occupied(const POINT& p);
void GenerateFoods();
void DrawMapObstacles();
void ResetData();
void GameLoop();
bool Opposite(int dir1, int dir2);
void SaveHighScoreEntry(const string& playerName, int score, int level);
void ShowHighScores();
vector<HighScoreEntry> LoadHighScores();

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
void SaveHighScoreEntry(const string& playerName, int score, int level) {
    ofstream file("highscores.txt", ios::app);
    if (file) {
        // Lấy thời gian hiện tại (sử dụng localtime_s an toàn)
        time_t now = time(0);
        struct tm timeinfo;
        char timeStr[20];

        if (localtime_s(&timeinfo, &now) == 0) {
            strftime(timeStr, sizeof(timeStr), "%d/%m/%Y", &timeinfo);
        }
        else {
            strcpy_s(timeStr, sizeof(timeStr), "Unknown");
        }

        file << playerName << "|" << score << "|" << level << "|" << timeStr << "\n";
    }
}

// Đọc tất cả high scores từ file
vector<HighScoreEntry> LoadHighScores() {
    vector<HighScoreEntry> scores;
    ifstream file("highscores.txt");
    string line;

    while (getline(file, line) && !line.empty()) {
        size_t pos1 = line.find('|');
        size_t pos2 = line.find('|', pos1 + 1);
        size_t pos3 = line.find('|', pos2 + 1);

        if (pos1 != string::npos && pos2 != string::npos && pos3 != string::npos) {
            string name = line.substr(0, pos1);
            int score = stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
            int level = stoi(line.substr(pos2 + 1, pos3 - pos2 - 1));
            string date = line.substr(pos3 + 1);

            scores.push_back(HighScoreEntry(name, score, level, date));
        }
    }

    // Sắp xếp theo điểm số giảm dần
    sort(scores.begin(), scores.end(), [](const HighScoreEntry& a, const HighScoreEntry& b) {
        return a.score > b.score;
        });

    return scores;
}

// Hiển thị bảng xếp hạng top 15
void ShowHighScores() {
    system("cls");
    DrawBoard(0, 0, WIDTH_CONSOLE, HEIGH_CONSOLE);

    DrawColoredText(WIDTH_CONSOLE / 2 - 8, 2, "=== TOP 15 HIGH SCORES ===", 14);

    vector<HighScoreEntry> scores = LoadHighScores();

    GotoXY(3, 4);  cout << "Rank  Player Name       Score    Level   Date";
    GotoXY(3, 5);  cout << "====  ==============  =======  ======  ==========";

    int displayCount = min(15, (int)scores.size());
    for (int i = 0; i < displayCount; i++) {
        GotoXY(3, 6 + i);
        printf("%2d    %-14s  %7d  %6d  %s",
            i + 1,
            scores[i].playerName.c_str(),
            scores[i].score,
            scores[i].level,
            scores[i].date.c_str());
    }

    if (scores.empty()) {
        GotoXY(WIDTH_CONSOLE / 2 - 10, 10);
        cout << "No high scores yet!";
    }

    PrintBottom("Press any key to return to menu...");
    _getch();
}

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

        SaveHighScoreEntry(playerName, currentScore, speedLevel);

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
    snake[0].setFillColor(Color::Green);
    spawnApple(appleSprite, snake, frameWidth, frameHeight, posX_frame, posY_frame, blockSize);
}

void startGame(RenderWindow& window) {
    const float blockSize = 25.f;
    const float frameWidth = 950.f;
    const float frameHeight = 550.f;
    const float posX_frame = 270.f;
    const float posY_frame = (static_cast<float>(window.getSize().y) - frameHeight) / 2.f;
    const int gridWidth = static_cast<int>(floor(frameWidth / blockSize));
    const int gridHeight = static_cast<int>(floor(frameHeight / blockSize));

    vector<RectangleShape> snake;
    Texture appleTexture, contextTexture, frameTexture;
    Sprite appleSprite, spriteContext, frameSprite;
    Vector2f direction(blockSize, 0.f), lastDirection = direction;

    if (!contextTexture.loadFromFile("images/Context.png")) return;
    spriteContext.setTexture(contextTexture);
    spriteContext.setScale(
        static_cast<float>(window.getSize().x) / contextTexture.getSize().x,
        static_cast<float>(window.getSize().y) / contextTexture.getSize().y
    );

    if (!frameTexture.loadFromFile("images/Frame.png")) return;
    frameSprite.setTexture(frameTexture);
    frameSprite.setScale(frameWidth / frameTexture.getSize().x, frameHeight / frameTexture.getSize().y);
    frameSprite.setPosition(posX_frame, posY_frame);

    if (!appleTexture.loadFromFile("images/Apple.png")) return;
    appleSprite.setTexture(appleTexture);
    float scaleApple = blockSize / appleTexture.getSize().x;
    appleSprite.setScale(scaleApple, scaleApple);

    resetGame(snake, direction, lastDirection, appleSprite, frameWidth, frameHeight, posX_frame, posY_frame, blockSize);

    Clock clock;
    Time timePerMove = milliseconds(150), timeSinceLastMove = Time::Zero;

    while (window.isOpen()) {
        Time dt = clock.restart();
        timeSinceLastMove += dt;
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) window.close();
            if (event.type == Event::KeyPressed && event.key.code == Keyboard::Escape) return;
            if (event.type == Event::KeyPressed) {
                if (event.key.code == Keyboard::W && lastDirection.y == 0) direction = { 0.f, -blockSize };
                else if (event.key.code == Keyboard::S && lastDirection.y == 0) direction = { 0.f, blockSize };
                else if (event.key.code == Keyboard::A && lastDirection.x == 0) direction = { -blockSize, 0.f };
                else if (event.key.code == Keyboard::D && lastDirection.x == 0) direction = { blockSize, 0.f };
            }
        }

        if (timeSinceLastMove >= timePerMove) {
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
                resetGame(snake, direction, lastDirection, appleSprite, frameWidth, frameHeight, posX_frame, posY_frame, blockSize);
                continue;
            }

            RectangleShape newHead;
            newHead.setSize({ blockSize, blockSize });
            newHead.setFillColor(Color::Green);
            newHead.setOutlineColor(Color::Black);
            newHead.setOutlineThickness(1.f);
            newHead.setPosition(newHeadPos);
            snake.insert(snake.begin(), newHead);

            if (newHeadPos == appleSprite.getPosition())
                spawnApple(appleSprite, snake, frameWidth, frameHeight, posX_frame, posY_frame, blockSize);
            else
                snake.pop_back();
        }

        window.clear(Color::Black);
        window.draw(spriteContext);
        window.draw(frameSprite);
        window.draw(appleSprite);
        for (int i = (int)snake.size() - 1; i >= 0; --i) window.draw(snake[i]);
        window.display();
    }
}
void showMenu(RenderWindow& window) {
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

    // Dời sang phải 20 pixel (tâm 775 + 20 = 795)
    float centerX = (window.getSize().x / 2.f) + 20.f;

    // Điều chỉnh vị trí Y bắt đầu một chút để phù hợp với ảnh mẫu
    float y = 390.f; // <<< ĐÃ ĐIỀU CHỈNH Y BẮT ĐẦU

    float spacing = 95.f; // <<< ĐÃ ĐIỀU CHỈNH KHOẢNG CÁCH GIỮA CÁC NÚT (thử với 95, bạn có thể điều chỉnh thêm)

    // Vòng lặp để căn chỉnh từng nút
    for (auto btn : buttons) {

        float scaleToUse;

        // <<< SỬA Ở ĐÂY: Logic scale mới để phù hợp với ảnh mẫu >>>
        if (btn == &newGame || btn == &quit) {
            // Nút New Game và Quit có vẻ to hơn một chút trong ảnh mẫu
            scaleToUse = 1.0f; // Sử dụng kích thước gốc hoặc gần gốc
        }
        else if (btn == &resume || btn == &tutorial || btn == &settings || btn == &rank) {
            // Các nút còn lại nhỏ hơn và đồng đều với nhau
            scaleToUse = 0.7f; // Giảm xuống 70% kích thước gốc
        }
        else {
            // Trường hợp dự phòng nếu có nút nào không được liệt kê
            scaleToUse = 0.9f;
        }

        btn->setScale(scaleToUse, scaleToUse);

        // Logic căn giữa (vẫn giữ nguyên)
        FloatRect bounds = btn->getLocalBounds();
        btn->setOrigin(bounds.width / 2.f, bounds.height / 2.f);
        btn->setPosition(centerX, y);

        // Đảm bảo dòng này vẫn còn để các nút không bị chồng lên nhau
        y += spacing;
    }

    while (window.isOpen()) {
        Event event;
        while (window.pollEvent(event)) {
            if (event.type == Event::Closed) { window.close(); return; }
            if (event.type == Event::MouseButtonPressed && event.mouseButton.button == Mouse::Left) {
                Vector2f mousePos = window.mapPixelToCoords(Mouse::getPosition(window));
                if (newGame.getGlobalBounds().contains(mousePos)) startGame(window);
                else if (quit.getGlobalBounds().contains(mousePos)) { window.close(); return; }
            }
        }

        window.clear();
        window.draw(bgSprite);
        for (auto btn : buttons) window.draw(*btn);
        window.display();
    }
}

int main() {
    RenderWindow window(VideoMode(1550, 1050), "Snake Game Menu");
    window.setFramerateLimit(60);
    srand(static_cast<unsigned>(time(0)));
    showMenu(window);
    return 0;
}
