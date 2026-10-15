#pragma once
#include <Geode/Geode.hpp>
#include <filesystem>
#include <vector>
#include <cstdint>

// RGBA8 bitmap, row-major, top row first.
struct Bitmap {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    bool empty() const { return w <= 0 || h <= 0; }
};

struct Quantized {
    int w = 0, h = 0;
    std::vector<cocos2d::ccColor3B> palette;
    std::vector<int> idx; // palette index per pixel, -1 = transparent
};

namespace ai::img {
    geode::Result<Bitmap> load(std::filesystem::path const& path);
    Bitmap resize(Bitmap const& src, int w, int h);            // box filter
    Bitmap circleCrop(Bitmap const& src, int size);            // center square -> size x size circle
    geode::Result<> savePng(Bitmap const& bmp, std::filesystem::path const& path);
    Quantized quantize(Bitmap const& bmp, int maxColors);      // median cut
    std::string base64File(std::filesystem::path const& path); // for sending to the AI
}
