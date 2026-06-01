#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <cctype>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <cmath>

using namespace Gdiplus;

using namespace std;

const int WINDOW_W = 800;
const int WINDOW_H = 500;
const int PADDLE_W = 140;
const int PADDLE_W_INCREASED = 220;
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
const int RED_BRICK_STAR_DROP_PERCENT = 10;
const int WHITE_BRICK_DROP_PIERCE_PERCENT = 10;
const int WHITE_BRICK_DROP_NONE_PERCENT = 20;
const int WHITE_BRICK_DROP_TRIPLE_PERCENT = 15;
const int WHITE_BRICK_DROP_EXTRA3_PERCENT = 15;
const int WHITE_BRICK_DROP_SPEED_PERCENT = 15;
const int WHITE_BRICK_DROP_PADDLE_INCREASE_PERCENT = 15;
const int WHITE_BRICK_DROP_FIXED_PADDLE_PERCENT = 10;
const int POWERUP_DURATION = 375; // frames (~3 seconds at 8ms per frame)
const int PIERCE_DURATION = POWERUP_DURATION;
const int SPEED_DURATION = POWERUP_DURATION;
const int PADDLE_SIZE_DURATION = POWERUP_DURATION;
const int FIXED_PADDLE_DURATION = POWERUP_DURATION;
const int EXTRA_BALL_COUNT = 3;
const int MAX_BALLS = 8;
const int ROUND_RADIUS_BRICK = 4;
const int ROUND_RADIUS_PADDLE = 8;
const int ROUND_RADIUS_HUD = 10;

const std::string ASSET_FOLDER = "assets/";
const std::string THEME_SETUP_FILE = ASSET_FOLDER + "theme_setup.txt";
const std::string THEME_FILE = ASSET_FOLDER + "theme.txt";
const std::string BACKGROUND_CACHE_PREFIX = ASSET_FOLDER + "background_cache";

enum BrickType {
    BRICK_TYPE_GRAY = 0,
    BRICK_TYPE_WHITE = 1,
    BRICK_TYPE_RED = 2,
};

struct Brick {
    RECT rc;
    bool alive;
    bool breakable;
    int type;
};

struct Ball {
    int x;
    int y;
    int vx;
    int vy;
    bool active;
};

enum DropType {
    DROP_STAR = 0,
    DROP_FUNCTION = 1,
    DROP_PIERCE = 2,
    DROP_TRIPLE_UP = 3,
    DROP_EXTRA_BALLS = 4,
    DROP_SPEED = 5,
    DROP_PADDLE_INCREASE = 6,
    DROP_FIXED_PADDLE = 7,
};

struct Drop {
    float x;
    float y;
    bool active;
    int type;
};

struct GameState {
    int paddleX;
    int score;
    int starsCollected;
    bool running;
    vector<Brick> bricks;
    vector<Drop> drops;
    vector<Ball> balls;
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

// Remote auth configuration
static bool gUseRemoteAuth = true; // set to true to use server-based auth
static std::string gAuthServerHost = "127.0.0.1";
static int gAuthServerPort = 5000;
static std::string gAuthToken;

static std::wstring Utf8ToWstring(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}

static std::string WinHttpPost(const std::wstring& host, INTERNET_PORT port, const std::wstring& path, const std::string& json, bool secure) {
    HINTERNET hSession = WinHttpOpen(L"BrickGameClient/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return "";
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), port, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
    DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    if (secure) {
        // allow self-signed certs for LAN testing
        DWORD securityFlags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_CERT_CN_INVALID;
        WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &securityFlags, sizeof(securityFlags));
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL send = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)headers.size(), (LPVOID)json.c_str(), (DWORD)json.size(), (DWORD)json.size(), 0);
    if (!send) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
    if (!WinHttpReceiveResponse(hRequest, nullptr)) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }

    std::string response;
    DWORD dwSize = 0;
    do {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &available)) break;
        if (available == 0) break;
        std::vector<char> buffer(available + 1);
        DWORD read = 0;
        if (WinHttpReadData(hRequest, buffer.data(), available, &read) && read > 0) {
            response.append(buffer.data(), read);
        } else break;
    } while (dwSize != 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return response;
}

static bool RemoteRegister(const std::string& username, const std::string& password, std::string& outMessage) {
    std::string json = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
    std::wstring host = Utf8ToWstring(gAuthServerHost);
    std::wstring path = L"/register";
    std::string resp = WinHttpPost(host, (INTERNET_PORT)gAuthServerPort, path, json, true);
    if (resp.empty()) { outMessage = "No response from auth server."; return false; }
    if (resp.find("\"status\":\"ok\"") != std::string::npos) {
        // try extract token
        auto pos = resp.find("\"token\":");
        if (pos != std::string::npos) {
            auto start = resp.find('"', pos + 8);
            if (start != std::string::npos) {
                auto end = resp.find('"', start + 1);
                if (end != std::string::npos) {
                    gAuthToken = resp.substr(start + 1, end - (start + 1));
                }
            }
        }
        outMessage = "Registration successful.";
        return true;
    }
    outMessage = resp;
    return false;
}

