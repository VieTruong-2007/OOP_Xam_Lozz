#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <cctype>

using namespace Gdiplus;

using namespace std;

const int WINDOW_W = 800;
const int WINDOW_H = 500;
const int PADDLE_W = 140;
const int PADDLE_H = 18;
const int BALL_R = 7;
const int BRICK_W = 18;
const int BRICK_H = 18;
const int BRICK_SPACING_X = 4;
const int BRICK_SPACING_Y = 4;
const int UI_PANEL_X = 8;
const int UI_PANEL_Y = 8;
const int UI_PANEL_W = 210;
const int UI_PANEL_H = 58;
const int BRICK_TOP_OFFSET = 72;
const int BRICK_CLEAR_ABOVE_PADDLE = 60;
const int STAR_RADIUS = 8;
const float STAR_FALL_SPEED = 3.0f;
const int STARS_TO_WIN = 10;
const int STAR_SPAWN_CHANCE_PERCENT = 5;
const int ROUND_RADIUS_BRICK = 4;
const int ROUND_RADIUS_PADDLE = 8;
const int ROUND_RADIUS_HUD = 10;

struct Brick {
    RECT rc;
    bool alive;
    bool breakable;
};

struct Ball {
    int x;
    int y;
    int vx;
    int vy;
};

struct Star {
    float x;
    float y;
    bool active;
};

struct GameState {
    int paddleX;
    int score;
    int starsCollected;
    bool running;
    vector<Brick> bricks;
    vector<Star> stars;
    Ball ball;
};

struct ThemeSettings {
    std::string backgroundImage = "default";
    int backgroundR = 10, backgroundG = 20, backgroundB = 40;
    int brickFillR = 139, brickFillG = 69, brickFillB = 19;
    int brickOutlineR = 92, brickOutlineG = 46, brickOutlineB = 12;
    int paddleR = 255, paddleG = 210, paddleB = 70;
    int ballR = 255, ballG = 90, ballB = 90;
    int textR = 255, textG = 255, textB = 255;
};

GameState g;
ThemeSettings gTheme;
static int gLayoutRows = 0;
static int gLayoutCols = 0;
Bitmap* gBackground = nullptr;
ULONG_PTR gGdiplusToken = 0;

