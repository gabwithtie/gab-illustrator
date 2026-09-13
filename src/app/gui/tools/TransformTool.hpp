#pragma once

#include "Tool.hpp"
#include "ToolRegistry.hpp"
#include "../SelectionManager.hpp"

#include <imgui.h>
#include <vector>
#include <cstdint>

namespace app {

class TransformTool : public Tool {
public:
    TransformTool();

    bool ProcessCanvasInput(const CanvasContext& ctx) override;
    void DrawOverlay(const CanvasContext& ctx, ImDrawList* drawList) override;
    void DrawSettingsUi() override;

private:
    struct FloatingPixelBuffer {
        int originX{0};
        int originY{0};
        int width{0};
        int height{0};
        std::vector<uint32_t> pixels;
        std::vector<uint8_t> mask;

        void Clear() {
            width = 0;
            height = 0;
            pixels.clear();
            mask.clear();
        }

        bool IsValid() const { return width > 0 && height > 0 && !pixels.empty(); }
    };

    bool isDragging{false};
    ImVec2 dragStartMouse{0.0f, 0.0f};
    int offsetX{0};
    int offsetY{0};
    int startOffsetX{0};
    int startOffsetY{0};

    FloatingPixelBuffer floatBuffer;

    ImVec2 MouseToImage(const CanvasContext& ctx) const;
    void CaptureSelectionSnapshot(const CanvasContext& ctx);
    void ApplyTransformation(const CanvasContext& ctx);
};

GBE_REGISTER_TOOL(TransformTool);

} // namespace app