#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

struct HighScoreEntry {
    string playerName;
    int score;
    int level;
    string date;

    HighScoreEntry(string name = "", int sc = 0, int lv = 1, string dt = "")
        : playerName(name), score(sc), level(lv), date(dt) {
    }
};
// Đọc danh sách top điểm từ file (đã sắp xếp giảm dần)
std::vector<HighScoreEntry> LoadHighScores();

// Ghi thêm 1 dòng điểm mới vào file
void SaveHighScoreEntry(const std::string& playerName, int score, int level);

// Hiển thị bảng top 10 điểm bằng SFML
void ShowHighScores();

