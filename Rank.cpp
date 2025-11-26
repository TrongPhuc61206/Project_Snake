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
#include <optional>

struct HighScoreEntry {
    std::string playerName;
    int score;
    int level;
    std::string date;

    HighScoreEntry(std::string name = "", int sc = 0, int lv = 1, std::string dt = "")
        : playerName(std::move(name)), score(sc), level(lv), date(std::move(dt)) {
    }
};
static const std::string HIGHSCORE_FILE   = "highscores.txt";
static const std::string FONT_PATH        = "assets/fonts/Arial.ttf";
static const std::string BG_IMAGE_PATH    = "assets/images/BackGround.png";

// 4 ảnh mới
static const std::string BACK_BTN_PATH    = "assets/images/back.png";
static const std::string GOLD_CUP_PATH    = "assets/images/gold_cup.png";
static const std::string SILVER_CUP_PATH  = "assets/images/silver_cup.png";
static const std::string BRONZE_CUP_PATH  = "assets/images/bronze_cup.png";

// 👉 Banner Ranking Leaderboard
static const std::string RANKING_TITLE_PATH = "assets/images/Title.png"; 
static const std::string LIST_PANEL_PATH   = "assets/images/rank_list.png";



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
                HighScoreEntry entry(
                    name,
                    std::stoi(scoreStr),
                    std::stoi(levelStr),
                    date
                );
                scores.push_back(entry);
            } catch (...) {
                std::cerr << "Invalid line: " << line << '\n';
            }
        }
    }

    std::sort(scores.begin(), scores.end(),
        [](const HighScoreEntry& a, const HighScoreEntry& b) {
            return a.score > b.score;
        });

    if (scores.size() > 10)
        scores.resize(10);

    return scores;
}

// ============================================================================
// 2. Ghi thêm 1 dòng điểm mới (đang chưa dùng, sau này game thật có thể dùng)
// ============================================================================

void SaveHighScoreEntry(const std::string& playerName, int score, int level) {
    // Tạo chuỗi ngày hiện tại
    time_t now = time(nullptr);
    struct tm timeinfo{};
    char timeStr[20];

#if defined(_WIN32)
    localtime_s(&timeinfo, &now);
#else
    localtime_r(&now, &timeinfo);
#endif

    strftime(timeStr, sizeof(timeStr), "%d/%m/%Y", &timeinfo);

    // Đọc các điểm hiện có (đã sort giảm dần + cắt top10)
    std::vector<HighScoreEntry> scores = LoadHighScores();

    // Thêm entry mới
    scores.emplace_back(playerName, score, level, timeStr);

    // Sort lại & cắt còn 10
    std::sort(scores.begin(), scores.end(),
              [](const HighScoreEntry& a, const HighScoreEntry& b) {
                  return a.score > b.score;
              });
    if (scores.size() > 10)
        scores.resize(10);

    // Ghi lại toàn bộ file từ đầu (KHÔNG append)
    std::ofstream file(HIGHSCORE_FILE);
    if (!file) {
        std::cerr << " Cannot open highscores.txt for writing.\n";
        return;
    }

    for (const auto& s : scores) {
        file << s.playerName << "|"
             << s.score      << "|"
             << s.level      << "|"
             << s.date       << "\n";
    }
}


// ============================================================================
// 3. Auto-fit sprite theo window + ảnh
// ============================================================================
static void FitSpriteToWindow(sf::Sprite& sprite,
                              const sf::RenderWindow& window,
                              bool cover = true,
                              sf::Vector2f anchor = {0.5f, 0.5f})
{
    const sf::Texture& texture = sprite.getTexture();
    auto texSize = texture.getSize();
    auto winSize = window.getSize();

    if (texSize.x == 0 || texSize.y == 0 || winSize.x == 0 || winSize.y == 0)
        return;

    float scaleX = static_cast<float>(winSize.x) / texSize.x;
    float scaleY = static_cast<float>(winSize.y) / texSize.y;

    float scale = cover ? std::max(scaleX, scaleY)
                        : std::min(scaleX, scaleY);

    sprite.setScale({scale, scale});

    float newW = texSize.x * scale;
    float newH = texSize.y * scale;

    float posX = (winSize.x - newW) * anchor.x;
    float posY = (winSize.y - newH) * anchor.y;

    sprite.setPosition({posX, posY});
}

