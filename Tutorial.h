// Tutorial.h

#ifndef TUTORIAL_H
#define TUTORIAL_H

#include <string>

static const std::string TUTORIAL_GUIDE_PATH = "images/TutorialGuide.png";

// Khai báo các hằng số tọa độ cho nút BACK (dựa trên ảnh 1024x1024)
const float ORIGINAL_BUTTON_X = 485.0f;
const float ORIGINAL_BUTTON_Y = 682.0f;
const float ORIGINAL_BUTTON_WIDTH = 82.0f;
const float ORIGINAL_BUTTON_HEIGHT = 33.0f;

void ShowTutorial();

#endif // TUTORIAL_H