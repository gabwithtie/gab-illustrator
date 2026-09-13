#include "SelectTool.hpp"
#include "../../App.hpp"

#include <algorithm>
#include <cmath>

namespace app {

SelectTool::SelectTool() : Tool("Select Tool") {}

ImVec2 SelectTool::MouseToImage(const CanvasContext& ctx) const {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    return ImVec2(
        (mouse.x - ctx.imageOrigin.x) / ctx.zoom,
        (mouse.y - ctx.imageOrigin.y) / ctx.zoom
    );
}

bool SelectTool::PointInPolygon(float x, float y, const std::vector<ImVec2>& poly) {
    bool inside = false;
    const size_t count = poly.size();
    if (count < 3) return false;

    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        const float xi = poly[i].x, yi = poly[i].y;
        const float xj = poly[j].x, yj = poly[j].y;

        const bool intersect = ((yi > y) != (yj > y)) &&
                               (x < (xj - xi) * (y - yi) / (yj - yi) + xi);
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
}

void SelectTool::ApplySelection(const CanvasContext& ctx) {
    auto& sel = SelectionManager::GetInstance();
    sel.EnsureSize(ctx.project.w, ctx.project.h);

    if (selectionOp == SelectionOp::New) {
        sel.Clear();
    }

    if (shapeMode == ShapeMode::Box) {
        const int minX = std::clamp(static_cast<int>(std::floor(std::min(dragStartImagePos.x, currentImagePos.x))), 0, ctx.project.w);
        const int maxX = std::clamp(static_cast<int>(std::ceil(std::max(dragStartImagePos.x, currentImagePos.x))), 0, ctx.project.w);
        const int minY = std::clamp(static_cast<int>(std::floor(std::min(dragStartImagePos.y, currentImagePos.y))), 0, ctx.project.h);
        const int maxY = std::clamp(static_cast<int>(std::ceil(std::max(dragStartImagePos.y, currentImagePos.y))), 0, ctx.project.h);

        const bool val = (selectionOp != SelectionOp::Subtract);
        for (int y = minY; y < maxY; ++y) {
            for (int x = minX; x < maxX; ++x) {
                sel.SetSelected(x, y, val);
            }
        }
    } else if (shapeMode == ShapeMode::Lasso && lassoPoints.size() >= 3) {
        float minX = static_cast<float>(ctx.project.w), minY = static_cast<float>(ctx.project.h);
        float maxX = 0.0f, maxY = 0.0f;
        for (const auto& pt : lassoPoints) {
            minX = std::min(minX, pt.x);
            minY = std::min(minY, pt.y);
            maxX = std::max(maxX, pt.x);
            maxY = std::max(maxY, pt.y);
        }

        const int iMinX = std::clamp(static_cast<int>(std::floor(minX)), 0, ctx.project.w);
        const int iMaxX = std::clamp(static_cast<int>(std::ceil(maxX)), 0, ctx.project.w);
        const int iMinY = std::clamp(static_cast<int>(std::floor(minY)), 0, ctx.project.h);
        const int iMaxY = std::clamp(static_cast<int>(std::ceil(maxY)), 0, ctx.project.h);

        const bool val = (selectionOp != SelectionOp::Subtract);
        for (int y = iMinY; y < iMaxY; ++y) {
            for (int x = iMinX; x < iMaxX; ++x) {
                if (PointInPolygon(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f, lassoPoints)) {
                    sel.SetSelected(x, y, val);
                }
            }
        }
    }

    sel.UpdateBounds();
}

bool SelectTool::ProcessCanvasInput(const CanvasContext& ctx) {
    SelectionManager::GetInstance().EnsureSize(ctx.project.w, ctx.project.h);

    if (!ctx.hovered && !isDragging) {
        return false;
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
        return false;
    }

    const ImVec2 mouseImg = MouseToImage(ctx);
    bool changed = false;

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        isDragging = true;
        dragStartImagePos = mouseImg;
        currentImagePos = mouseImg;
        lassoPoints.clear();
        lassoPoints.push_back(mouseImg);
    }