// ============================================================================
// 4.Layout dạng 4 cột: Name | Score | Lv | Date (thẳng hàng)
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
    nameTexts.clear();
    scoreTexts.clear();
    levelTexts.clear();
    dateTexts.clear();
    rowCentersY.clear();

    auto winSize = window.getSize();
    float w = static_cast<float>(winSize.x);
    float h = static_cast<float>(winSize.y);

    // Vùng "giấy"
    float boardLeft   = 0.23f * w;
    float boardRight  = 0.77f * w;
    float boardTop    = 0.23f * h;
    float boardBottom = 0.82f * h;

    float boardWidth  = boardRight - boardLeft;
    float boardHeight = boardBottom - boardTop;

    std::size_t count = std::min<std::size_t>(scores.size(), 10);
    if (count == 0)
        return;

float topPadding    = boardHeight * 0.27f;  
float bottomPadding = boardHeight * 0.05f;   


    float innerHeight = boardHeight - topPadding - bottomPadding;
    float spacing     = innerHeight / static_cast<float>(count);

    
    unsigned int charSize = static_cast<unsigned int>(innerHeight / (count + 1) * 0.8f);
    if (charSize < 18u) charSize = 18u;
    if (charSize > 36u) charSize = 36u;

    // Vị trí các cột
    float colNameX  = boardLeft + boardWidth * 0.20f;
    float colScoreX = boardLeft + boardWidth * 0.55f;
    float colLevelX = boardLeft + boardWidth * 0.60f;
    float colDateX  = boardLeft + boardWidth * 0.70f;

    for (std::size_t i = 0; i < count; ++i) {
        const auto& s = scores[i];

        sf::Text name(font, s.playerName, charSize);
        sf::Text score(font, std::to_string(s.score) + " pts", charSize);

        std::ostringstream lv;
        lv << "Lv" << s.level;
        sf::Text level(font, lv.str(), charSize);

        sf::Text date(font, s.date, charSize);

        name.setFillColor(sf::Color::White);
        score.setFillColor(sf::Color::White);
        level.setFillColor(sf::Color::White);
        date.setFillColor(sf::Color::White);

        // tâm mỗi dòng: bắt đầu từ boardTop + topPadding
        float yCenter = boardTop + topPadding + (i + 0.5f) * spacing;

        // Name: căn trái
        {
            auto b = name.getLocalBounds();
            float x  = colNameX - b.position.x;
            float yy = yCenter - b.size.y / 2.f - b.position.y;
            name.setPosition({x, yy});
        }

        // Score: căn phải
        {
            auto b = score.getLocalBounds();
            float x  = colScoreX - b.size.x - b.position.x;
            float yy = yCenter - b.size.y / 2.f - b.position.y;
            score.setPosition({x, yy});
        }

        // Level: căn trái
        {
            auto b = level.getLocalBounds();
            float x  = colLevelX - b.position.x;
            float yy = yCenter - b.size.y / 2.f - b.position.y;
            level.setPosition({x, yy});
        }

        // Date: căn trái
        {
            auto b = date.getLocalBounds();
            float x  = colDateX - b.position.x;
            float yy = yCenter - b.size.y / 2.f - b.position.y;
            date.setPosition({x, yy});
        }

        nameTexts.push_back(std::move(name));
        scoreTexts.push_back(std::move(score));
        levelTexts.push_back(std::move(level));
        dateTexts.push_back(std::move(date));
        rowCentersY.push_back(yCenter);  // để đặt cúp
    }
}