static std::string Trim(const std::string& s) {
    std::string::size_type start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    std::string::size_type end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

static bool FileExists(const std::string& path) {
    DWORD attr = GetFileAttributesA(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES;
}

static bool IsSafeThemeConfigPath(const std::string& path) {
    return path == "theme_setup.txt" || path == "theme.txt";
}

static bool IsSafeLocalImageName(const std::string& name) {
    if (name.empty() || name == "default") return false;
    if (name.find("..") != std::string::npos) return false;
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false;
    if (name.find(':') != std::string::npos) return false;
    for (std::string::size_type i = 0; i < name.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(name[i]);
        if (std::isalnum(c) || c == '_' || c == '.' || c == '-') continue;
        return false;
    }
    return true;
}

static int ClampInt(int value, int minVal, int maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
}

static int ClampColor(int value) {
    return ClampInt(value, 0, 255);
}

static void SpawnStar(int x, int y) {
    Star star;
    star.x = static_cast<float>(x);
    star.y = static_cast<float>(y);
    star.active = true;
    g.stars.push_back(star);
}

static void SpawnRandomBrick() {
    std::vector<int> deadIndices;
    for (int i = 0; i < static_cast<int>(g.bricks.size()); ++i) {
        if (g.bricks[i].breakable && !g.bricks[i].alive) deadIndices.push_back(i);
    }
    if (deadIndices.empty()) return;
    int chosenIndex = deadIndices[rand() % deadIndices.size()];
    g.bricks[chosenIndex].alive = true;
}

static bool SaveThemeSetting(const std::string& key, const std::string& value) {
    if (key == "BACKGROUND_IMAGE" && !IsSafeLocalImageName(value)) return false;

    std::ifstream in("theme_setup.txt");
    std::vector<std::string> lines;
    std::string line;
    bool found = false;
    while (std::getline(in, line)) {
        if (line.find(key + "=") == 0) {
            lines.push_back(key + "=" + value);
            found = true;
        } else {
            lines.push_back(line);
        }
    }
    in.close();
    if (!found) lines.push_back(key + "=" + value);

    std::ofstream out("theme_setup.txt", std::ios::trunc);
    for (const auto& text : lines) out << text << "\n";
    return out.good();
}

static bool CopyBackgroundToCache(const std::wstring& sourcePath, std::string& outCacheName) {
    std::wstring ext = L".jpg";
    size_t dot = sourcePath.find_last_of(L".");
    if (dot != std::wstring::npos) ext = sourcePath.substr(dot);

    std::string extStr = ".jpg";
    if (ext == L".png") extStr = ".png";
    else if (ext == L".gif") extStr = ".gif";
    else if (ext == L".bmp") extStr = ".bmp";
    else if (ext == L".jpeg") extStr = ".jpg";

    std::string cacheName = "background_cache" + extStr;
    std::wstring cachePath(cacheName.begin(), cacheName.end());
    if (!CopyFileW(sourcePath.c_str(), cachePath.c_str(), FALSE)) return false;

    outCacheName = cacheName;
    return true;
}

static bool AutoPickBackgroundImage(std::string& outPath) {
    const wchar_t* patterns[] = {L"*.jpg", L"*.jpeg", L"*.png", L"*.bmp", L"*.gif"};
    WIN32_FIND_DATAW fd = {};
    for (const auto* pattern : patterns) {
        std::wstring search = L".\\"; search += pattern;
        HANDLE h = FindFirstFileW(search.c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            std::wstring full = L".\\" + std::wstring(fd.cFileName);
            FindClose(h);
            outPath = std::string(full.begin(), full.end());
            return true;
        }
    }
    return false;
}

bool LoadThemeFromFile(const std::string& path);

static void LoadTheme() {
    if (!LoadThemeFromFile("theme_setup.txt")) {
        LoadThemeFromFile("theme.txt");
    }
}

bool LoadThemeFromFile(const std::string& path) {
    if (!IsSafeThemeConfigPath(path)) return false;

    std::ifstream in(path.c_str());
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::string::size_type pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = Trim(line.substr(0, pos));
        std::string value = Trim(line.substr(pos + 1));

        if (key == "BACKGROUND_IMAGE") {
            if (IsSafeLocalImageName(value)) gTheme.backgroundImage = value;
        } else if (key == "BACKGROUND_R") gTheme.backgroundR = ClampColor(atoi(value.c_str()));
        else if (key == "BACKGROUND_G") gTheme.backgroundG = ClampColor(atoi(value.c_str()));
        else if (key == "BACKGROUND_B") gTheme.backgroundB = ClampColor(atoi(value.c_str()));
        else if (key == "BRICK_FILL_R") gTheme.brickFillR = ClampColor(atoi(value.c_str()));
        else if (key == "BRICK_FILL_G") gTheme.brickFillG = ClampColor(atoi(value.c_str()));
        else if (key == "BRICK_FILL_B") gTheme.brickFillB = ClampColor(atoi(value.c_str()));
        else if (key == "BRICK_OUTLINE_R") gTheme.brickOutlineR = ClampColor(atoi(value.c_str()));
        else if (key == "BRICK_OUTLINE_G") gTheme.brickOutlineG = ClampColor(atoi(value.c_str()));
        else if (key == "BRICK_OUTLINE_B") gTheme.brickOutlineB = ClampColor(atoi(value.c_str()));
        else if (key == "PADDLE_R") gTheme.paddleR = ClampColor(atoi(value.c_str()));
        else if (key == "PADDLE_G") gTheme.paddleG = ClampColor(atoi(value.c_str()));
        else if (key == "PADDLE_B") gTheme.paddleB = ClampColor(atoi(value.c_str()));
        else if (key == "BALL_R") gTheme.ballR = ClampColor(atoi(value.c_str()));
        else if (key == "BALL_G") gTheme.ballG = ClampColor(atoi(value.c_str()));
        else if (key == "BALL_B") gTheme.ballB = ClampColor(atoi(value.c_str()));
        else if (key == "TEXT_R") gTheme.textR = ClampColor(atoi(value.c_str()));
        else if (key == "TEXT_G") gTheme.textG = ClampColor(atoi(value.c_str()));
        else if (key == "TEXT_B") gTheme.textB = ClampColor(atoi(value.c_str()));
    }
    return true;
}

bool LoadBackgroundImage(HWND hwnd) {
    if (IsSafeLocalImageName(gTheme.backgroundImage)) {
        const std::string& candidate = gTheme.backgroundImage;
        if (FileExists(candidate)) {
            std::wstring wpath(candidate.begin(), candidate.end());
            delete gBackground;
            Image* image = Image::FromFile(wpath.c_str(), FALSE);
            if (!image || image->GetLastStatus() != Ok) {
                delete image;
                return false;
            }
            gBackground = static_cast<Bitmap*>(image);
            return true;
        }
    }

    std::string autoPath;
    if (AutoPickBackgroundImage(autoPath)) {
        std::wstring sourcePath(autoPath.begin(), autoPath.end());
        std::string cacheName;
        if (CopyBackgroundToCache(sourcePath, cacheName)) {
            gTheme.backgroundImage = cacheName;
            SaveThemeSetting("BACKGROUND_IMAGE", cacheName);
            std::wstring cachePath(cacheName.begin(), cacheName.end());
            delete gBackground;
            Image* image = Image::FromFile(cachePath.c_str(), FALSE);
            if (!image || image->GetLastStatus() != Ok) {
                delete image;
                return false;
            }
            gBackground = static_cast<Bitmap*>(image);
            return true;
        }
    }

    OPENFILENAMEW ofn = {};
    WCHAR filePath[MAX_PATH] = L"";
    static const wchar_t filter[] = L"Image Files\0*.bmp;*.jpg;*.jpeg;*.png;*.gif\0All Files\0*.*\0";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) {
        return false;
    }

    std::wstring sourcePath(filePath);
    std::string cacheName;
    if (!CopyBackgroundToCache(sourcePath, cacheName)) return false;

    gTheme.backgroundImage = cacheName;
    SaveThemeSetting("BACKGROUND_IMAGE", cacheName);

    std::wstring cachePath(cacheName.begin(), cacheName.end());
    delete gBackground;
    Image* image = Image::FromFile(cachePath.c_str(), FALSE);
    if (!image || image->GetLastStatus() != Ok) {
        delete image;
        return false;
    }
    gBackground = static_cast<Bitmap*>(image);
    return true;
}

