#include "TransformTool.hpp"
#include "../../App.hpp"

#include <algorithm>
#include <cmath>

namespace {

static ImU32 ToImU32(uint32_t rgba) {
    const uint8_t r = static_cast<uint8_t>((rgba >> 24) & 0xFFu);
    const uint8_t g = static_cast<uint8_t>((rgba >> 16) & 0xFFu);
    const uint8_t b = static_cast<uint8_t>((rgba >> 8) & 0xFFu);
    const uint8_t a = static_cast<uint8_t>(rgba & 0xFFu);
    return IM_COL32(r, g, b, a);
}

static uint32_t AlphaCompositeOver(uint32_t dst, uint32_t src) {
    const float sr = static_cast<float>((src >> 24) & 0xFFu) / 255.0f;
    const float sg = static_cast<float>((src >> 16) & 0xFFu) / 255.0f;
    const float sb = static_cast<float>((src >> 8) & 0xFFu) / 255.0f;
    const float sa = static_cast<float>(src & 0xFFu) / 255.0f;

    const float dr = static_cast<float>((dst >> 24) & 0xFFu) / 255.0f;
    const float dg = static_cast<float>((dst >> 16) & 0xFFu) / 255.0f;
    const float db = static_cast<float>((dst >> 8) & 0xFFu) / 255.0f;
    const float da = static_cast<float>(dst & 0xFFu) / 255.0f;

    const float outA = sa + da * (1.0f - sa);
    if (outA <= 0.0f) {
        return Model::Layer::Transparent;
    }

    const float outR = (sr * sa + dr * da * (1.0f - sa)) / outA;
    const float outG = (sg * sa + dg * da * (1.0f - sa)) / outA;
    const float outB = (sb * sa + db * da * (1.0f - sa)) / outA;

    const uint8_t r = static_cast<uint8_t>(std::clamp(outR, 0.0f, 1.0f) * 255.0f);
    const uint8_t g = static_cast<uint8_t>(std::clamp(outG, 0.0f, 1.0f) * 255.0f);
    const uint8_t b = static_cast<uint8_t>(std::clamp(outB, 0.0f, 1.0f) * 255.0f);
    const uint8_t a = static_cast<uint8_t>(std::clamp(outA, 0.0f, 1.0f) * 255.0f);

    return (static_cast<uint32_t>(r) << 24) |
           (static_cast<uint32_t>(g) << 16) |
           (static_cast<uint32_t>(b) << 8) |
           static_cast<uint32_t>(a);
}

} // namespace

namespace app {

TransformTool::TransformTool() : Tool("Transform Tool") {}

ImVec2 TransformTool::MouseToImage(const CanvasContext& ctx) const {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    return ImVec2(
        (mouse.x - ctx.imageOrigin.x) / ctx.zoom,
        (mouse.y - ctx.imageOrigin.y) / ctx.zoom
    );
}

void TransformTool::CaptureSelectionSnapshot(const CanvasContext& ctx) {
    auto& sel = SelectionManager::GetInstance();
    if (!sel.HasSelection() || ctx.selectedLayer == nullptr) {
        floatBuffer.Clear();
        return;
    }

    const auto bounds = sel.GetBounds();
    floatBuffer.originX = bounds.minX;
    floatBuffer.originY = bounds.minY;
    floatBuffer.width = bounds.Width();
    floatBuffer.height = bounds.Height();

    const size_t totalPixels = static_cast<size_t>(floatBuffer.width * floatBuffer.height);
    floatBuffer.pixels.assign(totalPixels, Model::Layer::Transparent);
    floatBuffer.mask.assign(totalPixels, 0);

    for (int y = bounds.minY; y < bounds.maxY; ++y) {
        for (int x = bounds.minX; x < bounds.maxX; ++x) {
            if (sel.IsSelected(x, y)) {
                const int localIdx = (y - bounds.minY) * floatBuffer.width + (x - bounds.minX);
                floatBuffer.pixels[static_cast<size_t>(localIdx)] = ctx.selectedLayer->GetPixel(x, y);
                floatBuffer.mask[static_cast<size_t>(localIdx)] = 1;
            }
        }
    }
}

void TransformTool::ApplyTransformation(const CanvasContext& ctx) {
    if (ctx.selectedLayer == nullptr || ctx.selectedLayerIndex < 0) return;
    if (!floatBuffer.IsValid()) return;

    auto& sel = SelectionManager::GetInstance();
    App& app = App::GetInstance();
    const int layerIndex = ctx.selectedLayerIndex;
    const int dx = offsetX;
    const int dy = offsetY;

    app.edit_history.Execute(app.project, "Transform Selection", [&](Model::Project& project) {
        if (layerIndex < 0 || layerIndex >= static_cast<int>(project.layers.size())) return;
        Model::Layer& layer = project.layers[static_cast<size_t>(layerIndex)];

        // Clear original source pixels on the layer
        for (int ly = 0; ly < floatBuffer.height; ++ly) {
            for (int lx = 0; lx < floatBuffer.width; ++lx) {
                const size_t localIdx = static_cast<size_t>(ly * floatBuffer.width + lx);
                if (floatBuffer.mask[localIdx] != 0) {
                    const int origX = floatBuffer.originX + lx;
                    const int origY = floatBuffer.originY + ly;
                    layer.SetPixel(origX, origY, Model::Layer::Transparent);
                }
            }
        }

        // Commit floating pixels to their target coordinates
        for (int ly = 0; ly < floatBuffer.height; ++ly) {
            for (int lx = 0; lx < floatBuffer.width; ++lx) {
                const size_t localIdx = static_cast<size_t>(ly * floatBuffer.width + lx);
                if (floatBuffer.mask[localIdx] != 0) {
                    const uint32_t color = floatBuffer.pixels[localIdx];
                    const int targetX = floatBuffer.originX + lx + dx;
                    const int targetY = floatBuffer.originY + ly + dy;

                    if (layer.IsInBounds(targetX, targetY)) {
                        layer.SetPixel(targetX, targetY, color);
                    }
                }
            }
        }
    });

    // Update active selection mask offset
    sel.ShiftMask(dx, dy);

    // Reset floating state
    floatBuffer.Clear();
    offsetX = 0;
    offsetY = 0;
    startOffsetX = 0;
    startOffsetY = 0;
    isDragging = false;
}

bool TransformTool::ProcessCanvasInput(const CanvasContext& ctx) {
    auto& sel = SelectionManager::GetInstance();
    if (!sel.HasSelection()) {
        floatBuffer.Clear();
        return false;
    }

    if (!floatBuffer.IsValid()) {
        CaptureSelectionSnapshot(ctx);
    }

    // Spacebar commits transformation
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        ApplyTransformation(ctx);
        return true;
    }

