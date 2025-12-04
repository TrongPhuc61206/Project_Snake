#ifndef SETTING_H
#define SETTING_H

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>

extern bool setting_musicOn;
extern int setting_difficulty; // 0=easy, 1=normal, 2=hard
extern int setting_snakeColor; // 0-3
extern sf::Music bgm;


void showSettings(sf::RenderWindow& window);
#endif 