void InitGame() {
    g.paddleX = (WINDOW_W - PADDLE_W) / 2;
    g.score = 0;
    g.starsCollected = 0;
    g.running = true;
    g.bricks.clear();
    g.stars.clear();

    const int frameLeft = 0;
    const int rightWallLeft = WINDOW_W - BRICK_W;
    const int innerAvailW = WINDOW_W - 2 * BRICK_W;
    const int paddleTop = WINDOW_H - 50;
    const int originTop = BRICK_TOP_OFFSET + BRICK_H;
    const int innerAvailH = paddleTop - originTop - BRICK_CLEAR_ABOVE_PADDLE;

    gLayoutCols = (innerAvailW + BRICK_SPACING_X) / (BRICK_W + BRICK_SPACING_X);
    gLayoutRows = (innerAvailH + BRICK_SPACING_Y) / (BRICK_H + BRICK_SPACING_Y);
    if (gLayoutCols < 1) gLayoutCols = 1;
    if (gLayoutRows < 1) gLayoutRows = 1;

    const int innerW = gLayoutCols * BRICK_W + (gLayoutCols - 1) * BRICK_SPACING_X;
    const int innerH = gLayoutRows * BRICK_H + (gLayoutRows - 1) * BRICK_SPACING_Y;
    const int originLeft = BRICK_W + (innerAvailW - innerW) / 2;

    auto addBrick = [&](int left, int top, bool breakable) {
        Brick b;
        b.rc.left = left;
        b.rc.top = top;
        b.rc.right = left + BRICK_W;
        b.rc.bottom = top + BRICK_H;
        b.alive = true;
        b.breakable = breakable;
        g.bricks.push_back(b);
        return b;
    };

    const int topWallTop = BRICK_TOP_OFFSET;
    for (int left = 0; left + BRICK_W <= WINDOW_W; left += BRICK_W) {
        addBrick(left, topWallTop, false);
    }

    const int sideWallMaxBottom = paddleTop - BRICK_SPACING_Y;
    for (int i = 0; ; ++i) {
        const int top = originTop + i * BRICK_H;
        if (top + BRICK_H > sideWallMaxBottom) break;
        addBrick(frameLeft, top, false);
        addBrick(rightWallLeft, top, false);
    }

    int breakableBottom = 0;
    for (int r = 0; r < gLayoutRows; ++r) {
        for (int c = 0; c < gLayoutCols; ++c) {
            const int left = originLeft + c * (BRICK_W + BRICK_SPACING_X);
            const int top = originTop + r * (BRICK_H + BRICK_SPACING_Y);
            Brick b = addBrick(left, top, true);
            if (b.rc.bottom > breakableBottom) breakableBottom = b.rc.bottom;
        }
    }

    g.ball = { WINDOW_W / 2, breakableBottom + BALL_R, 5, -5 };
}

