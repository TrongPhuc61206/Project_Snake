#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>
#include <iostream>
// #include <optional> // Bỏ thư viện này nếu không bật C++17, code này không bắt buộc dùng nó.

struct HighScoreEntry {
    std::string playerName;
    int score;
    int level;
    std::string date;

    HighScoreEntry(std::string name = "", int sc = 0, int lv = 1, std::string dt = "")
        : playerName(std::move(name)), score(sc), level(lv), date(std::move(dt)) {
    }
};

static const std::string HIGHSCORE_FILE = "highscores.txt";
static const std::string FONT_PATH = "fonts/Arial.ttf";
static const std::string BG_IMAGE_PATH = "images/BackGround.png";

// 4 ảnh mới
static const std::string BACK_BTN_PATH = "images/back.png";
static const std::string GOLD_CUP_PATH = "images/gold_cup.png";
static const std::string SILVER_CUP_PATH = "images/silver_cup.png";
static const std::string BRONZE_CUP_PATH = "images/bronze_cup.png";

static const std::string RANKING_TITLE_PATH = "images/Title.png";
static const std::string LIST_PANEL_PATH = "images/rank_list.png";

// ============================================================================
// 1. Đọc danh sách điểm từ file
// ============================================================================
std::vector<HighScoreEntry> LoadHighScores() {
    std::vector<HighScoreEntry> scores;
    std::ifstream file(HIGHSCORE_FILE);
    std::string line;

    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string name, scoreStr, levelStr, date;

        if (std::getline(ss, name, '|') &&
            std::getline(ss, scoreStr, '|') &&
            std::getline(ss, levelStr, '|') &&
            std::getline(ss, date)) {
            try {
                // Sửa lỗi: dùng push_back thông thường để tránh lỗi C++ cũ nếu chưa bật C++17
                HighScoreEntry entry(name, std::stoi(scoreStr), std::stoi(levelStr), date);
                scores.push_back(entry);
            }
            catch (...) {}
        }
    }

    std::sort(scores.begin(), scores.end(),
        [](const HighScoreEntry& a, const HighScoreEntry& b) {
            return a.score > b.score;
        });

    if (scores.size() > 10) scores.resize(10);
    return scores;
}

// ============================================================================
// 2. Ghi thêm 1 dòng điểm mới
// ============================================================================
void SaveHighScoreEntry(const std::string& playerName, int score, int level) {
    time_t now = time(nullptr);
    struct tm timeinfo {};
    char timeStr[20];

#if defined(_WIN32)
    localtime_s(&timeinfo, &now);
#else
    localtime_r(&now, &timeinfo);
#endif
    strftime(timeStr, sizeof(timeStr), "%d/%m/%Y", &timeinfo);

    std::vector<HighScoreEntry> scores = LoadHighScores();
    scores.push_back(HighScoreEntry(playerName, score, level, timeStr));

    std::sort(scores.begin(), scores.end(),
        [](const HighScoreEntry& a, const HighScoreEntry& b) {
            return a.score > b.score;
        });
    if (scores.size() > 10) scores.resize(10);

    std::ofstream file(HIGHSCORE_FILE);
    if (!file) return;

    for (const auto& s : scores) {
        file << s.playerName << "|" << s.score << "|" << s.level << "|" << s.date << "\n";
    }
}

// ============================================================================
// 3. Auto-fit sprite
// ============================================================================
static void FitSpriteToWindow(sf::Sprite& sprite, const sf::RenderWindow& window, bool cover = true, sf::Vector2f anchor = { 0.5f, 0.5f })
{
    const sf::Texture* texture = sprite.getTexture();
    if (!texture) return;

    auto texSize = texture->getSize();
    auto winSize = window.getSize();

    if (texSize.x == 0 || texSize.y == 0 || winSize.x == 0 || winSize.y == 0) return;

    float scaleX = static_cast<float>(winSize.x) / texSize.x;
    float scaleY = static_cast<float>(winSize.y) / texSize.y;
    float scale = cover ? std::max(scaleX, scaleY) : std::min(scaleX, scaleY);

    sprite.setScale(scale, scale); // SFML 2.6: setScale(x, y) hoặc setScale(Vector2f) đều được

    float newW = texSize.x * scale;
    float newH = texSize.y * scale;

    float posX = (winSize.x - newW) * anchor.x;
    float posY = (winSize.y - newH) * anchor.y;

    sprite.setPosition(posX, posY);
}

