#include "ImageTools.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

using namespace geode::prelude;

namespace ai::img {

Result<Bitmap> load(std::filesystem::path const& path) {
    auto ext = geode::utils::string::toLower(path.extension().string());
    CCImage::EImageFormat fmt = CCImage::kFmtPng;
    if (ext == ".jpg" || ext == ".jpeg") fmt = CCImage::kFmtJpg;

    auto image = new CCImage();
    if (!image->initWithImageFile(path.string().c_str(), fmt)) {
        image->release();
        return Err("Could not decode image (use PNG or JPG)");
    }
    int w = image->getWidth(), h = image->getHeight();
    auto data = image->getData();
    if (!data || w <= 0 || h <= 0) {
        image->release();
        return Err("Image is empty");
    }
    int bpp = image->hasAlpha() ? 4 : 3;
    Bitmap bmp;
    bmp.w = w; bmp.h = h;
    bmp.px.resize(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < static_cast<size_t>(w) * h; i++) {
        bmp.px[i * 4 + 0] = data[i * bpp + 0];
        bmp.px[i * 4 + 1] = data[i * bpp + 1];
        bmp.px[i * 4 + 2] = data[i * bpp + 2];
        bmp.px[i * 4 + 3] = bpp == 4 ? data[i * bpp + 3] : 255;
    }
    image->release();
    return Ok(std::move(bmp));
}

Bitmap resize(Bitmap const& src, int w, int h) {
    Bitmap out;
    out.w = w; out.h = h;
    out.px.assign(static_cast<size_t>(w) * h * 4, 0);
    for (int y = 0; y < h; y++) {
        int sy0 = y * src.h / h;
        int sy1 = std::max(sy0 + 1, (y + 1) * src.h / h);
        for (int x = 0; x < w; x++) {
            int sx0 = x * src.w / w;
            int sx1 = std::max(sx0 + 1, (x + 1) * src.w / w);
            double r = 0, g = 0, b = 0, a = 0;
            int n = 0;
            for (int sy = sy0; sy < sy1 && sy < src.h; sy++) {
                for (int sx = sx0; sx < sx1 && sx < src.w; sx++) {
                    auto p = &src.px[(static_cast<size_t>(sy) * src.w + sx) * 4];
                    double al = p[3] / 255.0;
                    r += p[0] * al; g += p[1] * al; b += p[2] * al; a += al;
                    n++;
                }
            }
            auto o = &out.px[(static_cast<size_t>(y) * w + x) * 4];
            if (n > 0 && a > 0) {
                o[0] = static_cast<uint8_t>(r / a);
                o[1] = static_cast<uint8_t>(g / a);
                o[2] = static_cast<uint8_t>(b / a);
                o[3] = static_cast<uint8_t>(255.0 * a / n);
            }
        }
    }
    return out;
}

Bitmap circleCrop(Bitmap const& src, int size) {
    int side = std::min(src.w, src.h);
    int ox = (src.w - side) / 2, oy = (src.h - side) / 2;
    Bitmap square;
    square.w = square.h = side;
    square.px.resize(static_cast<size_t>(side) * side * 4);
    for (int y = 0; y < side; y++) {
        std::copy_n(&src.px[(static_cast<size_t>(y + oy) * src.w + ox) * 4],
                    static_cast<size_t>(side) * 4,
                    &square.px[static_cast<size_t>(y) * side * 4]);
    }
    Bitmap out = resize(square, size, size);
    float c = size / 2.f, r = size / 2.f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float d = std::hypot(x + 0.5f - c, y + 0.5f - c);
            float cover = std::clamp(r - d + 0.5f, 0.f, 1.f); // 1px anti-aliased rim
            auto p = &out.px[(static_cast<size_t>(y) * size + x) * 4];
            p[3] = static_cast<uint8_t>(p[3] * cover);
        }
    }
    return out;
}

