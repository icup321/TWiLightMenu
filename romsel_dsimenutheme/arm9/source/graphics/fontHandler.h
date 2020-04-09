#include "FontGraphic.h"

#pragma once

void fontInit();

void updateText(bool top);
void clearText(bool top);
void clearText();

void printSmall(bool top, int x, int y, const std::string &message, Alignment align = Alignment::left);
void printLarge(bool top, int x, int y, const std::string &message, Alignment align = Alignment::left);

int calcSmallFontWidth(const char *text);
int calcLargeFontWidth(const char *text);