// ============================================================================
// 4. Layout 
// ============================================================================
void RebuildScoreColumns(const std::vector<HighScoreEntry>& scores,
    const sf::Font& font,
    const sf::RenderWindow& window,
    std::vector<sf::Text>& nameTexts,
    std::vector<sf::Text>& scoreTexts,
    std::vector<sf::Text>& levelTexts,
    std::vector<sf::Text>& dateTexts,
    std::vector<float>& rowCentersY)
{
    nameTexts.clear(); scoreTexts.clear(); levelTexts.clear(); dateTexts.clear(); rowCentersY.clear();

    auto winSize = window.getSize();
    float w = static_cast<float>(winSize.x);
    float h = static_cast<float>(winSize.y);

    float boardLeft = 0.23f * w;
    float boardRight = 0.77f * w;
    float boardTop = 0.23f * h;
    float boardBottom = 0.82f * h;
    float boardWidth = boardRight - boardLeft;
    float boardHeight = boardBottom - boardTop;

    std::size_t count = std::min<std::size_t>(scores.size(), 10);
    if (count == 0) return;

    float topPadding = boardHeight * 0.27f;
    float bottomPadding = boardHeight * 0.05f;
    float innerHeight = boardHeight - topPadding - bottomPadding;
    float spacing = innerHeight / static_cast<float>(count);

    unsigned int charSize = static_cast<unsigned int>(innerHeight / (count + 1) * 0.8f);
    if (charSize < 18u) charSize = 18u;
    if (charSize > 36u) charSize = 36u;

    float colNameX = boardLeft + boardWidth * 0.20f;
    float colScoreX = boardLeft + boardWidth * 0.55f;
    float colLevelX = boardLeft + boardWidth * 0.60f;
    float colDateX = boardLeft + boardWidth * 0.70f;

    for (std::size_t i = 0; i < count; ++i) {
        const auto& s = scores[i];

        sf::Text name(s.playerName, font, charSize);
        sf::Text score(std::to_string(s.score) + " pts", font, charSize);

        std::ostringstream lv; lv << "Lv" << s.level;
        sf::Text level(lv.str(), font, charSize);

        sf::Text date(s.date, font, charSize);

        name.setFillColor(sf::Color::White);
        score.setFillColor(sf::Color::White);
        level.setFillColor(sf::Color::White);
        date.setFillColor(sf::Color::White);

        float yCenter = boardTop + topPadding + (i + 0.5f) * spacing;

        // --- SỬA LỖI SFML 2.6 TẠI ĐÂY ---
        // Thay vì b.position.x -> dùng b.left
        // Thay vì b.size.y     -> dùng b.height
        // Thay vì b.position.y -> dùng b.top

        // Name: căn trái
        {
            auto b = name.getLocalBounds();
            float x = colNameX - b.left;
            float yy = yCenter - b.height / 2.f - b.top;
            name.setPosition(x, yy);
        }
        // Score: căn phải
        {
            auto b = score.getLocalBounds();
            float x = colScoreX - b.width - b.left;
            float yy = yCenter - b.height / 2.f - b.top;
            score.setPosition(x, yy);
        }
        // Level: căn trái
        {
            auto b = level.getLocalBounds();
            float x = colLevelX - b.left;
            float yy = yCenter - b.height / 2.f - b.top;
            level.setPosition(x, yy);
        }
        // Date: căn trái
        {
            auto b = date.getLocalBounds();
            float x = colDateX - b.left;
            float yy = yCenter - b.height / 2.f - b.top;
            date.setPosition(x, yy);
        }

        nameTexts.push_back(name); // bỏ std::move cho an toàn với C++ cũ
        scoreTexts.push_back(score);
        levelTexts.push_back(level);
        dateTexts.push_back(date);
        rowCentersY.push_back(yCenter);
    }
}

// ============================================================================
// 5. Ghi sẵn mẫu
// ============================================================================
void WriteSampleScores() {
    std::ofstream file(HIGHSCORE_FILE);
    if (!file) return;
    file << "Eve|1600|5|21/11/2025\nCharlie|1500|4|28/10/2025\nAlice|1400|4|15/10/2025\n";
    file << "Bob|1300|3|10/10/2025\nDavid|1200|3|05/10/2025\nFrank|1100|3|01/10/2025\n";
    file << "Grace|1000|2|25/09/2025\nHeidi|900|2|20/09/2025\nIvan|800|2|15/09/2025\nJudy|700|1|10/09/2025\n";
}