static bool RemoteLogin(const std::string& username, const std::string& password, std::string& outMessage) {
    std::string json = "{\"username\":\"" + username + "\",\"password\":\"" + password + "\"}";
    std::wstring host = Utf8ToWstring(gAuthServerHost);
    std::wstring path = L"/login";
    std::string resp = WinHttpPost(host, (INTERNET_PORT)gAuthServerPort, path, json, true);
    if (resp.empty()) { outMessage = "No response from auth server."; return false; }
    if (resp.find("\"status\":\"ok\"") != std::string::npos) {
        auto pos = resp.find("\"token\":");
        if (pos != std::string::npos) {
            auto start = resp.find('"', pos + 8);
            if (start != std::string::npos) {
                auto end = resp.find('"', start + 1);
                if (end != std::string::npos) {
                    gAuthToken = resp.substr(start + 1, end - (start + 1));
                }
            }
        }
        outMessage = "Login successful.";
        return true;
    }
    outMessage = resp;
    return false;
}

GameState g;
ThemeSettings gTheme;
static bool gIsLoggedIn = false;
static bool gIsRegisterMode = false;
static bool gIsTestMode = false;
static int gLoginFocusField = 0; // 0 = username, 1 = password
static std::string gLoginUser;
static std::string gLoginPass;
static std::string gLoginMessage;
static bool gLoginFailed = false;
static std::unordered_map<std::string, std::string> gAccounts;
static RECT gLoginUserRect = {0};
static RECT gLoginPassRect = {0};
static RECT gLoginActionRect = {0};
static RECT gLoginSwitchRect = {0};
static bool gPierceActive = false;
static int gPierceTimer = 0;
static bool gSpeedActive = false;
static int gSpeedTimer = 0;
static bool gPaddleSizeActive = false;
static int gPaddleSizeTimer = 0;
static bool gPaddleLocked = false;
static int gPaddleLockTimer = 0;
static int gPaddleWidth = PADDLE_W;
static const char* ACCOUNT_FILE = "data/accounts.txt";
static int gClientWidth = WINDOW_W;
static int gClientHeight = WINDOW_H;
static int gLayoutRows = 0;
static int gLayoutCols = 0;
Bitmap* gBackground = nullptr;
Bitmap* gStarImage = nullptr;
ULONG_PTR gGdiplusToken = 0;

static Bitmap* CreateDefaultStarBitmap(int diameter) {
    int size = diameter > 0 ? diameter : 1;
    Bitmap* bmp = new Bitmap(size, size, PixelFormat32bppARGB);
    if (!bmp || bmp->GetLastStatus() != Ok) {
        delete bmp;
        return nullptr;
    }

    Graphics graphics(bmp);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.Clear(Color(0, 0, 0, 0));

    float cx = size * 0.5f;
    float cy = size * 0.5f;
    float outerR = size * 0.45f;
    float innerR = size * 0.18f;
    PointF points[10];

    for (int i = 0; i < 10; ++i) {
        float angle = -3.14159265f / 2.0f + i * 3.14159265f / 5.0f;
        float radius = (i % 2 == 0) ? outerR : innerR;
        points[i].X = cx + radius * std::cos(angle);
        points[i].Y = cy + radius * std::sin(angle);
    }

    GraphicsPath path;
    path.AddLines(points, 10);
    path.CloseFigure();

    SolidBrush fillBrush(Color(255, 255, 215, 0));
    Pen outlinePen(Color(255, 255, 235, 120), 2.0f);
    graphics.FillPath(&fillBrush, &path);
    graphics.DrawPath(&outlinePen, &path);

    return bmp;
}

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

static int GetClientWidth() {
    return gClientWidth > 0 ? gClientWidth : WINDOW_W;
}

static int GetClientHeight() {
    return gClientHeight > 0 ? gClientHeight : WINDOW_H;
}