// ============================================================================
// 5.Ghi sẵn mẫu điểm (option 1)
// ============================================================================
void WriteSampleScores(){
    std::ofstream file(HIGHSCORE_FILE); // TRUNCATE
    if (!file) {
        std::cerr << " Cannot open highscores.txt for writing.\n";
        return;
    }

    // 10 dòng mẫu – bạn muốn sửa tên/điểm tùy ý
    file << "Eve|1600|5|21/11/2025\n";
    file << "Charlie|1500|4|28/10/2025\n";
    file << "Alice|1400|4|15/10/2025\n";
    file << "Bob|1300|3|10/10/2025\n";
    file << "David|1200|3|05/10/2025\n";
    file << "Frank|1100|3|01/10/2025\n";
    file << "Grace|1000|2|25/09/2025\n";
    file << "Heidi|900|2|20/09/2025\n";
    file << "Ivan|800|2|15/09/2025\n";
    file << "Judy|700|1|10/09/2025\n";
}


// ============================================================================
// 6️. Hiển thị top 10
// ============================================================================
void ShowHighScores() {
    // Đọc top scores
    std::vector<HighScoreEntry> scores = LoadHighScores();

    // Cửa sổ SFML
    sf::RenderWindow window(
        sf::VideoMode({1550u, 1050u}),
        "Top 10 High Scores",
        sf::Style::Titlebar | sf::Style::Close
    );
    window.setFramerateLimit(60);

    // Font
    sf::Font font;
    if (!font.openFromFile(FONT_PATH)) {
        std::cerr << "Cannot load font: " << FONT_PATH << '\n';
        return;
    }

    // Background
    sf::Texture bgTexture;
    if (!bgTexture.loadFromFile(BG_IMAGE_PATH)) {
        std::cerr << "Cannot load background: " << BG_IMAGE_PATH << '\n';
    }
    sf::Sprite background(bgTexture);

        // Textures cho nút Back + 3 cup
    sf::Texture backTex, goldTex, silverTex, bronzeTex;
    if (!backTex.loadFromFile(BACK_BTN_PATH)) {
        std::cerr << "Cannot load back button: " << BACK_BTN_PATH << '\n';
    }
    if (!goldTex.loadFromFile(GOLD_CUP_PATH)) {
        std::cerr << "Cannot load gold cup: " << GOLD_CUP_PATH << '\n';
    }
    if (!silverTex.loadFromFile(SILVER_CUP_PATH)) {
        std::cerr << "Cannot load silver cup: " << SILVER_CUP_PATH << '\n';
    }
    if (!bronzeTex.loadFromFile(BRONZE_CUP_PATH)) {
        std::cerr << "Cannot load bronze cup: " << BRONZE_CUP_PATH << '\n';
    }

    sf::Sprite backSprite(backTex);
    sf::Sprite goldCup(goldTex);
    sf::Sprite silverCup(silverTex);
    sf::Sprite bronzeCup(bronzeTex);

    // Banner Ranking Leaderboard
    sf::Texture rankingTitleTex;
    if (!rankingTitleTex.loadFromFile(RANKING_TITLE_PATH)) {
        std::cerr << "Cannot load ranking title: " << RANKING_TITLE_PATH << '\n';
    }
    sf::Sprite rankingTitleSprite(rankingTitleTex);

    // Khung giấy danh sách
    sf::Texture listPanelTex;
    if (!listPanelTex.loadFromFile(LIST_PANEL_PATH)) {
        std::cerr << "Cannot load list panel: " << LIST_PANEL_PATH << '\n';
    }
    sf::Sprite listPanelSprite(listPanelTex);

    // Tiêu đề bên trong tờ giấy
    sf::Text titleText(font, "TOP 10 HIGHEST SCORE", 40);
    titleText.setFillColor(sf::Color::Black);
    titleText.setOutlineColor(sf::Color::White);
    titleText.setOutlineThickness(2.f);

    // 4 cột text + vị trí tâm từng dòng (để đặt cúp)
    std::vector<sf::Text> nameTexts;
    std::vector<sf::Text> scoreTexts;
    std::vector<sf::Text> levelTexts;
    std::vector<sf::Text> dateTexts;
    std::vector<float>    rowCentersY;

    // Hàm cập nhật layout khi mở / resize
        auto updateLayout = [&]() {
        FitSpriteToWindow(background, window, true, {0.5f, 0.5f});

        RebuildScoreColumns(
            scores, font, window,
            nameTexts, scoreTexts, levelTexts, dateTexts, rowCentersY
        );

        auto winSize = window.getSize();
        float w = static_cast<float>(winSize.x);
        float h = static_cast<float>(winSize.y);

        // Vùng "giấy"
        float boardLeft   = 0.23f * w;
        float boardRight  = 0.77f * w;
        float boardTop    = 0.23f * h;
        float boardBottom = 0.82f * h;
        float boardWidth  = boardRight - boardLeft;
        float boardHeight = boardBottom - boardTop;

float headerHeight = boardHeight * 0.27f;

        // Banner Ranking + vị trí tiêu đề 
if (rankingTitleTex.getSize().x > 0 && rankingTitleTex.getSize().y > 0) {
    float texW = static_cast<float>(rankingTitleTex.getSize().x);
    float texH = static_cast<float>(rankingTitleTex.getSize().y);

    float maxBannerH = headerHeight * 8.0f;
    float maxBannerW = boardWidth   * 7.5f;

    float scale = std::min(maxBannerH / texH, maxBannerW / texW);
    if (scale <= 0.f) scale = 1.f;

    rankingTitleSprite.setScale({scale, scale});

    float bannerW = texW * scale;
    float bannerH = texH * scale;

    float bannerX = boardLeft + (boardWidth - bannerW) / 2.f;
    float bannerY = boardTop  + headerHeight * 0.02f;
    rankingTitleSprite.setPosition({bannerX, bannerY - 500.0f});

    // // Chữ "TOP 10 HIGHEST" ngay bên dưới banner
    // auto tb = titleText.getLocalBounds();
    // float tx = boardLeft + (boardWidth - tb.size.x) / 2.f - tb.position.x;
    // float ty = bannerY + bannerH + headerHeight * 0.03f - tb.position.y;
    // titleText.setPosition({tx, ty - 1400.0f});
}

        else {
    auto tb = titleText.getLocalBounds();
    float tx = boardLeft + (boardWidth - tb.size.x) / 2.f - tb.position.x;
    float ty = boardTop + boardHeight * 0.03f - tb.position.y;
    titleText.setPosition({tx, ty});
}

// -------- Khung giấy TOP1 → TOP10 --------
if (listPanelTex.getSize().x > 0 && listPanelTex.getSize().y > 0) {
    float texW = static_cast<float>(listPanelTex.getSize().x);
    float texH = static_cast<float>(listPanelTex.getSize().y);

    float rowTop    = boardTop + boardHeight * 0.27f;
    float rowBottom = boardBottom - boardHeight * 0.05f;
    float rowHeight = rowBottom - rowTop;

    float targetHeight = rowHeight * 3.2f;
    float maxWidth     = boardWidth * 5.0f;

    float baseScale = std::min(targetHeight / texH, maxWidth / texW);
    if (baseScale <= 0.f) baseScale = 1.f;

    float scaleX = baseScale * 1.2f;  
    float scaleY = baseScale;

    listPanelSprite.setScale({scaleX, scaleY});

    float panelW = texW * scaleX;
    float panelH = texH * scaleY;

    float centerX = boardLeft + boardWidth * 0.5f;
    float centerY = (rowTop + rowBottom) * 0.5f;

    float panelX = centerX - panelW / 2.f;
    float panelY = centerY - panelH / 2.f;

    listPanelSprite.setPosition({panelX, panelY});
}

        // --- Nút Back ở góc trên trái
        if (backTex.getSize().y > 0) {
            float targetH = 0.12f * h; 
            float scale   = targetH / backTex.getSize().y;
            backSprite.setScale({scale, scale});
            backSprite.setPosition({20.f, 20.f});
        }

        // --- Đặt 3 cái cúp nếu có ít nhất 1 dòng ---
        if (!rowCentersY.empty()) {
            unsigned int charSize = nameTexts.empty()
                                    ? 24u
                                    : nameTexts[0].getCharacterSize();
            float cupHeight = static_cast<float>(charSize) * 2.0f; 

            float colNameX = boardLeft + boardWidth * 0.20f;
            float padding  = -30.f; // khoảng cách giữa cúp và cột name

            auto setupCup = [&](sf::Sprite& sprite,
                    const sf::Texture& tex,
                    int rowIndex)
{
    if (rowIndex >= static_cast<int>(rowCentersY.size()))
        return;
    if (tex.getSize().y == 0)
        return;

    float scale = cupHeight / static_cast<float>(tex.getSize().y);

    
    sprite.setScale({scale, scale});

    auto lb = sprite.getLocalBounds();
    float scaledW = lb.size.x * scale;
    float scaledH = lb.size.y * scale;

    float x = colNameX - padding - scaledW;
    float y = rowCentersY[rowIndex] - scaledH / 2.f;

    sprite.setPosition({x, y});
};


            setupCup(goldCup,   goldTex,   0); // top 1
            setupCup(silverCup, silverTex, 1); // top 2
            setupCup(bronzeCup, bronzeTex, 2); // top 3
        }
    };


    updateLayout();

    // --- Vòng lặp chính ---
    while (window.isOpen()) {
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (event->is<sf::Event::KeyPressed>()) {
                auto keyEvt = event->getIf<sf::Event::KeyPressed>();
                if (keyEvt && keyEvt->code == sf::Keyboard::Key::Escape) {
                    window.close();
                }
            }

            // Click vào nút Back để thoát
            if (event->is<sf::Event::MouseButtonPressed>()) {
                auto mouseEvt = event->getIf<sf::Event::MouseButtonPressed>();
                if (mouseEvt && mouseEvt->button == sf::Mouse::Button::Left) {
                    sf::Vector2f mpos(
                        static_cast<float>(mouseEvt->position.x),
                        static_cast<float>(mouseEvt->position.y)
                    );
                    if (backSprite.getGlobalBounds().contains(mpos)) {
                        window.close(); // quay lại menu/game chính
                    }
                }
            }

            // Nếu resize thì cập nhật lại layout
            if (event->is<sf::Event::Resized>()) {
                updateLayout();
            }
        }

        window.clear(sf::Color(20, 60, 20));
        window.draw(background);

        // ★ Khung giấy vẽ trước
        if (listPanelTex.getSize().x > 0)
            window.draw(listPanelSprite);

        // 👉 Banner
        if (rankingTitleTex.getSize().x > 0)
            window.draw(rankingTitleSprite);

        //window.draw(titleText);          // tiêu đề chữ "TOP 10 HIGHEST"

        // 3 cúp
        if (goldTex.getSize().x   > 0) window.draw(goldCup);
        if (silverTex.getSize().x > 0) window.draw(silverCup);
        if (bronzeTex.getSize().x > 0) window.draw(bronzeCup);

        // 4 cột text
        for (const auto& t : nameTexts)  window.draw(t);
        for (const auto& t : scoreTexts) window.draw(t);
        for (const auto& t : levelTexts) window.draw(t);
        for (const auto& t : dateTexts)  window.draw(t);

        // Nút back vẽ sau cùng để nằm trên
        if (backTex.getSize().x > 0) window.draw(backSprite);

        window.display();

    }
}


// ============================================================================
// 7. main
// ============================================================================
int main() {
    std::cout << "=== Snake Game High Score Test ===\n";
    std::cout << "1. Add sample scores\n";
    std::cout << "2. Show High Scores (SFML window)\n";
    std::cout << "Your choice: ";

    int choice;
    std::cin >> choice;

    if (choice == 1) {
        WriteSampleScores();
        std::cout << "Sample scores saved to highscores.txt!\n";
    }
    else if (choice == 2) {
        ShowHighScores();
    }
    else {
        std::cout << "Invalid choice.\n";
    }

    return 0;
}
