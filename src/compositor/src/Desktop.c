#include "Desktop.h"

typedef struct { float r, g, b; } Color;

static const Color SILVER     = { 0.75f, 0.77f, 0.80f };
static const Color SILVER_DIM = { 0.40f, 0.42f, 0.45f };
static const Color BLACK      = { 0.02f, 0.02f, 0.025f };
static const Color PANEL      = { 0.07f, 0.075f, 0.09f };
static const Color SHADOW     = { 0.0f, 0.0f, 0.0f };

typedef struct {
    VkCommandBuffer cb;
    VkExtent2D extent;
} Ctx;

static void fillRect(const Ctx* c, int x, int y, int w, int h, Color col) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w > (int)c->extent.width ? (int)c->extent.width : x + w;
    int y1 = y + h > (int)c->extent.height ? (int)c->extent.height : y + h;
    if (x1 <= x0 || y1 <= y0) return;

    VkClearAttachment att = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .colorAttachment = 0,
        .clearValue.color = {{ col.r, col.g, col.b, 1.0f }},
    };
    VkClearRect rect = {
        .rect = { { x0, y0 }, { (uint32_t)(x1 - x0), (uint32_t)(y1 - y0) } },
        .baseArrayLayer = 0,
        .layerCount = 1,
    };
    vkCmdClearAttachments(c->cb, 1, &att, 1, &rect);
}

static void strokeRect(const Ctx* c, int x, int y, int w, int h, int t, Color col) {
    fillRect(c, x, y, w, t, col);
    fillRect(c, x, y + h - t, w, t, col);
    fillRect(c, x, y, t, h, col);
    fillRect(c, x + w - t, y, t, h, col);
}

static void drawWallpaper(const Ctx* c) {
    const int bands = 64;
    int H = (int)c->extent.height;
    for (int i = 0; i < bands; i++) {
        float t = (float)i / (bands - 1);
        Color col = { 0.01f + 0.07f * t, 0.01f + 0.08f * t, 0.02f + 0.11f * t };
        int y0 = H * i / bands;
        int y1 = H * (i + 1) / bands;
        fillRect(c, 0, y0, (int)c->extent.width, y1 - y0, col);
    }
}

static void drawDesktopIcons(const Ctx* c) {
    for (int i = 0; i < 4; i++) {
        int x = 24, y = 24 + i * 96;
        fillRect(c, x + 4, y + 4, 64, 64, SHADOW);
        fillRect(c, x, y, 64, 64, BLACK);
        strokeRect(c, x, y, 64, 64, 2, SILVER);
        fillRect(c, x + 20, y + 20, 24, 24, SILVER_DIM);
        strokeRect(c, x + 20, y + 20, 24, 24, 2, SILVER);
    }
}

static void drawWidget(const Ctx* c) {
    int w = 280, h = 160;
    int x = (int)c->extent.width - w - 32, y = 32;
    fillRect(c, x + 8, y + 10, w, h, SHADOW);
    fillRect(c, x, y, w, h, PANEL);
    strokeRect(c, x, y, w, h, 2, SILVER);
    fillRect(c, x + 16, y + 16, 120, 12, SILVER);
    for (int i = 0; i < 4; i++) {
        fillRect(c, x + 16, y + 48 + i * 22, 200 - i * 30, 8, SILVER_DIM);
    }
}

static void drawWindow(const Ctx* c) {
    int w = 640, h = 380;
    int x = ((int)c->extent.width - w) / 2, y = ((int)c->extent.height - h) / 2 - 24;
    fillRect(c, x + 12, y + 14, w, h, SHADOW);
    fillRect(c, x, y, w, h, PANEL);
    strokeRect(c, x, y, w, h, 2, SILVER);

    // Title bar: icon at left, title text block next to it, controls top right
    fillRect(c, x + 2, y + 2, w - 4, 32, BLACK);
    fillRect(c, x + 10, y + 9, 16, 16, SILVER);
    fillRect(c, x + 34, y + 14, 140, 6, SILVER);
    for (int i = 0; i < 4; i++) {
        int bx = x + w - 2 - (4 - i) * 32;
        strokeRect(c, bx + 6, y + 8, 18, 18, 2, SILVER);
    }
    fillRect(c, x + w - 2 - 26, y + 14, 6, 6, SILVER); // close mark

    // Content placeholder
    for (int i = 0; i < 8; i++) {
        fillRect(c, x + 24, y + 56 + i * 36, 420 - (i % 3) * 70, 8, SILVER_DIM);
    }
}

static void drawTaskbar(const Ctx* c) {
    const int barH = 48;
    int W = (int)c->extent.width, y = (int)c->extent.height - barH;
    fillRect(c, 0, y, W, barH, BLACK);
    fillRect(c, 0, y, W, 1, SILVER);

    // Start button
    strokeRect(c, 4, y + 4, 40, 40, 2, SILVER);
    fillRect(c, 16, y + 16, 16, 16, SILVER);

    // Task buttons (first one active -> inverted)
    for (int i = 0; i < 3; i++) {
        int bx = 60 + i * 168;
        if (i == 0) {
            fillRect(c, bx, y + 6, 160, 36, SILVER);
            fillRect(c, bx + 10, y + 16, 16, 16, BLACK);
            fillRect(c, bx + 34, y + 21, 90, 6, BLACK);
        } else {
            strokeRect(c, bx, y + 6, 160, 36, 1, SILVER_DIM);
            fillRect(c, bx + 10, y + 16, 16, 16, SILVER_DIM);
            fillRect(c, bx + 34, y + 21, 90, 6, SILVER_DIM);
        }
    }

    // System tray + clock (bottom right)
    for (int i = 0; i < 3; i++) {
        fillRect(c, W - 200 + i * 28, y + 16, 16, 16, SILVER_DIM);
    }
    strokeRect(c, W - 104, y + 6, 96, 36, 1, SILVER);
    fillRect(c, W - 90, y + 18, 68, 12, SILVER);
}

void recordDesktopScene(VkCommandBuffer commandBuffer, VkExtent2D extent) {
    Ctx c = { commandBuffer, extent };
    drawWallpaper(&c);
    drawDesktopIcons(&c);
    drawWidget(&c);
    drawWindow(&c);
    drawTaskbar(&c);
}