    if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        currentImagePos = mouseImg;
        if (shapeMode == ShapeMode::Lasso) {
            if (lassoPoints.empty() || 
                std::hypot(lassoPoints.back().x - mouseImg.x, lassoPoints.back().y - mouseImg.y) > 1.0f) {
                lassoPoints.push_back(mouseImg);
            }
        }
    }

    if (isDragging && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        currentImagePos = mouseImg;
        ApplySelection(ctx);
        isDragging = false;
        lassoPoints.clear();
        changed = true;
    }

    return changed;
}

void SelectTool::DrawOverlay(const CanvasContext& ctx, ImDrawList* drawList) {
    if (drawList == nullptr) return;

    auto toScreen = [&](const ImVec2& p) {
        return ImVec2(ctx.imageOrigin.x + p.x * ctx.zoom, ctx.imageOrigin.y + p.y * ctx.zoom);
    };

    const auto& sel = SelectionManager::GetInstance();

    // Render active selection bounding box
    if (sel.HasSelection()) {
        const auto b = sel.GetBounds();
        const ImVec2 p0 = toScreen(ImVec2(static_cast<float>(b.minX), static_cast<float>(b.minY)));
        const ImVec2 p1 = toScreen(ImVec2(static_cast<float>(b.maxX), static_cast<float>(b.maxY)));
        drawList->AddRect(p0, p1, IM_COL32(0, 162, 232, 255), 0.0f, 0, 1.5f);
    }

    // Render drag overlay shape
    if (isDragging) {
        const ImU32 outlineCol = IM_COL32(255, 255, 0, 255);
        const ImU32 fillCol = IM_COL32(255, 255, 0, 40);

        if (shapeMode == ShapeMode::Box) {
            const ImVec2 p0 = toScreen(dragStartImagePos);
            const ImVec2 p1 = toScreen(currentImagePos);
            drawList->AddRectFilled(p0, p1, fillCol);
            drawList->AddRect(p0, p1, outlineCol, 0.0f, 0, 1.5f);
        } else if (shapeMode == ShapeMode::Lasso && lassoPoints.size() >= 2) {
            std::vector<ImVec2> screenPts;
            screenPts.reserve(lassoPoints.size());
            for (const auto& pt : lassoPoints) {
                screenPts.push_back(toScreen(pt));
            }

            drawList->AddPolyline(screenPts.data(), static_cast<int>(screenPts.size()), outlineCol, true, 1.5f);
            drawList->AddConvexPolyFilled(screenPts.data(), static_cast<int>(screenPts.size()), fillCol);
        }
    }
}

void SelectTool::DrawSettingsUi() {
    ImGui::TextUnformatted("Select Tool Settings");

    int shapeSel = shapeMode == ShapeMode::Box ? 0 : 1;
    ImGui::RadioButton("Box", &shapeSel, 0); ImGui::SameLine();
    ImGui::RadioButton("Lasso", &shapeSel, 1);
    shapeMode = shapeSel == 0 ? ShapeMode::Box : ShapeMode::Lasso;

    int opSel = selectionOp == SelectionOp::New ? 0 : (selectionOp == SelectionOp::Add ? 1 : 2);
    ImGui::RadioButton("New", &opSel, 0); ImGui::SameLine();
    ImGui::RadioButton("Add", &opSel, 1); ImGui::SameLine();
    ImGui::RadioButton("Subtract", &opSel, 2);
    selectionOp = opSel == 0 ? SelectionOp::New : (opSel == 1 ? SelectionOp::Add : SelectionOp::Subtract);

    ImGui::Separator();
    if (ImGui::Button("Clear Selection")) {
        SelectionManager::GetInstance().Clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Select All")) {
        auto& ctxProject = App::GetInstance().project;
        SelectionManager::GetInstance().EnsureSize(ctxProject.w, ctxProject.h);
        SelectionManager::GetInstance().SelectAll();
    }
}

} // namespace app