static void DrawRoundedRect(HDC hdc, const RECT& rc, COLORREF fillRgb, COLORREF borderRgb, int radius) {
    if (radius < 1) radius = 1;

    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);

    const REAL x = static_cast<REAL>(rc.left);
    const REAL y = static_cast<REAL>(rc.top);
    const REAL w = static_cast<REAL>(rc.right - rc.left - 1);
    const REAL h = static_cast<REAL>(rc.bottom - rc.top - 1);
    const REAL d = static_cast<REAL>(radius * 2);

    GraphicsPath path;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();

    SolidBrush fillBrush(Color(255, GetRValue(fillRgb), GetGValue(fillRgb), GetBValue(fillRgb)));
    graphics.FillPath(&fillBrush, &path);

    Pen borderPen(Color(255, GetRValue(borderRgb), GetGValue(borderRgb), GetBValue(borderRgb)), 1.0f);
    graphics.DrawPath(&borderPen, &path);
}

void DrawBrick(HDC hdc, const Brick& brick) {
    COLORREF fillRgb;
    COLORREF borderRgb;
    if (!brick.breakable) {
        fillRgb = RGB(180, 180, 180);
        borderRgb = RGB(100, 100, 100);
    } else {
        fillRgb = RGB(gTheme.brickFillR, gTheme.brickFillG, gTheme.brickFillB);
        borderRgb = RGB(gTheme.brickOutlineR, gTheme.brickOutlineG, gTheme.brickOutlineB);
    }
    DrawRoundedRect(hdc, brick.rc, fillRgb, borderRgb, ROUND_RADIUS_BRICK);
}

void DrawPaddle(HDC hdc) {
    RECT rc = { g.paddleX, WINDOW_H - 50, g.paddleX + PADDLE_W, WINDOW_H - 50 + PADDLE_H };
    DrawRoundedRect(hdc, rc, RGB(gTheme.paddleR, gTheme.paddleG, gTheme.paddleB),
                    RGB(220, 180, 40), ROUND_RADIUS_PADDLE);
}