Result<> savePng(Bitmap const& bmp, std::filesystem::path const& path) {
    auto image = new CCImage();
    auto copy = bmp.px; // initWithImageData takes a non-const pointer
    if (!image->initWithImageData(copy.data(), static_cast<int>(copy.size()),
                                  CCImage::kFmtRawData, bmp.w, bmp.h, 8)) {
        image->release();
        return Err("Could not build image");
    }
    bool ok = image->saveToFile(path.string().c_str(), false);
    image->release();
    if (!ok) return Err("Could not write PNG");
    return Ok();
}

std::string base64File(std::filesystem::path const& path) {
    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    static char const* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((d.size() + 2) / 3 * 4);
    for (size_t i = 0; i < d.size(); i += 3) {
        uint32_t v = d[i] << 16;
        if (i + 1 < d.size()) v |= d[i + 1] << 8;
        if (i + 2 < d.size()) v |= d[i + 2];
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += i + 1 < d.size() ? T[(v >> 6) & 63] : '=';
        out += i + 2 < d.size() ? T[v & 63] : '=';
    }
    return out;
}

Quantized quantize(Bitmap const& bmp, int maxColors) {
    Quantized q;
    q.w = bmp.w; q.h = bmp.h;
    q.idx.assign(static_cast<size_t>(bmp.w) * bmp.h, -1);

    using Col = std::array<int, 3>;
    std::vector<Col> cols(q.idx.size());
    std::vector<int> opaque;
    for (size_t i = 0; i < q.idx.size(); i++) {
        auto p = &bmp.px[i * 4];
        cols[i] = {p[0], p[1], p[2]};
        if (p[3] >= 128) opaque.push_back(static_cast<int>(i));
    }
    if (opaque.empty()) return q;

    std::vector<std::vector<int>> boxes{opaque};
    auto range = [&](std::vector<int> const& b, int& chan) {
        int lo[3] = {255, 255, 255}, hi[3] = {0, 0, 0};
        for (int i : b) for (int c = 0; c < 3; c++) {
            lo[c] = std::min(lo[c], cols[i][c]);
            hi[c] = std::max(hi[c], cols[i][c]);
        }
        int best = 0; chan = 0;
        for (int c = 0; c < 3; c++) if (hi[c] - lo[c] >= best) { best = hi[c] - lo[c]; chan = c; }
        return best;
    };
    while (static_cast<int>(boxes.size()) < maxColors) {
        int pick = -1, bestScore = 0, pickChan = 0;
        for (size_t i = 0; i < boxes.size(); i++) {
            if (boxes[i].size() < 2) continue;
            int chan;
            int score = range(boxes[i], chan) * static_cast<int>(boxes[i].size());
            if (score > bestScore) { bestScore = score; pick = static_cast<int>(i); pickChan = chan; }
        }
        if (pick < 0) break;
        auto b = std::move(boxes[pick]);
        std::sort(b.begin(), b.end(), [&](int a, int c) { return cols[a][pickChan] < cols[c][pickChan]; });
        auto mid = b.begin() + b.size() / 2;
        boxes[pick] = std::vector<int>(b.begin(), mid);
        boxes.emplace_back(mid, b.end());
    }
    for (auto const& b : boxes) {
        if (b.empty()) continue;
        long r = 0, g = 0, bl = 0;
        for (int i : b) { r += cols[i][0]; g += cols[i][1]; bl += cols[i][2]; }
        auto n = static_cast<long>(b.size());
        q.palette.push_back({static_cast<GLubyte>(r / n), static_cast<GLubyte>(g / n), static_cast<GLubyte>(bl / n)});
    }
    for (int i : opaque) {
        int best = 0, bd = INT32_MAX;
        for (size_t k = 0; k < q.palette.size(); k++) {
            int dr = cols[i][0] - q.palette[k].r, dg = cols[i][1] - q.palette[k].g, db = cols[i][2] - q.palette[k].b;
            int d = dr * dr + dg * dg + db * db;
            if (d < bd) { bd = d; best = static_cast<int>(k); }
        }
        q.idx[i] = best;
    }
    return q;
}

} // namespace ai::img