static bool IsSafeThemeConfigPath(const std::string& path) {
    return path == THEME_SETUP_FILE || path == THEME_FILE;
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

static void DrawRoundedRect(HDC hdc, const RECT& rc, COLORREF fillRgb, COLORREF borderRgb, int radius);
static void InitGame(int width, int height);

static void SpawnDrop(int x, int y, int type) {
    Drop drop;
    drop.x = static_cast<float>(x);
    drop.y = static_cast<float>(y);
    drop.active = true;
    drop.type = type;
    g.drops.push_back(drop);
}

static void SpawnStar(int x, int y) {
    SpawnDrop(x, y, DROP_STAR);
}

static void SpawnFunctionDrop(int x, int y) {
    SpawnDrop(x, y, DROP_FUNCTION);
}

static void SpawnBall(int x, int y, int vx, int vy) {
    if (static_cast<int>(g.balls.size()) >= MAX_BALLS) return;
    Ball ball = { x, y, vx, vy, true };
    g.balls.push_back(ball);
}

static void ApplyDropEffect(int type) {
    switch (type) {
        case DROP_PIERCE:
            gPierceActive = true;
            gPierceTimer = PIERCE_DURATION;
            break;
        case DROP_SPEED:
            gSpeedActive = true;
            gSpeedTimer = SPEED_DURATION;
            break;
        case DROP_PADDLE_INCREASE:
            gPaddleSizeActive = true;
            gPaddleSizeTimer = PADDLE_SIZE_DURATION;
            gPaddleWidth = min(PADDLE_W_INCREASED, GetClientWidth());
            if (g.paddleX + gPaddleWidth > GetClientWidth()) {
                g.paddleX = GetClientWidth() - gPaddleWidth;
            }
            break;
        case DROP_FIXED_PADDLE:
            gPaddleLocked = true;
            gPaddleLockTimer = FIXED_PADDLE_DURATION;
            break;
        case DROP_TRIPLE_UP:
            for (auto& ball : g.balls) {
                if (!ball.active) continue;
                ball.vy = -abs(ball.vy) * 3;
                if (ball.vy == 0) ball.vy = -12;
            }
            break;
        case DROP_EXTRA_BALLS: {
            int px = g.paddleX + gPaddleWidth / 2;
            int py = GetClientHeight() - 50 - BALL_R;
            SpawnBall(px, py, -5, -8);
            SpawnBall(px, py, 0, -10);
            SpawnBall(px, py, 5, -8);
            break;
        }
        default:
            break;
    }
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

    std::ifstream in(THEME_SETUP_FILE);
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

    std::ofstream out(THEME_SETUP_FILE, std::ios::trunc);
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

    std::string cacheName = BACKGROUND_CACHE_PREFIX + extStr;
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
    if (!LoadThemeFromFile(THEME_SETUP_FILE)) {
        LoadThemeFromFile(THEME_FILE);
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

static std::string HashPassword(const std::string& password) {
    std::hash<std::string> hasher;
    std::ostringstream oss;
    oss << std::hex << hasher(password);
    return oss.str();
}

static bool LoadAccountDatabase() {
    gAccounts.clear();
    std::ifstream in(ACCOUNT_FILE);
    if (!in.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string user = Trim(line.substr(0, pos));
        std::string pass = Trim(line.substr(pos + 1));
        if (!user.empty() && !pass.empty()) {
            gAccounts[user] = pass;
        }
    }
    return true;
}

static bool SaveAccountDatabase() {
    std::ofstream out(ACCOUNT_FILE, std::ios::trunc);
    if (!out.is_open()) return false;
    for (const auto& entry : gAccounts) {
        out << entry.first << ":" << entry.second << "\n";
    }
    return out.good();
}

static void EnsureAccountDatabaseFile() {
    std::ofstream out(ACCOUNT_FILE, std::ios::app);
}

static bool RegisterAccount(const std::string& username, const std::string& password) {
    if (username.empty() || password.empty()) {
        gLoginMessage = "Username or password cannot be empty.";
        return false;
    }
    if (gUseRemoteAuth) {
        std::string msg;
        bool ok = RemoteRegister(username, password, msg);
        gLoginMessage = msg;
        return ok;
    }
    if (gAccounts.find(username) != gAccounts.end()) {
        gLoginMessage = "Account already exists.";
        return false;
    }
    gAccounts[username] = HashPassword(password);
    if (!SaveAccountDatabase()) {
        gLoginMessage = "Failed to save account.";
        return false;
    }
    gLoginMessage = "Registration successful. Please login.";
    return true;
}

static bool CheckLoginCredentials(const std::string& username, const std::string& password) {
    if (gUseRemoteAuth) {
        std::string msg;
        bool ok = RemoteLogin(username, password, msg);
        if (ok) {
            gLoginMessage.clear();
            return true;
        }
        gLoginMessage = msg;
        return false;
    }
    auto it = gAccounts.find(username);
    return it != gAccounts.end() && it->second == HashPassword(password);
}

static std::string MaskPassword(const std::string& password) {
    return std::string(password.size(), '*');
}

static void SubmitLoginForm(HWND hwnd) {
    if (gIsRegisterMode) {
        gLoginFailed = !RegisterAccount(gLoginUser, gLoginPass);
        if (!gLoginFailed) {
            gIsRegisterMode = false;
            gLoginPass.clear();
        }
    } else {
        if (CheckLoginCredentials(gLoginUser, gLoginPass)) {
            gIsLoggedIn = true;
            gLoginFailed = false;
            gLoginMessage.clear();
            InitGame(GetClientWidth(), GetClientHeight());
        } else {
            gLoginFailed = true;
            gLoginMessage = "Login failed.";
            gLoginPass.clear();
        }
    }
    InvalidateRect(hwnd, nullptr, TRUE);
}

static void DrawLoginScreen(HDC hdc, int clientWidth, int clientHeight) {
    const RECT loginRect = { clientWidth / 2 - 220, clientHeight / 2 - 140, clientWidth / 2 + 220, clientHeight / 2 + 140 };
    DrawRoundedRect(hdc, loginRect, RGB(20, 20, 40), RGB(255, 255, 255), 12);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 255, 255));

    RECT titleRect = loginRect;
    titleRect.bottom = titleRect.top + 30;
    const char* titleText = gIsRegisterMode ? "Codex: Dang ky" : "Codex: Dang nhap";
    DrawTextA(hdc, titleText, -1, &titleRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

    const int fieldLeft = loginRect.left + 20;
    const int fieldRight = loginRect.right - 20;
    int y = titleRect.bottom + 16;

    TextOutA(hdc, fieldLeft, y, "Username:", -1);
    y += 24;
    gLoginUserRect = { fieldLeft, y, fieldRight, y + 30 };
    DrawRoundedRect(hdc, gLoginUserRect, RGB(30, 30, 60), gLoginFocusField == 0 ? RGB(220, 180, 70) : RGB(100, 100, 120), 8);
    TextOutA(hdc, gLoginUserRect.left + 10, gLoginUserRect.top + 6, gLoginUser.c_str(), (int)gLoginUser.size());
    y += 44;

    TextOutA(hdc, fieldLeft, y, "Password:", -1);
    y += 24;
    gLoginPassRect = { fieldLeft, y, fieldRight, y + 30 };
    DrawRoundedRect(hdc, gLoginPassRect, RGB(30, 30, 60), gLoginFocusField == 1 ? RGB(220, 180, 70) : RGB(100, 100, 120), 8);
    std::string maskedPass = MaskPassword(gLoginPass);
    TextOutA(hdc, gLoginPassRect.left + 10, gLoginPassRect.top + 6, maskedPass.c_str(), (int)maskedPass.size());
    y += 44;

    gLoginActionRect = { fieldLeft, y, fieldRight, y + 36 };
    DrawRoundedRect(hdc, gLoginActionRect, RGB(80, 120, 220), RGB(200, 220, 255), 10);
    const char* actionText = gIsRegisterMode ? "Dang ky" : "Dang nhap";
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextA(hdc, actionText, -1, &gLoginActionRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    y += 46;

    gLoginSwitchRect = { fieldLeft, y, fieldRight, y + 28 };
    DrawRoundedRect(hdc, gLoginSwitchRect, RGB(40, 40, 80), RGB(180, 180, 220), 8);
    const char* switchText = gIsRegisterMode ? "Chuyen sang dang nhap" : "Chuyen sang dang ky";
    SetTextColor(hdc, RGB(220, 220, 220));
    DrawTextA(hdc, switchText, -1, &gLoginSwitchRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    y += 34;

    const char* infoText = "Tab = switch field, Enter = submit";
    SetTextColor(hdc, RGB(255, 255, 255));
    TextOutA(hdc, fieldLeft, y, infoText, (int)strlen(infoText));
    y += 22;
    const char* testText = "Press T = Test mode (no login, 100% stars)";
    TextOutA(hdc, fieldLeft, y, testText, (int)strlen(testText));
    y += 24;
    if (!gLoginMessage.empty()) {
        SetTextColor(hdc, gLoginFailed ? RGB(240, 100, 100) : RGB(160, 240, 160));
        TextOutA(hdc, fieldLeft, y, gLoginMessage.c_str(), (int)gLoginMessage.size());
    }
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

void InitGame(int width, int height) {
    gClientWidth = width;
    gClientHeight = height;
    gPaddleWidth = PADDLE_W;
    g.paddleX = (width - gPaddleWidth) / 2;
    gPaddleSizeActive = false;
    gPaddleSizeTimer = 0;
    gPaddleLocked = false;
    gPaddleLockTimer = 0;
    g.score = 0;
    g.starsCollected = 0;
    g.running = true;
    g.bricks.clear();
    g.drops.clear();

    const int frameLeft = 0;
    const int rightWallLeft = width - BRICK_W;
    const int innerAvailW = width - 2 * BRICK_W;
    const int paddleTop = height - 50;
    const int originTop = BRICK_TOP_OFFSET + BRICK_H;
    const int innerAvailH = paddleTop - originTop - BRICK_CLEAR_ABOVE_PADDLE;

    gLayoutCols = (innerAvailW + BRICK_SPACING_X) / (BRICK_W + BRICK_SPACING_X);
    gLayoutRows = (innerAvailH + BRICK_SPACING_Y) / (BRICK_H + BRICK_SPACING_Y);
    if (gLayoutCols < 1) gLayoutCols = 1;
    if (gLayoutRows < 1) gLayoutRows = 1;

    const int innerW = gLayoutCols * BRICK_W + (gLayoutCols - 1) * BRICK_SPACING_X;
    const int innerH = gLayoutRows * BRICK_H + (gLayoutRows - 1) * BRICK_SPACING_Y;
    const int originLeft = BRICK_W + (innerAvailW - innerW) / 2;

    auto addBrick = [&](int left, int top, bool breakable, int type) {
        Brick b;
        b.rc.left = left;
        b.rc.top = top;
        b.rc.right = left + BRICK_W;
        b.rc.bottom = top + BRICK_H;
        b.alive = true;
        b.breakable = breakable;
        b.type = type;
        g.bricks.push_back(b);
        return b;
    };

    const int topWallTop = BRICK_TOP_OFFSET;
    for (int left = 0; left + BRICK_W <= width; left += BRICK_W) {
        addBrick(left, topWallTop, false, BRICK_TYPE_GRAY);
    }

    const int sideWallMaxBottom = paddleTop - BRICK_SPACING_Y;
    for (int i = 0; ; ++i) {
        const int top = originTop + i * BRICK_H;
        if (top + BRICK_H > sideWallMaxBottom) break;
        addBrick(frameLeft, top, false, BRICK_TYPE_GRAY);
        addBrick(rightWallLeft, top, false, BRICK_TYPE_GRAY);
    }

    int breakableBottom = 0;
    const int centerRow = gLayoutRows / 2;
    const int centerCol = gLayoutCols / 2;
    for (int r = 0; r < gLayoutRows; ++r) {
        for (int c = 0; c < gLayoutCols; ++c) {
            const int left = originLeft + c * (BRICK_W + BRICK_SPACING_X);
            const int top = originTop + r * (BRICK_H + BRICK_SPACING_Y);
            bool breakable = true;
            int type;

            if (r == 0) {
                type = BRICK_TYPE_WHITE;
            } else if (r <= 2) {
                type = BRICK_TYPE_RED;
            } else {
                type = ((r + c) % 4 == 0) ? BRICK_TYPE_WHITE : BRICK_TYPE_RED;
            }

            bool grayBorder = false;
            int borderInset = 2;
            if (r >= borderInset && r < gLayoutRows - borderInset &&
                c >= borderInset && c < gLayoutCols - borderInset) {
                if (r == borderInset || r == gLayoutRows - borderInset - 1 ||
                    c == borderInset || c == gLayoutCols - borderInset - 1) {
                    grayBorder = true;
                }
            }
            bool entryHole = (r == gLayoutRows - borderInset - 1 && c == centerCol);
            if (entryHole) {
                continue; // leave a small opening in the gray rectangle for the ball to enter
            }
            if (grayBorder) {
                breakable = false;
                type = BRICK_TYPE_GRAY;
            }

            Brick b = addBrick(left, top, breakable, type);
            if (b.rc.bottom > breakableBottom) breakableBottom = b.rc.bottom;
        }
    }

    g.balls.clear();
    SpawnBall(width / 2, breakableBottom + BALL_R, 5, -5);
    gPierceActive = false;
    gPierceTimer = 0;
    gSpeedActive = false;
    gSpeedTimer = 0;
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
    if (!brick.breakable || brick.type == BRICK_TYPE_GRAY) {
        fillRgb = RGB(180, 180, 180);
        borderRgb = RGB(100, 100, 100);
    } else if (brick.type == BRICK_TYPE_RED) {
        fillRgb = RGB(220, 50, 50);
        borderRgb = RGB(150, 30, 30);
    } else {
        fillRgb = RGB(255, 255, 255);
        borderRgb = RGB(200, 200, 200);
    }
    DrawRoundedRect(hdc, brick.rc, fillRgb, borderRgb, ROUND_RADIUS_BRICK);
}

void DrawPaddle(HDC hdc) {
    const int height = GetClientHeight();
    RECT rc = { g.paddleX, height - 50, g.paddleX + gPaddleWidth, height - 50 + PADDLE_H };
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
    for (const auto& ball : g.balls) {
        if (!ball.active) continue;
        Ellipse(hdc, ball.x - BALL_R, ball.y - BALL_R, ball.x + BALL_R, ball.y + BALL_R);
    }
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

static void ResolveBallCollisions(Ball& ball, int paddleTop) {
    if (ball.x <= BALL_R) {
        BounceBallAxis(ball.x, ball.vx, BALL_R, false);
    } else if (ball.x >= GetClientWidth() - BALL_R) {
        BounceBallAxis(ball.x, ball.vx, GetClientWidth() - BALL_R, true);
    }
    if (ball.y <= BALL_R) {
        BounceBallAxis(ball.y, ball.vy, BALL_R, false);
    }

    for (auto& brick : g.bricks) {
        if (!brick.alive) continue;

        const int nearestX = ClampInt(ball.x, brick.rc.left, brick.rc.right);
        const int nearestY = ClampInt(ball.y, brick.rc.top, brick.rc.bottom);
        const int dx = ball.x - nearestX;
        const int dy = ball.y - nearestY;

        if (dx * dx + dy * dy > BALL_R * BALL_R) continue;

        const bool hitBreakable = brick.breakable;
        const int spawnThreshold = (gLayoutRows * gLayoutCols * 2 + 2) / 3;
        if (hitBreakable) {
            brick.alive = false;
            g.score++;
        }

        if (hitBreakable) {
            if (brick.type == BRICK_TYPE_RED && (gIsTestMode || rand() % 100 < RED_BRICK_STAR_DROP_PERCENT)) {
                SpawnStar((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom);
            } else if (brick.type == BRICK_TYPE_WHITE) {
                int roll = rand() % 100;
                if (roll < WHITE_BRICK_DROP_PIERCE_PERCENT) {
                    SpawnDrop((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom, DROP_PIERCE);
                } else if (roll < WHITE_BRICK_DROP_PIERCE_PERCENT + WHITE_BRICK_DROP_NONE_PERCENT) {
                    // nothing drops
                } else if (roll < WHITE_BRICK_DROP_PIERCE_PERCENT + WHITE_BRICK_DROP_NONE_PERCENT + WHITE_BRICK_DROP_TRIPLE_PERCENT) {
                    SpawnDrop((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom, DROP_TRIPLE_UP);
                } else if (roll < WHITE_BRICK_DROP_PIERCE_PERCENT + WHITE_BRICK_DROP_NONE_PERCENT + WHITE_BRICK_DROP_TRIPLE_PERCENT + WHITE_BRICK_DROP_EXTRA3_PERCENT) {
                    SpawnDrop((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom, DROP_EXTRA_BALLS);
                } else if (roll < WHITE_BRICK_DROP_PIERCE_PERCENT + WHITE_BRICK_DROP_NONE_PERCENT + WHITE_BRICK_DROP_TRIPLE_PERCENT + WHITE_BRICK_DROP_EXTRA3_PERCENT + WHITE_BRICK_DROP_SPEED_PERCENT) {
                    SpawnDrop((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom, DROP_SPEED);
                } else if (roll < WHITE_BRICK_DROP_PIERCE_PERCENT + WHITE_BRICK_DROP_NONE_PERCENT + WHITE_BRICK_DROP_TRIPLE_PERCENT + WHITE_BRICK_DROP_EXTRA3_PERCENT + WHITE_BRICK_DROP_SPEED_PERCENT + WHITE_BRICK_DROP_PADDLE_INCREASE_PERCENT) {
                    SpawnDrop((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom, DROP_PADDLE_INCREASE);
                } else {
                    SpawnDrop((brick.rc.left + brick.rc.right) / 2, brick.rc.bottom, DROP_FIXED_PADDLE);
                }
            }
        }
        bool shouldBounce = !(gPierceActive && brick.type != BRICK_TYPE_GRAY && hitBreakable);
        if (shouldBounce) {
            if (dx == 0 && dy == 0) {
                const int pushLeft = ball.x - brick.rc.left;
                const int pushRight = brick.rc.right - ball.x;
                const int pushTop = ball.y - brick.rc.top;
                const int pushBottom = brick.rc.bottom - ball.y;
                const int minPush = min(min(pushLeft, pushRight), min(pushTop, pushBottom));
                if (minPush == pushLeft) {
                    ball.x = brick.rc.left - BALL_R;
                    ball.vx = -abs(ball.vx);
                } else if (minPush == pushRight) {
                    ball.x = brick.rc.right + BALL_R;
                    ball.vx = abs(ball.vx);
                } else if (minPush == pushTop) {
                    ball.y = brick.rc.top - BALL_R;
                    ball.vy = -abs(ball.vy);
                } else {
                    ball.y = brick.rc.bottom + BALL_R;
                    ball.vy = abs(ball.vy);
                }
            } else if (abs(dx) > abs(dy)) {
                if (dx > 0) {
                    ball.x = brick.rc.right + BALL_R;
                    ball.vx = abs(ball.vx);
                } else {
                    ball.x = brick.rc.left - BALL_R;
                    ball.vx = -abs(ball.vx);
                }
            } else {
                if (dy > 0) {
                    ball.y = brick.rc.bottom + BALL_R;
                    ball.vy = abs(ball.vy);
                } else {
                    ball.y = brick.rc.top - BALL_R;
                    ball.vy = -abs(ball.vy);
                }
            }

            if (abs(ball.vx) < 4) ball.vx = (ball.vx >= 0) ? 4 : -4;
            if (abs(ball.vy) < 4) ball.vy = (ball.vy >= 0) ? 4 : -4;
        }

        if (hitBreakable && g.score >= spawnThreshold) {
            SpawnRandomBrick();
        }
        return;
    }

    const RECT paddle = { g.paddleX, paddleTop, g.paddleX + gPaddleWidth, paddleTop + PADDLE_H };
    if (ball.vy <= 0) return;

    if (ball.y + BALL_R < paddle.top) return;
    if (ball.y - BALL_R > paddle.bottom + BALL_R) return;
    if (ball.x + BALL_R < paddle.left) return;
    if (ball.x - BALL_R > paddle.right) return;

    ball.vy = -abs(ball.vy);
    if (ball.vy > -4) ball.vy = -5;
    ball.y = paddle.top - BALL_R;

    const int paddleCenter = paddle.left + gPaddleWidth / 2;
    const int offset = ball.x - paddleCenter;
    ball.vx = offset * 10 / (gPaddleWidth / 2);
    if (ball.vx > 10) ball.vx = 10;
    if (ball.vx < -10) ball.vx = -10;
    if (ball.vx > -4 && ball.vx < 4) {
        ball.vx = (offset >= 0) ? 4 : -4;
    }
}

void UpdateGame() {
    if (!gIsLoggedIn || !g.running) return;

    if (!gPaddleLocked) {
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) g.paddleX -= 10;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g.paddleX += 10;
    }

    if (g.paddleX < 0) g.paddleX = 0;
    if (g.paddleX + gPaddleWidth > GetClientWidth()) g.paddleX = GetClientWidth() - gPaddleWidth;

    const int paddleTop = GetClientHeight() - 50;
    const int SUBSTEPS = 4;
    int moveFactor = gSpeedActive ? 2 : 1;

    for (int step = 0; step < SUBSTEPS; ++step) {
        for (auto& ball : g.balls) {
            if (!ball.active) continue;
            ball.x += ball.vx * moveFactor / SUBSTEPS;
            ball.y += ball.vy * moveFactor / SUBSTEPS;
            ResolveBallCollisions(ball, paddleTop);
        }
    }

    if (gPierceTimer > 0) {
        gPierceTimer -= 1;
        if (gPierceTimer <= 0) {
            gPierceActive = false;
        }
    }
    if (gSpeedTimer > 0) {
        gSpeedTimer -= 1;
        if (gSpeedTimer <= 0) {
            gSpeedActive = false;
        }
    }
    if (gPaddleSizeTimer > 0) {
        gPaddleSizeTimer -= 1;
        if (gPaddleSizeTimer <= 0) {
            gPaddleSizeActive = false;
            gPaddleWidth = PADDLE_W;
            if (g.paddleX + gPaddleWidth > GetClientWidth()) {
                g.paddleX = GetClientWidth() - gPaddleWidth;
            }
        }
    }
    if (gPaddleLockTimer > 0) {
        gPaddleLockTimer -= 1;
        if (gPaddleLockTimer <= 0) {
            gPaddleLocked = false;
        }
    }

    for (auto& ball : g.balls) {
        if (!ball.active) continue;
        if (ball.vx > 10) ball.vx = 10;
        if (ball.vx < -10) ball.vx = -10;
        if (ball.vy > 10) ball.vy = 10;
        if (ball.vy < -10) ball.vy = -10;
        if (ball.y >= GetClientHeight() - 40) {
            ball.active = false;
        }
    }

    g.balls.erase(std::remove_if(g.balls.begin(), g.balls.end(), [](const Ball& b) { return !b.active; }), g.balls.end());
    if (g.balls.empty()) {
        g.running = false;
    }

    for (auto& drop : g.drops) {
        if (!drop.active) continue;
        drop.y += STAR_FALL_SPEED;

        RECT paddle = { g.paddleX, GetClientHeight() - 50, g.paddleX + gPaddleWidth, GetClientHeight() - 50 + PADDLE_H };
        if (drop.y + STAR_RADIUS >= paddle.top && drop.y - STAR_RADIUS <= paddle.bottom &&
            drop.x >= paddle.left && drop.x <= paddle.right) {
            drop.active = false;
            if (drop.type == DROP_STAR) {
                g.starsCollected++;
                if (g.starsCollected >= STARS_TO_WIN) {
                    g.running = false;
                }
            } else if (drop.type == DROP_FUNCTION) {
                // Placeholder for future function drops
            } else {
                ApplyDropEffect(drop.type);
            }
        }

        if (drop.y - STAR_RADIUS > GetClientHeight()) {
            drop.active = false;
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
            EnsureAccountDatabaseFile();
            if (!LoadAccountDatabase() || gAccounts.empty()) {
                gAccounts.clear();
                gAccounts["codex"] = HashPassword("login");
                SaveAccountDatabase();
            }
            {
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                gClientWidth = clientRect.right - clientRect.left;
                gClientHeight = clientRect.bottom - clientRect.top;
            }
            InitGame(gClientWidth, gClientHeight);
            LoadBackgroundImage(hwnd);
            gStarImage = CreateDefaultStarBitmap(STAR_RADIUS * 2);
            SetTimer(hwnd, 1, 8, nullptr);
            return 0;
        case WM_SIZE:
            gClientWidth = LOWORD(lParam);
            gClientHeight = HIWORD(lParam);
            if (gClientWidth < 1) gClientWidth = WINDOW_W;
            if (gClientHeight < 1) gClientHeight = WINDOW_H;
            InvalidateRect(hwnd, nullptr, TRUE);
            return 0;
        case WM_TIMER:
            UpdateGame();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE: {
            if (!gIsLoggedIn) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (PtInRect(&gLoginUserRect, pt) || PtInRect(&gLoginPassRect, pt) || PtInRect(&gLoginActionRect, pt) || PtInRect(&gLoginSwitchRect, pt)) {
                    SetCursor(LoadCursor(nullptr, IDC_HAND));
                    return 0;
                }
            }
            break;
        }
        case WM_LBUTTONDOWN: {
            if (!gIsLoggedIn) {
                POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                if (PtInRect(&gLoginUserRect, pt)) {
                    gLoginFocusField = 0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (PtInRect(&gLoginPassRect, pt)) {
                    gLoginFocusField = 1;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (PtInRect(&gLoginActionRect, pt)) {
                    SubmitLoginForm(hwnd);
                    return 0;
                }
                if (PtInRect(&gLoginSwitchRect, pt)) {
                    gIsRegisterMode = !gIsRegisterMode;
                    gLoginFailed = false;
                    gLoginMessage.clear();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            break;
        }
        case WM_KEYDOWN:
            if (!gIsLoggedIn) {
                if (wParam == 'T') {
                    gIsTestMode = true;
                    gIsLoggedIn = true;
                    gLoginFailed = false;
                    gLoginMessage.clear();
                    InitGame(GetClientWidth(), GetClientHeight());
                    InvalidateRect(hwnd, nullptr, TRUE);
                    return 0;
                }
                if (wParam == VK_TAB) {
                    gLoginFocusField = 1 - gLoginFocusField;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam == VK_RETURN) {
                    SubmitLoginForm(hwnd);
                    return 0;
                }
                if (wParam == VK_ESCAPE) {
                    DestroyWindow(hwnd);
                    return 0;
                }
                return 0;
            }
            if (wParam == VK_ESCAPE) DestroyWindow(hwnd);
            if (!g.running && wParam == VK_RETURN) {
                InitGame(GetClientWidth(), GetClientHeight());
                InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        case WM_CHAR:
            if (!gIsLoggedIn) {
                if (wParam == 8) {
                    std::string& target = gLoginFocusField == 0 ? gLoginUser : gLoginPass;
                    if (!target.empty()) target.pop_back();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (wParam >= 32 && wParam < 127) {
                    std::string& target = gLoginFocusField == 0 ? gLoginUser : gLoginPass;
                    if (target.size() < 32) target.push_back(static_cast<char>(wParam));
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            int clientWidth = clientRect.right - clientRect.left;
            int clientHeight = clientRect.bottom - clientRect.top;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, clientWidth, clientHeight);
            HGDIOBJ oldBM = SelectObject(memDC, memBM);

            if (gBackground != nullptr) {
                Graphics graphics(memDC);
                graphics.DrawImage(gBackground, 0, 0, clientWidth, clientHeight);
            } else {
                HBRUSH bg = CreateSolidBrush(RGB(gTheme.backgroundR, gTheme.backgroundG, gTheme.backgroundB));
                RECT fullRect = { 0, 0, clientWidth, clientHeight };
                FillRect(memDC, &fullRect, bg);
                DeleteObject(bg);
            }

            if (gIsLoggedIn) {
                for (const auto& brick : g.bricks) {
                    if (brick.alive) DrawBrick(memDC, brick);
                }
                DrawPaddle(memDC);
                DrawBall(memDC);
            } else {
                DrawLoginScreen(memDC, clientWidth, clientHeight);
            }

            HBRUSH starBrush = CreateSolidBrush(RGB(255, 215, 0));
            HBRUSH functionBrush = CreateSolidBrush(RGB(120, 180, 255));
            HBRUSH pierceBrush = CreateSolidBrush(RGB(180, 255, 180));
            HBRUSH tripleBrush = CreateSolidBrush(RGB(255, 180, 80));
            HBRUSH extraBrush = CreateSolidBrush(RGB(180, 120, 255));
            HBRUSH speedBrush = CreateSolidBrush(RGB(255, 120, 120));
            HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, starBrush);
            Graphics dropGraphics(memDC);
            for (const auto& drop : g.drops) {
                if (!drop.active) continue;

                if (drop.type == DROP_STAR && gStarImage != nullptr) {
                    dropGraphics.DrawImage(gStarImage,
                                           static_cast<REAL>(static_cast<int>(drop.x) - STAR_RADIUS),
                                           static_cast<REAL>(static_cast<int>(drop.y) - STAR_RADIUS),
                                           static_cast<REAL>(STAR_RADIUS * 2),
                                           static_cast<REAL>(STAR_RADIUS * 2));
                } else {
                    HBRUSH currentBrush = starBrush;
                    if (drop.type == DROP_FUNCTION) currentBrush = functionBrush;
                    else if (drop.type == DROP_PIERCE) currentBrush = pierceBrush;
                    else if (drop.type == DROP_TRIPLE_UP) currentBrush = tripleBrush;
                    else if (drop.type == DROP_EXTRA_BALLS) currentBrush = extraBrush;
                    else if (drop.type == DROP_SPEED) currentBrush = speedBrush;
                    SelectObject(memDC, currentBrush);
                    Ellipse(memDC,
                            static_cast<int>(drop.x) - STAR_RADIUS,
                            static_cast<int>(drop.y) - STAR_RADIUS,
                            static_cast<int>(drop.x) + STAR_RADIUS,
                            static_cast<int>(drop.y) + STAR_RADIUS);
                }
            }
            SelectObject(memDC, oldBrush);
            DeleteObject(starBrush);
            DeleteObject(functionBrush);
            DeleteObject(pierceBrush);
            DeleteObject(tripleBrush);
            DeleteObject(extraBrush);
            DeleteObject(speedBrush);

            DrawHud(memDC);

            if (!g.running) {
                SetBkMode(memDC, TRANSPARENT);
                SetTextColor(memDC, RGB(255, 255, 255));
                RECT msgRc = { GetClientWidth() / 2 - 160, GetClientHeight() / 2 - 20, GetClientWidth() / 2 + 160, GetClientHeight() / 2 + 20 };
                const char* msg = g.starsCollected >= STARS_TO_WIN ? "Chuc mung! Ban da thang!" : "Game over!";
                DrawTextA(memDC, msg, -1, &msgRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            BitBlt(hdc, 0, 0, clientWidth, clientHeight, memDC, 0, 0, SRCCOPY);
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
            delete gStarImage;
            gStarImage = nullptr;
            if (gGdiplusToken != 0) {
                Gdiplus::GdiplusShutdown(gGdiplusToken);
                gGdiplusToken = 0;
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int) {
    srand(static_cast<unsigned>(time(nullptr)));
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gGdiplusToken, &gdiplusStartupInput, nullptr);

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