// ============================================================================
// 6. Hiển thị
// ============================================================================
void ShowHighScores() {
    std::vector<HighScoreEntry> scores = LoadHighScores();

    sf::RenderWindow window(sf::VideoMode(1550, 1050), "Top 10 High Scores", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    sf::Font font;
    // --- SỬA LỖI SFML 2.6: Dùng loadFromFile thay vì openFromFile ---
    if (!font.loadFromFile(FONT_PATH)) {
        std::cerr << "Cannot load font: " << FONT_PATH << '\n';
        return;
    }

    sf::Texture bgTexture, backTex, goldTex, silverTex, bronzeTex, rankingTitleTex, listPanelTex;

    // Load ảnh (bỏ qua check error để code gọn, trong thực tế nên check)
    bgTexture.loadFromFile(BG_IMAGE_PATH);
    backTex.loadFromFile(BACK_BTN_PATH);
    goldTex.loadFromFile(GOLD_CUP_PATH);
    silverTex.loadFromFile(SILVER_CUP_PATH);
    bronzeTex.loadFromFile(BRONZE_CUP_PATH);
    rankingTitleTex.loadFromFile(RANKING_TITLE_PATH);
    listPanelTex.loadFromFile(LIST_PANEL_PATH);

    sf::Sprite background(bgTexture);
    sf::Sprite backSprite(backTex);
    sf::Sprite goldCup(goldTex), silverCup(silverTex), bronzeCup(bronzeTex);
    sf::Sprite rankingTitleSprite(rankingTitleTex);
    sf::Sprite listPanelSprite(listPanelTex);

    std::vector<sf::Text> nameTexts, scoreTexts, levelTexts, dateTexts;
    std::vector<float> rowCentersY;

    auto updateLayout = [&]() {
        FitSpriteToWindow(background, window, true);
        RebuildScoreColumns(scores, font, window, nameTexts, scoreTexts, levelTexts, dateTexts, rowCentersY);

        auto winSize = window.getSize();
        float w = (float)winSize.x;
        float h = (float)winSize.y;

        float boardLeft = 0.23f * w;
        float boardRight = 0.77f * w;
        float boardTop = 0.23f * h;
        float boardBottom = 0.82f * h;
        float boardWidth = boardRight - boardLeft;
        float boardHeight = boardBottom - boardTop;
        float headerHeight = boardHeight * 0.27f;

        // Banner
        if (rankingTitleTex.getSize().x > 0) {
            float texW = (float)rankingTitleTex.getSize().x;
            float texH = (float)rankingTitleTex.getSize().y;
            float scale = std::min((headerHeight * 8.0f) / texH, (boardWidth * 7.5f) / texW);
            rankingTitleSprite.setScale(scale, scale);
            float bannerW = texW * scale;
            float bannerX = boardLeft + (boardWidth - bannerW) / 2.f;
            float bannerY = boardTop + headerHeight * 0.02f;
            rankingTitleSprite.setPosition(bannerX, bannerY - 500.0f);
        }

        // Panel
        if (listPanelTex.getSize().x > 0) {
            float texW = (float)listPanelTex.getSize().x;
            float texH = (float)listPanelTex.getSize().y;
            float rowTop = boardTop + boardHeight * 0.27f;
            float rowBottom = boardBottom - boardHeight * 0.05f;
            float baseScale = std::min((rowBottom - rowTop) * 3.2f / texH, boardWidth * 5.0f / texW);

            float scaleX = baseScale * 1.2f;
            float scaleY = baseScale;
            listPanelSprite.setScale(scaleX, scaleY);

            float panelW = texW * scaleX;
            float panelH = texH * scaleY;
            float centerX = boardLeft + boardWidth * 0.5f;
            float centerY = (rowTop + rowBottom) * 0.5f;
            listPanelSprite.setPosition(centerX - panelW / 2.f, centerY - panelH / 2.f);
        }

        // Back Btn
        if (backTex.getSize().y > 0) {
            float scale = (0.12f * h) / backTex.getSize().y;
            backSprite.setScale(scale, scale);
            backSprite.setPosition(20.f, 20.f);
        }

        // Cups
        if (!rowCentersY.empty()) {
            unsigned int charSize = nameTexts.empty() ? 24u : nameTexts[0].getCharacterSize();
            float cupHeight = (float)charSize * 2.0f;
            float colNameX = boardLeft + boardWidth * 0.20f;
            float padding = -30.f;

            auto setupCup = [&](sf::Sprite& spr, const sf::Texture& tx, int idx) {
                if (idx >= (int)rowCentersY.size() || tx.getSize().y == 0) return;
                float s = cupHeight / (float)tx.getSize().y;
                spr.setScale(s, s);
                auto lb = spr.getLocalBounds(); // lb.width, lb.height (SFML 2.6)
                float scaledW = lb.width * s;
                float scaledH = lb.height * s;
                spr.setPosition(colNameX - padding - scaledW, rowCentersY[idx] - scaledH / 2.f);
                };
            setupCup(goldCup, goldTex, 0);
            setupCup(silverCup, silverTex, 1);
            setupCup(bronzeCup, bronzeTex, 2);
        }
        };

    updateLayout();

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed) window.close();
            if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) window.close();

            if (event.type == sf::Event::MouseButtonPressed && event.mouseButton.button == sf::Mouse::Left) {
                if (backSprite.getGlobalBounds().contains((float)event.mouseButton.x, (float)event.mouseButton.y)) {
                    window.close();
                }
            }
            if (event.type == sf::Event::Resized) {
                sf::FloatRect visibleArea(0, 0, (float)event.size.width, (float)event.size.height);
                window.setView(sf::View(visibleArea));
                updateLayout();
            }
        }

        window.clear(sf::Color(20, 60, 20));
        window.draw(background);
        if (listPanelTex.getSize().x > 0) window.draw(listPanelSprite);
        if (rankingTitleTex.getSize().x > 0) window.draw(rankingTitleSprite);

        if (goldTex.getSize().x > 0) window.draw(goldCup);
        if (silverTex.getSize().x > 0) window.draw(silverCup);
        if (bronzeTex.getSize().x > 0) window.draw(bronzeCup);

        for (const auto& t : nameTexts) window.draw(t);
        for (const auto& t : scoreTexts) window.draw(t);
        for (const auto& t : levelTexts) window.draw(t);
        for (const auto& t : dateTexts) window.draw(t);

        if (backTex.getSize().x > 0) window.draw(backSprite);
        window.display();
    }
}



