#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
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
const int BRICK_ROWS = 20;
const int BRICK_COLS = 25;
const int BRICK_SPACING_X = 5;
const int BRICK_SPACING_Y = 5;
const int BRICK_LEFT_OFFSET = 20;
const int BRICK_TOP_OFFSET = 20;
const int STAR_RADIUS = 8;
const float STAR_FALL_SPEED = 3.0f;
const int STARS_TO_WIN = 10;
const int STAR_SPAWN_CHANCE_PERCENT = 5;
const int STAR_SPAWN_THRESHOLD = (BRICK_ROWS * BRICK_COLS * 2 + 2) / 3;

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

static int ClampInt(int value, int minVal, int maxVal) {
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
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
        if (!g.bricks[i].alive) deadIndices.push_back(i);
    }
    if (deadIndices.empty()) return;
    int chosenIndex = deadIndices[rand() % deadIndices.size()];
    g.bricks[chosenIndex].alive = true;
}

static bool SaveThemeSetting(const std::string& key, const std::string& value) {
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

bool LoadThemeFromFile(const std::string& path) {
    std::ifstream in(path.c_str());
    if (!in.is_open()) return false;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::string::size_type pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = Trim(line.substr(0, pos));
        std::string value = Trim(line.substr(pos + 1));

        if (key == "BACKGROUND_IMAGE") gTheme.backgroundImage = value;
        else if (key == "BACKGROUND_R") gTheme.backgroundR = atoi(value.c_str());
        else if (key == "BACKGROUND_G") gTheme.backgroundG = atoi(value.c_str());
        else if (key == "BACKGROUND_B") gTheme.backgroundB = atoi(value.c_str());
        else if (key == "BRICK_FILL_R") gTheme.brickFillR = atoi(value.c_str());
        else if (key == "BRICK_FILL_G") gTheme.brickFillG = atoi(value.c_str());
        else if (key == "BRICK_FILL_B") gTheme.brickFillB = atoi(value.c_str());
        else if (key == "BRICK_OUTLINE_R") gTheme.brickOutlineR = atoi(value.c_str());
        else if (key == "BRICK_OUTLINE_G") gTheme.brickOutlineG = atoi(value.c_str());
        else if (key == "BRICK_OUTLINE_B") gTheme.brickOutlineB = atoi(value.c_str());
        else if (key == "PADDLE_R") gTheme.paddleR = atoi(value.c_str());
        else if (key == "PADDLE_G") gTheme.paddleG = atoi(value.c_str());
        else if (key == "PADDLE_B") gTheme.paddleB = atoi(value.c_str());
        else if (key == "BALL_R") gTheme.ballR = atoi(value.c_str());
        else if (key == "BALL_G") gTheme.ballG = atoi(value.c_str());
        else if (key == "BALL_B") gTheme.ballB = atoi(value.c_str());
        else if (key == "TEXT_R") gTheme.textR = atoi(value.c_str());
        else if (key == "TEXT_G") gTheme.textG = atoi(value.c_str());
        else if (key == "TEXT_B") gTheme.textB = atoi(value.c_str());
    }
    return true;
}