    const ImVec2 mouseImg = MouseToImage(ctx);
    const auto bounds = sel.GetBounds();

    const SelectionRect transformedBounds{
        bounds.minX + offsetX,
        bounds.minY + offsetY,
        bounds.maxX + offsetX,
        bounds.maxY + offsetY
    };

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ctx.hovered) {
        if (transformedBounds.Contains(static_cast<int>(std::floor(mouseImg.x)), static_cast<int>(std::floor(mouseImg.y)))) {
            isDragging = true;
            dragStartMouse = mouseImg;
            startOffsetX = offsetX;
            startOffsetY = offsetY;
        }
    }

    if (isDragging && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float dx = mouseImg.x - dragStartMouse.x;
        const float dy = mouseImg.y - dragStartMouse.y;
        offsetX = startOffsetX + static_cast<int>(std::round(dx));
        offsetY = startOffsetY + static_cast<int>(std::round(dy));
        return true;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        isDragging = false;
    }

    return false;
}

void TransformTool::DrawOverlay(const CanvasContext& ctx, ImDrawList* drawList) {
    if (drawList == nullptr) return;

    const auto& sel = SelectionManager::GetInstance();
    if (!sel.HasSelection() || !floatBuffer.IsValid()) return;

    auto toScreen = [&](const ImVec2& p) {
        return ImVec2(ctx.imageOrigin.x + p.x * ctx.zoom, ctx.imageOrigin.y + p.y * ctx.zoom);
    };

    const auto bounds = sel.GetBounds();
    const int step = std::max(1, static_cast<int>(std::ceil(1.0f / ctx.zoom)));

    // 1. Visually mask/erase original layer pixels if moved
    if (offsetX != 0 || offsetY != 0) {
        for (int ly = 0; ly < floatBuffer.height; ly += step) {
            for (int lx = 0; lx < floatBuffer.width; lx += step) {
                const size_t localIdx = static_cast<size_t>(ly * floatBuffer.width + lx);
                if (floatBuffer.mask[localIdx] == 0) continue;

                const int gx = floatBuffer.originX + lx;
                const int gy = floatBuffer.originY + ly;

                // Composite checkerboard + lower layers below selectedLayerIndex
                const ImVec2 p0 = toScreen(ImVec2(static_cast<float>(gx), static_cast<float>(gy)));
                const ImVec2 p1 = toScreen(ImVec2(static_cast<float>(std::min(gx + step, ctx.project.w)),
                                                 static_cast<float>(std::min(gy + step, ctx.project.h))));

                uint32_t compositedUnder = Model::Layer::Transparent;
                for (int i = 0; i < ctx.selectedLayerIndex; ++i) {
                    if (ctx.project.layers[static_cast<size_t>(i)].visible) {
                        compositedUnder = AlphaCompositeOver(compositedUnder, ctx.project.layers[static_cast<size_t>(i)].GetPixel(gx, gy));
                    }
                }

                // Render background tile over the original position
                const int checkerSize = 16;
                const int sx = static_cast<int>(p0.x - ctx.canvasOrigin.x);
                const int sy = static_cast<int>(p0.y - ctx.canvasOrigin.y);
                const bool light = ((sx / checkerSize) + (sy / checkerSize)) % 2 == 0;
                const ImU32 checkerCol = light ? IM_COL32(70, 70, 70, 255) : IM_COL32(55, 55, 55, 255);

                drawList->AddRectFilled(p0, p1, checkerCol);
                if ((compositedUnder & 0xFFu) != 0u) {
                    drawList->AddRectFilled(p0, p1, ToImU32(compositedUnder));
                }
            }
        }
    }

    // 2. Render live preview of moving pixels at target (x + offsetX, y + offsetY)
    for (int ly = 0; ly < floatBuffer.height; ly += step) {
        for (int lx = 0; lx < floatBuffer.width; lx += step) {
            const size_t localIdx = static_cast<size_t>(ly * floatBuffer.width + lx);
            if (floatBuffer.mask[localIdx] == 0) continue;

            const uint32_t pixelCol = floatBuffer.pixels[localIdx];
            if ((pixelCol & 0xFFu) == 0u) continue;

            const int targetX = floatBuffer.originX + lx + offsetX;
            const int targetY = floatBuffer.originY + ly + offsetY;

            const ImVec2 p0 = toScreen(ImVec2(static_cast<float>(targetX), static_cast<float>(targetY)));
            const ImVec2 p1 = toScreen(ImVec2(static_cast<float>(targetX + step), static_cast<float>(targetY + step)));

            drawList->AddRectFilled(p0, p1, ToImU32(pixelCol));
        }
    }

    // 3. Render initial source selection bounds (dashed white/gray)
    const ImVec2 origP0 = toScreen(ImVec2(static_cast<float>(bounds.minX), static_cast<float>(bounds.minY)));
    const ImVec2 origP1 = toScreen(ImVec2(static_cast<float>(bounds.maxX), static_cast<float>(bounds.maxY)));
    drawList->AddRect(origP0, origP1, IM_COL32(200, 200, 200, 150), 0.0f, 0, 1.0f);

    // 4. Render active target bounding box overlay (bright yellow)
    const ImVec2 transP0 = toScreen(ImVec2(static_cast<float>(bounds.minX + offsetX), static_cast<float>(bounds.minY + offsetY)));
    const ImVec2 transP1 = toScreen(ImVec2(static_cast<float>(bounds.maxX + offsetX), static_cast<float>(bounds.maxY + offsetY)));

    drawList->AddRectFilled(transP0, transP1, IM_COL32(255, 235, 59, 30));
    drawList->AddRect(transP0, transP1, IM_COL32(255, 235, 59, 255), 0.0f, 0, 2.0f);

    const float handleSize = 4.0f;
    drawList->AddCircleFilled(transP0, handleSize, IM_COL32(255, 255, 255, 255));
    drawList->AddCircleFilled(ImVec2(transP1.x, transP0.y), handleSize, IM_COL32(255, 255, 255, 255));
    drawList->AddCircleFilled(ImVec2(transP0.x, transP1.y), handleSize, IM_COL32(255, 255, 255, 255));
    drawList->AddCircleFilled(transP1, handleSize, IM_COL32(255, 255, 255, 255));
}

void TransformTool::DrawSettingsUi() {
    ImGui::TextUnformatted("Transform Tool Settings");

    const auto& sel = SelectionManager::GetInstance();
    if (!sel.HasSelection()) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "No active selection.");
        return;
    }

    ImGui::Text("Offset X: %d, Y: %d", offsetX, offsetY);

    if (ImGui::Button("Commit (Spacebar)")) {
        ApplyTransformation(Tool::CanvasContext{
            App::GetInstance().project,
            App::GetInstance().selected_layer_index < static_cast<int>(App::GetInstance().project.layers.size())
                ? &App::GetInstance().project.layers[static_cast<size_t>(App::GetInstance().selected_layer_index)]
                : nullptr,
            App::GetInstance().selected_layer_index,
            ImVec2(0, 0), ImVec2(0, 0), ImVec2(0, 0), 1.0f, false, false
        });
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Left Click + Drag inside yellow box to offset pixels.");
    ImGui::TextUnformatted("Press Spacebar to commit the transformation.");
}

} // namespace app