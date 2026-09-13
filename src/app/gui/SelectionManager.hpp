#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

namespace app {

struct SelectionRect {
    int minX{0};
    int minY{0};
    int maxX{0};
    int maxY{0};

    bool IsEmpty() const { return minX >= maxX || minY >= maxY; }
    int Width() const { return std::max(0, maxX - minX); }
    int Height() const { return std::max(0, maxY - minY); }
    bool Contains(int x, int y) const {
        return x >= minX && x < maxX && y >= minY && y < maxY;
    }
};

class SelectionManager {
public:
    static SelectionManager& GetInstance() {
        static SelectionManager instance;
        return instance;
    }

    SelectionManager(const SelectionManager&) = delete;
    SelectionManager& operator=(const SelectionManager&) = delete;

    void EnsureSize(int w, int h) {
        if (width == w && height == h && mask.size() == static_cast<size_t>(w * h)) {
            return;
        }
        width = w;
        height = h;
        mask.assign(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
        hasSelection = false;
        bounds = SelectionRect{};
    }

    bool IsSelected(int x, int y) const {
        if (x < 0 || y < 0 || x >= width || y >= height || mask.empty()) {
            return false;
        }
        return mask[static_cast<size_t>(y * width + x)] != 0;
    }

    void SetSelected(int x, int y, bool selected) {
        if (x < 0 || y < 0 || x >= width || y >= height || mask.empty()) {
            return;
        }
        mask[static_cast<size_t>(y * width + x)] = selected ? 1 : 0;
    }

    void Clear() {
        std::fill(mask.begin(), mask.end(), 0);
        hasSelection = false;
        bounds = SelectionRect{};
    }

    void SelectAll() {
        std::fill(mask.begin(), mask.end(), 1);
        hasSelection = (width > 0 && height > 0);
        bounds = SelectionRect{0, 0, width, height};
    }

    void UpdateBounds() {
        int minX = width, minY = height, maxX = -1, maxY = -1;
        bool any = false;

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (mask[static_cast<size_t>(y * width + x)] != 0) {
                    any = true;
                    if (x < minX) minX = x;
                    if (y < minY) minY = y;
                    if (x > maxX) maxX = x;
                    if (y > maxY) maxY = y;
                }
            }
        }

        hasSelection = any;
        bounds = any ? SelectionRect{minX, minY, maxX + 1, maxY + 1} : SelectionRect{};
    }

    void ShiftMask(int dx, int dy) {
        if (mask.empty() || (dx == 0 && dy == 0)) return;

        std::vector<uint8_t> newMask(mask.size(), 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (IsSelected(x, y)) {
                    int nx = x + dx;
                    int ny = y + dy;
                    if (nx >= 0 && ny >= 0 && nx < width && ny < height) {
                        newMask[static_cast<size_t>(ny * width + nx)] = 1;
                    }
                }
            }
        }
        mask = std::move(newMask);
        UpdateBounds();
    }

    bool HasSelection() const { return hasSelection; }
    SelectionRect GetBounds() const { return bounds; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    const std::vector<uint8_t>& GetMask() const { return mask; }

private:
    SelectionManager() = default;

    int width{0};
    int height{0};
    std::vector<uint8_t> mask;
    bool hasSelection{false};
    SelectionRect bounds;
};

} // namespace app