bool LoadBackgroundImage(HWND hwnd) {
    if (!gTheme.backgroundImage.empty() && gTheme.backgroundImage != "default") {
        std::string candidate = gTheme.backgroundImage;
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

    int totalRows = BRICK_ROWS + 2;
    int totalCols = BRICK_COLS + 2;
    for (int r = 0; r < totalRows; ++r) {
        for (int c = 0; c < totalCols; ++c) {
            Brick b;
            b.rc.left = BRICK_LEFT_OFFSET + c * (BRICK_W + BRICK_SPACING_X);
            b.rc.top = BRICK_TOP_OFFSET + r * (BRICK_H + BRICK_SPACING_Y);
            b.rc.right = b.rc.left + BRICK_W;
            b.rc.bottom = b.rc.top + BRICK_H;
            b.alive = true;
            b.breakable = (r > 0 && r < totalRows - 1 && c > 0 && c < totalCols - 1);
            g.bricks.push_back(b);
        }
    }

    g.ball = { WINDOW_W / 2, WINDOW_H - 120, 5, -5 };
}

void DrawBrick(HDC hdc, const Brick& brick) {
    HBRUSH fill;
    HBRUSH outline;
    if (!brick.breakable) {
        fill = CreateSolidBrush(RGB(180, 180, 180));
        outline = CreateSolidBrush(RGB(100, 100, 100));
    } else {
        fill = CreateSolidBrush(RGB(gTheme.brickFillR, gTheme.brickFillG, gTheme.brickFillB));
        outline = CreateSolidBrush(RGB(gTheme.brickOutlineR, gTheme.brickOutlineG, gTheme.brickOutlineB));
    }
    FillRect(hdc, &brick.rc, fill);
    FrameRect(hdc, &brick.rc, outline);
    DeleteObject(fill);
    DeleteObject(outline);
}

void DrawPaddle(HDC hdc) {
    RECT rc = { g.paddleX, WINDOW_H - 50, g.paddleX + PADDLE_W, WINDOW_H - 50 + PADDLE_H };
    HBRUSH brush = CreateSolidBrush(RGB(gTheme.paddleR, gTheme.paddleG, gTheme.paddleB));
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void DrawBall(HDC hdc) {
    HBRUSH brush = CreateSolidBrush(RGB(gTheme.ballR, gTheme.ballG, gTheme.ballB));
    HBRUSH old = (HBRUSH)SelectObject(hdc, brush);
    Ellipse(hdc, g.ball.x - BALL_R, g.ball.y - BALL_R, g.ball.x + BALL_R, g.ball.y + BALL_R);
    SelectObject(hdc, old);
    DeleteObject(brush);
}

void UpdateGame() {
    if (!g.running) return;

    if (GetAsyncKeyState(VK_LEFT) & 0x8000) g.paddleX -= 10;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g.paddleX += 10;

    if (g.paddleX < 0) g.paddleX = 0;
    if (g.paddleX + PADDLE_W > WINDOW_W) g.paddleX = WINDOW_W - PADDLE_W;

    g.ball.x += g.ball.vx;
    g.ball.y += g.ball.vy;

    if (g.ball.x <= BALL_R) {
        g.ball.x = BALL_R;
        g.ball.vx = abs(g.ball.vx);
    } else if (g.ball.x >= WINDOW_W - BALL_R) {
        g.ball.x = WINDOW_W - BALL_R;
        g.ball.vx = -abs(g.ball.vx);
    }
    if (g.ball.y <= BALL_R) {
        g.ball.y = BALL_R;
        g.ball.vy = abs(g.ball.vy);
    }

    if (g.ball.y >= WINDOW_H - 40) {
        g.running = false;
        return;
    }

    RECT paddle = { g.paddleX, WINDOW_H - 50, g.paddleX + PADDLE_W, WINDOW_H - 50 + PADDLE_H };
    if (g.ball.y + BALL_R >= paddle.top && g.ball.y - BALL_R <= paddle.bottom &&
        g.ball.x >= paddle.left && g.ball.x <= paddle.right) {
        g.ball.vy = -abs(g.ball.vy);
        g.ball.y = paddle.top - BALL_R;
        if (g.ball.x < paddle.left + PADDLE_W / 2) g.ball.vx = -abs(g.ball.vx);
        else g.ball.vx = abs(g.ball.vx);
        if (g.ball.vx == 0) g.ball.vx = (rand() % 2 == 0) ? 5 : -5;
    }

    for (auto& brick : g.bricks) {
            if (!brick.alive) continue;

            int nearestX = ClampInt(g.ball.x, brick.rc.left, brick.rc.right);
            int nearestY = ClampInt(g.ball.y, brick.rc.top, brick.rc.bottom);

            int dx = g.ball.x - nearestX;
            int dy = g.ball.y - nearestY;

            if (dx * dx + dy * dy <= BALL_R * BALL_R) {
                bool hitBreakable = brick.breakable;
                if (hitBreakable) {
                    brick.alive = false;
                    g.score++;
                }

                int spawnThreshold = STAR_SPAWN_THRESHOLD;
                if (hitBreakable && g.score >= spawnThreshold) {
                    if (rand() % 100 < STAR_SPAWN_CHANCE_PERCENT) {
                        SpawnStar((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom);
                    }
                }

                if (dx == 0 && dy == 0) {
                    g.ball.vx = -g.ball.vx;
                    g.ball.vy = -g.ball.vy;
                    g.ball.x += g.ball.vx;
                    g.ball.y += g.ball.vy;
                } else if (abs(dx) > abs(dy)) {
                    g.ball.vx = -g.ball.vx;
                    if (dx > 0) g.ball.x = brick.rc.right + BALL_R;
                    else g.ball.x = brick.rc.left - BALL_R;
                } else {
                    g.ball.vy = -g.ball.vy;
                    if (dy > 0) g.ball.y = brick.rc.bottom + BALL_R;
                    else g.ball.y = brick.rc.top - BALL_R;
                }

                if (hitBreakable && g.score >= spawnThreshold) {
                    SpawnRandomBrick();
                }
                break;
            }
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

    bool anyAlive = false;
    for (const auto& brick : g.bricks) {
        if (brick.alive) {
            anyAlive = true;
            break;
        }
    }
    if (!anyAlive && g.starsCollected < STARS_TO_WIN) {
        g.running = false;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            LoadThemeFromFile("theme_setup.txt");
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

            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(gTheme.textR, gTheme.textG, gTheme.textB));
            TextOutA(memDC, 20, 15, "Tro choi pha gach", -1);

            char scoreText[64];
            sprintf_s(scoreText, sizeof(scoreText), "Diem: %d", g.score);
            TextOutA(memDC, 20, 35, scoreText, (int)strlen(scoreText));

            char starText[64];
            sprintf_s(starText, sizeof(starText), "Sao: %d/%d", g.starsCollected, STARS_TO_WIN);
            TextOutA(memDC, 20, 55, starText, (int)strlen(starText));

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

            if (!g.running) {
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

    HWND hwnd = CreateWindowEx(
        0, TEXT("BrickGameClass"), TEXT("Tro choi pha gach"),
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, WINDOW_W, WINDOW_H,
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
