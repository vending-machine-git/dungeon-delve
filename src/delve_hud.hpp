#pragma once
#include "main.hpp"
#include "ui/Image.hpp"
#include "ui/Text.hpp"
#include "ui/Frame.hpp"
#include <algorithm>
#include <cmath>
#include <string>

namespace DelveHUD {
constexpr int width = 300;
inline const Uint32 brass = makeColorRGB(216, 179, 106);
inline const Uint32 ivory = makeColorRGB(231, 223, 202);
inline const Uint32 muted = makeColorRGB(156, 153, 142);
inline const Uint32 cyan = makeColorRGB(133, 205, 214);
inline const Uint32 danger = makeColorRGB(242, 109, 85);

struct Cooldown {
    const char* name = nullptr;
    unsigned int remaining = 0, total = 0;
    int slot = 0;
    bool passive = false;
    bool visible() const { return name && remaining > 0 && total > 0 && slot >= 0 && slot < 2; }
    float fraction() const { return total ? std::clamp(remaining / static_cast<float>(total), 0.f, 1.f) : 0.f; }
    SDL_Rect bounds() const { return {48 + (passive ? 142 : 0), 164 + slot * 82, 52, 52}; }
};

class Canvas {
public:
    SDL_Rect screen(SDL_Rect bounds) const {
        const int left = std::lround(bounds.x * scale + 12 * nativeScale);
        const int top = std::lround(bounds.y * scale + 80 * nativeScale);
        const int right = std::lround((bounds.x + bounds.w) * scale + 12 * nativeScale);
        const int bottom = std::lround((bounds.y + bounds.h) * scale + 80 * nativeScale);
        return {left, top, right - left, bottom - top};
    }
    void rectangle(SDL_Rect bounds, Uint32 color) const {
        if (auto image = Image::get("*#images/system/white.png")) image->drawColor(nullptr, screen(bounds), viewport, color);
    }
    void icon(const char* path, SDL_Rect bounds, Uint32 color = 0xffffffff) const {
        if (auto image = Image::get(path)) image->drawColor(nullptr, screen(bounds), viewport, color);
    }
    void panel(int top, int height) const {
        auto image = Image::get("*#images/ui/GenericWindow.png");
        rectangle({2, top + 3, width, height}, makeColor(0, 0, 0, 110));
        rectangle({4, top + 4, width - 8, height - 8}, makeColor(22, 24, 27, 240));
        if (!image) return;
        const int columns[] = {0, 16, width - 16, width};
        const int rows[] = {top, top + 16, top + height - 16, top + height};
        for (int row = 0; row < 3; ++row) {
            for (int column = 0; column < 3; ++column) {
                SDL_Rect section{column * 16, row * 16, 16, 16};
                image->drawColor(&section, screen({columns[column], rows[row], columns[column + 1] - columns[column], rows[row + 1] - rows[row]}), viewport, 0xffffffff);
            }
        }
    }
    void label(int left, int top, int available, const char* value, Uint32 color = ivory,
        const char* font = "lang/en.ttf#12#1", bool rightAligned = false, bool centered = false) const {
        std::string shortened = value;
        auto text = Text::get(shortened.c_str(), font, color, makeColorRGB(8, 9, 11));
        if (!text) return;
        while (text->getWidth() > available && !shortened.empty()) {
            size_t end = shortened.size() - 1;
            while (end > 0 && (static_cast<unsigned char>(shortened[end]) & 0xc0) == 0x80) --end;
            shortened.resize(end);
            text = Text::get((shortened + "...").c_str(), font, color, makeColorRGB(8, 9, 11));
            if (!text) return;
        }
        if (rightAligned) left += std::max(0, available - static_cast<int>(text->getWidth()));
        else if (centered) left += std::max(0, (available - static_cast<int>(text->getWidth())) / 2);
        text->draw({0, 0, 0, 0}, screen({left, top, static_cast<int>(text->getWidth()), static_cast<int>(text->getHeight())}), viewport);
    }
    void rule(int top, Uint32 color = brass) const {
        rectangle({12, top, width - 24, 1}, makeColorRGB(70, 64, 52));
        rectangle({width / 2 - 3, top - 1, 6, 3}, color);
        rectangle({width / 2 - 1, top - 3, 2, 7}, color);
    }
    void badge(SDL_Rect bounds, const char* value, Uint32 color) const {
        rectangle(bounds, makeColorRGB(73, 64, 48));
        rectangle({bounds.x + 1, bounds.y + 1, bounds.w - 2, bounds.h - 2}, makeColorRGB(28, 29, 30));
        label(bounds.x + 4, bounds.y + 2, bounds.w - 8, value, color, "lang/en.ttf#10#1", true);
    }
    void cooldown(const Cooldown& entry) const {
        if (!entry.visible()) return;
        const auto bounds = entry.bounds();
        const auto color = entry.passive ? cyan : brass;
        icon("*#images/rmf/cooldown-disc.png", bounds, makeColor(18, 21, 26, 240));
        icon("*#images/rmf/cooldown-ring.png", bounds, makeColorRGB(65, 68, 72));
        if (auto ring = Image::get("*#images/rmf/cooldown-ring.png")) {
            auto centered = screen(bounds);
            centered.x += centered.w / 2; centered.y += centered.h / 2;
            ring->drawClockwise(entry.fraction(), nullptr, centered, viewport, color);
        }
        char text[24];
        snprintf(text, sizeof(text), "%c%d", entry.passive ? 'P' : 'A', entry.slot + 1);
        label(bounds.x + 5, bounds.y + 9, bounds.w - 10, text, color, "lang/en.ttf#10#1", false, true);
        snprintf(text, sizeof(text), "%us", (entry.remaining + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND);
        label(bounds.x + 4, bounds.y + 23, bounds.w - 8, text, ivory, "lang/en.ttf#16#1", false, true);
        label(bounds.x - 40, bounds.y + 56, 132, entry.name, color, "lang/en.ttf#11#1", false, true);
    }
private:
    const double nativeScale = std::min(xres / static_cast<double>(std::max(1, Frame::virtualScreenX)),
        yres / static_cast<double>(std::max(1, Frame::virtualScreenY)));
    const double scale = std::max(1.0, nativeScale);
    const SDL_Rect viewport{0, 0, xres, yres};
};
}