static void DrawHud(HDC hdc) {
    RECT panel = { UI_PANEL_X, UI_PANEL_Y, UI_PANEL_X + UI_PANEL_W, UI_PANEL_Y + UI_PANEL_H };
    DrawRoundedRect(hdc, panel, RGB(0, 0, 0), RGB(255, 210, 70), ROUND_RADIUS_HUD);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    const int textX = UI_PANEL_X + 10;
    int textY = UI_PANEL_Y + 8;
    TextOutA(hdc, textX, textY, "Tro choi pha gach", -1);
    textY += 16;

    char scoreText[64];
    sprintf_s(scoreText, sizeof(scoreText), "Diem: %d", g.score);
    TextOutA(hdc, textX, textY, scoreText, (int)strlen(scoreText));
    textY += 16;

    char starText[64];
    sprintf_s(starText, sizeof(starText), "Sao: %d/%d", g.starsCollected, STARS_TO_WIN);
    TextOutA(hdc, textX, textY, starText, (int)strlen(starText));
}

void DrawBall(HDC hdc) {
    HBRUSH brush = CreateSolidBrush(RGB(gTheme.ballR, gTheme.ballG, gTheme.ballB));
    HBRUSH old = (HBRUSH)SelectObject(hdc, brush);
    Ellipse(hdc, g.ball.x - BALL_R, g.ball.y - BALL_R, g.ball.x + BALL_R, g.ball.y + BALL_R);
    SelectObject(hdc, old);
    DeleteObject(brush);
}

static void BounceBallAxis(int& pos, int& vel, int bound, bool positiveBound) {
    const int minSpeed = 4;
    if (positiveBound) {
        pos = bound;
        vel = -abs(vel);
        if (vel > -minSpeed) vel = -minSpeed;
    } else {
        pos = bound;
        vel = abs(vel);
        if (vel < minSpeed) vel = minSpeed;
    }
}

