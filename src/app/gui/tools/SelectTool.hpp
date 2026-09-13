#pragma once

#include "Tool.hpp"
#include "ToolRegistry.hpp"
#include "../SelectionManager.hpp"

#include <imgui.h>
#include <vector>

namespace app {

class SelectTool : public Tool {
public:
    enum class ShapeMode { Box, Lasso };
    enum class SelectionOp { New, Add, Subtract };

    SelectTool();

    bool ProcessCanvasInput(const CanvasContext& ctx) override;
    void DrawOverlay(const CanvasContext& ctx, ImDrawList* drawList) override;
    void DrawSettingsUi() override;

private:
    ShapeMode shapeMode{ShapeMode::Box};
    SelectionOp selectionOp{SelectionOp::New};

    bool isDragging{false};
    ImVec2 dragStartImagePos{0.0f, 0.0f};
    ImVec2 currentImagePos{0.0f, 0.0f};

    std::vector<ImVec2> lassoPoints;

    ImVec2 MouseToImage(const CanvasContext& ctx) const;
    void ApplySelection(const CanvasContext& ctx);
    static bool PointInPolygon(float x, float y, const std::vector<ImVec2>& poly);
};

GBE_REGISTER_TOOL(SelectTool);

} // namespace app