#ifndef SETTING_H
#define SETTING_H

#include <SFML/Graphics.hpp>

// Khai báo bi?n toàn c?c (extern) ?? các file khác có th? truy c?p
extern bool setting_musicOn;
extern int setting_difficulty; // 0=easy, 1=normal, 2=hard
extern int setting_snakeColor; // 0-3

// Khai báo hàm
void showSettings(sf::RenderWindow& window);
#endif 