static void ResolveBallCollisions(int paddleTop) {
    if (g.ball.x <= BALL_R) {
        BounceBallAxis(g.ball.x, g.ball.vx, BALL_R, false);
    } else if (g.ball.x >= WINDOW_W - BALL_R) {
        BounceBallAxis(g.ball.x, g.ball.vx, WINDOW_W - BALL_R, true);
    }
    if (g.ball.y <= BALL_R) {
        BounceBallAxis(g.ball.y, g.ball.vy, BALL_R, false);
    }

    for (auto& brick : g.bricks) {
        if (!brick.alive) continue;

        const int nearestX = ClampInt(g.ball.x, brick.rc.left, brick.rc.right);
        const int nearestY = ClampInt(g.ball.y, brick.rc.top, brick.rc.bottom);
        const int dx = g.ball.x - nearestX;
        const int dy = g.ball.y - nearestY;

        if (dx * dx + dy * dy > BALL_R * BALL_R) continue;

        const bool hitBreakable = brick.breakable;
        if (hitBreakable) {
            brick.alive = false;
            g.score++;
        }

        const int spawnThreshold = (gLayoutRows * gLayoutCols * 2 + 2) / 3;
        if (hitBreakable && g.score >= spawnThreshold) {
            if (rand() % 100 < STAR_SPAWN_CHANCE_PERCENT) {
                SpawnStar((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom);
            }
        }

        if (dx == 0 && dy == 0) {
            const int pushLeft = g.ball.x - brick.rc.left;
            const int pushRight = brick.rc.right - g.ball.x;
            const int pushTop = g.ball.y - brick.rc.top;
            const int pushBottom = brick.rc.bottom - g.ball.y;
            const int minPush = min(min(pushLeft, pushRight), min(pushTop, pushBottom));
            if (minPush == pushLeft) {
                g.ball.x = brick.rc.left - BALL_R;
                g.ball.vx = -abs(g.ball.vx);
            } else if (minPush == pushRight) {
                g.ball.x = brick.rc.right + BALL_R;
                g.ball.vx = abs(g.ball.vx);
            } else if (minPush == pushTop) {
                g.ball.y = brick.rc.top - BALL_R;
                g.ball.vy = -abs(g.ball.vy);
            } else {
                g.ball.y = brick.rc.bottom + BALL_R;
                g.ball.vy = abs(g.ball.vy);
            }
        } else if (abs(dx) > abs(dy)) {
            if (dx > 0) {
                g.ball.x = brick.rc.right + BALL_R;
                g.ball.vx = abs(g.ball.vx);
            } else {
                g.ball.x = brick.rc.left - BALL_R;
                g.ball.vx = -abs(g.ball.vx);
            }
        } else {
            if (dy > 0) {
                g.ball.y = brick.rc.bottom + BALL_R;
                g.ball.vy = abs(g.ball.vy);
            } else {
                g.ball.y = brick.rc.top - BALL_R;
                g.ball.vy = -abs(g.ball.vy);
            }
        }

        if (abs(g.ball.vx) < 4) g.ball.vx = (g.ball.vx >= 0) ? 4 : -4;
        if (abs(g.ball.vy) < 4) g.ball.vy = (g.ball.vy >= 0) ? 4 : -4;

        if (hitBreakable && g.score >= spawnThreshold) {
            SpawnRandomBrick();
        }
        return;
    }

    const RECT paddle = { g.paddleX, paddleTop, g.paddleX + PADDLE_W, paddleTop + PADDLE_H };
    if (g.ball.vy <= 0) return;

    if (g.ball.y + BALL_R < paddle.top) return;
    if (g.ball.y - BALL_R > paddle.bottom + BALL_R) return;
    if (g.ball.x + BALL_R < paddle.left) return;
    if (g.ball.x - BALL_R > paddle.right) return;

    g.ball.vy = -abs(g.ball.vy);
    if (g.ball.vy > -4) g.ball.vy = -5;
    g.ball.y = paddle.top - BALL_R;

    const int paddleCenter = paddle.left + PADDLE_W / 2;
    const int offset = g.ball.x - paddleCenter;
    g.ball.vx = offset * 10 / (PADDLE_W / 2);
    if (g.ball.vx > 10) g.ball.vx = 10;
    if (g.ball.vx < -10) g.ball.vx = -10;
    if (g.ball.vx > -4 && g.ball.vx < 4) {
        g.ball.vx = (offset >= 0) ? 4 : -4;
    }
}

void UpdateGame() {
    if (!g.running) return;

    if (GetAsyncKeyState(VK_LEFT) & 0x8000) g.paddleX -= 10;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g.paddleX += 10;

    if (g.paddleX < 0) g.paddleX = 0;
    if (g.paddleX + PADDLE_W > WINDOW_W) g.paddleX = WINDOW_W - PADDLE_W;

    const int paddleTop = WINDOW_H - 50;
    const int SUBSTEPS = 4;
    for (int step = 0; step < SUBSTEPS; ++step) {
        g.ball.x += g.ball.vx / SUBSTEPS;
        g.ball.y += g.ball.vy / SUBSTEPS;
        ResolveBallCollisions(paddleTop);
    }

    const int maxSpeed = 10;
    if (g.ball.vx > maxSpeed) g.ball.vx = maxSpeed;
    if (g.ball.vx < -maxSpeed) g.ball.vx = -maxSpeed;
    if (g.ball.vy > maxSpeed) g.ball.vy = maxSpeed;
    if (g.ball.vy < -maxSpeed) g.ball.vy = -maxSpeed;

    if (g.ball.y >= WINDOW_H - 40) {
        g.running = false;
        return;
    }
    for (auto& star : g.stars) {
        if (!star.active) continue;
        star.y += STAR_FALL_SPEED;

        RECT paddle = { g.paddleX, WINDOW_H - 50, g.paddleX + PADDLE_W, WINDOW_H - 50 + PADDLE_H };
        if (star.y + STAR_RADIUS >= paddle.top && star.y - STAR_RADIUS <= paddle.bottom &&
            star.x >= paddle.left && star.x <= paddle.right) {
            star.active = false;
            g.starsCollected++;
            if (g.starsCollected >= STARS_TO_WIN) {
                g.running = false;
            }
        }

        if (star.y - STAR_RADIUS > WINDOW_H) {
            star.active = false;
        }
    }

    bool anyBreakableAlive = false;
    for (const auto& brick : g.bricks) {
        if (brick.breakable && brick.alive) {
            anyBreakableAlive = true;
            break;
        }
    }
    if (!anyBreakableAlive && g.starsCollected < STARS_TO_WIN) {
        g.running = false;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            LoadTheme();
            InitGame();
            LoadBackgroundImage(hwnd);
            SetTimer(hwnd, 1, 8, nullptr);
            return 0;
        case WM_TIMER:
            UpdateGame();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
            if (!g.running && wParam == VK_RETURN) {
                InitGame();
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, WINDOW_W, WINDOW_H);
            HGDIOBJ oldBM = SelectObject(memDC, memBM);

            if (gBackground != nullptr) {
                Graphics graphics(memDC);
                graphics.DrawImage(gBackground, 0, 0, WINDOW_W, WINDOW_H);
            } else {
                HBRUSH bg = CreateSolidBrush(RGB(gTheme.backgroundR, gTheme.backgroundG, gTheme.backgroundB));
                FillRect(memDC, &ps.rcPaint, bg);
                DeleteObject(bg);
            }

            for (const auto& brick : g.bricks) {
                if (brick.alive) DrawBrick(memDC, brick);
            }
            DrawPaddle(memDC);
            DrawBall(memDC);

            HBRUSH starBrush = CreateSolidBrush(RGB(255, 215, 0));
            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, starBrush);
            for (const auto& star : g.stars) {
                if (!star.active) continue;
                Ellipse(memDC,
                        static_cast<int>(star.x) - STAR_RADIUS,
                        static_cast<int>(star.y) - STAR_RADIUS,
                        static_cast<int>(star.x) + STAR_RADIUS,
                        static_cast<int>(star.y) + STAR_RADIUS);
            }
            SelectObject(memDC, oldBrush);
            DeleteObject(starBrush);

            DrawHud(memDC);

            if (!g.running) {
                SetBkMode(memDC, TRANSPARENT);
                SetTextColor(memDC, RGB(255, 255, 255));
                RECT msgRc = { WINDOW_W / 2 - 160, WINDOW_H / 2 - 20, WINDOW_W / 2 + 160, WINDOW_H / 2 + 20 };
                const char* msg = g.starsCollected >= STARS_TO_WIN ? "Chuc mung! Ban da thang!" : "Game over!";
                DrawTextA(memDC, msg, -1, &msgRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            BitBlt(hdc, 0, 0, WINDOW_W, WINDOW_H, memDC, 0, 0, SRCCOPY);
            SelectObject(memDC, oldBM);
            DeleteObject(memBM);
            DeleteDC(memDC);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            delete gBackground;
            gBackground = nullptr;
            if (gGdiplusToken != 0) {
                GdiplusShutdown(gGdiplusToken);
                gGdiplusToken = 0;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int) {
    srand(static_cast<unsigned>(time(nullptr)));
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gGdiplusToken, &gdiplusStartupInput, nullptr);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("BrickGameClass");
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    RECT windowRect = { 0, 0, WINDOW_W, WINDOW_H };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
    const int windowW = windowRect.right - windowRect.left;
    const int windowH = windowRect.bottom - windowRect.top;

    HWND hwnd = CreateWindowEx(
        0, TEXT("BrickGameClass"), TEXT("Tro choi pha gach"),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, windowW, windowH,
        nullptr, nullptr, hInstance, nullptr);

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
