#pragma once
#include "menu.h"
#include "ui.h"

Screen* createHomeScreen();
Screen* createDialScreen(int initialDetents = 0, int accel = 1);
Screen* createMainMenu();
Screen* createWeatherScreen();
Screen* createBusScreen();
Screen* createInfoScreen();
Screen* createJogScreen(bool open, bool wizard);
Screen* createRatioScreen(bool wizard);
