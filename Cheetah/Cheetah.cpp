/*
 * Copyright [2025] [codeboxqc]
 *
 * This file is part of  cheetah .
 *
 * cheetah is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * cheetah is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with [Program Name].  If not, see <https://www.gnu.org/licenses/>.
 */



#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

//☣️☠️🦠☣️☠️🦠☣️☠️🦠☣️☠️🦠☣️☠️🦠☣️☠️🦠☣️☠️🦠☣️☠️🦠☣️☠️🦠☣️☠️

// Add these macros at the top of your file, after includes if not already defined
#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif


#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <objidl.h>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <dwmapi.h>
#include <algorithm>
#include <gdiplus.h>


#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"  // Ensure stb_image.h is in your project/include path

#include "resource.h"

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "User32.lib")


#define WM_USER_PROGRESS (WM_USER + 100)



struct ProgressData {
    int64_t currentFrame = 0;
    int64_t totalFrames = 0;
    double fps = 0.0;
    int64_t elapsedMs = 0;
    int64_t fileSizeBytes = 0;
    bool isEncoding = false;
};

static ProgressData g_progress;
static std::mutex g_progressMutex;




extern "C" void set_output_format(int format);
extern "C" void set_progress_callback(void(*callback)(int64_t, int64_t, double, int64_t, int64_t));
/////////////////////////////////////////////////////////
extern "C" int transcode_main(int argc, char** argv);
extern "C" void set_encoding_option(int option);
static int g_selectedEncodingOption = 2; // Default: H.264 High Quality

struct EncodingButton {
    RECT rect;
    const wchar_t* label;
    int option;
};

// Define 4 encoding option buttons
EncodingButton g_encodingButtons[] = {
    { {373, 310, 34+376, 340}, L"H.265\nArch.", 1 },
    { {373, 360, 34+376, 390}, L"H.264\nHigh", 2 },
    { {373, 410, 34+376, 440}, L"H.264\nBalance", 3 },
    { {373, 460, 34+376, 490}, L"H.264\nSmall", 4 }
};
/////////////////////////////////////////////////////////



// Design size of your interface.png
const int DESIGN_X = 450+30;
const int DESIGN_Y = 650;

#define TIMER_ID 1
#define TIMER_BOB 2
#define TIMER_MINI 3





// Global resources
struct ImageData {
    unsigned char* data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
};

ImageData g_backgroundImg{};
HBITMAP   g_hBackgroundBitmap = nullptr;

std::wstring g_status = L"Drop a video file here to convert to MP4...";
std::mutex   g_statusMutex;
std::atomic<bool> g_windowAlive{ true };

int glowIntensity = 0;
bool glowIncreasing = true;


using namespace Gdiplus;
ULONG_PTR gdiplusToken;
Image* pImage = nullptr;
Image* intface = nullptr;
Image* b1 = nullptr;
Image* b2 = nullptr;

Image* led1 = nullptr;
Image* led2 = nullptr;

Image* mled1 = nullptr;
Image* mled2 = nullptr;
bool miniled = 0;


int mx = 0, my = 0;
int ix1 = 0, iy1 = 0;
int statusled = 0;
////////////////////////////////////

/////////////////////////format output selection


// STEP 1: Add format enum and variable after encoding options (around line 60)
typedef enum {
    FORMAT_MP4 = 1,   // Universal - works everywhere
    FORMAT_MKV = 2,   // High quality archival
    FORMAT_WEBP = 3,  // Web optimized
    FORMAT_MOV = 4    // Apple/editing workflow
} OutputFormat;

static OutputFormat g_selectedFormat = FORMAT_MP4; // Default to MP4

// STEP 2: Define format selection buttons (add after encoding buttons around line 75)
struct FormatButton {
    RECT rect;
    const wchar_t* label;
    OutputFormat format;
    const wchar_t* extension;
};

// Position these buttons wherever you want in your UI
FormatButton g_formatButtons[] = {
    //{ {373, 510, 407, 540}, L"MP4",  FORMAT_MP4,  L".mp4" },
   // { {410, 510, 444, 540}, L"MKV",  FORMAT_MKV,  L".mkv" },
   // { {373, 543, 407, 573}, L"WebP", FORMAT_WEBP, L".webp" },
   // { {410, 543, 444, 573}, L"MOV",  FORMAT_MOV,  L".mov" }

    { {175, 570, 207 , 600 }, L"MP4",  FORMAT_MP4,  L".mp4" },
    { {220, 568, 252 , 598}, L"MKV",  FORMAT_MKV,  L".mkv" },
    { {265, 564, 295 , 594}, L"WebP", FORMAT_WEBP, L".webp" },
    { {308, 560, 340,  590}, L"MOV",  FORMAT_MOV,  L".mov" }
};

// STEP 3: Add function to get file extension based on format
const wchar_t* GetFormatExtension(OutputFormat format) {
    switch (format) {
    case FORMAT_MP4:  return L".mp4";
    case FORMAT_MKV:  return L".mkv";
    case FORMAT_WEBP: return L".webp";
    case FORMAT_MOV:  return L".mov";
    default:          return L".mp4";
    }
}

// STEP 4: Add function to get format name for FFmpeg
const char* GetFFmpegFormatName(OutputFormat format) {
    switch (format) {
    case FORMAT_MP4:  return "mp4";
    case FORMAT_MKV:  return "matroska";
    case FORMAT_WEBP: return "webp";
    case FORMAT_MOV:  return "mov";
    default:          return "mp4";
    }
}

// STEP 5: Draw function for format buttons (add near DrawEncodingButton around line 650)
void DrawFormatButton(HDC hdc, FormatButton& btn, bool selected)
{
    HPEN pen = CreatePen(PS_SOLID, 2, selected ? RGB(0, 255, 0) : RGB(100, 100, 100));
    HBRUSH brush = CreateSolidBrush(selected ? RGB(50, 150, 50) : RGB(40, 40, 45));

    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);

    RoundRect(hdc, btn.rect.left, btn.rect.top, btn.rect.right, btn.rect.bottom, 8, 8);

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    LOGFONTW lf = {};
    lf.lfHeight = 12;
    lf.lfWeight = selected ? FW_BOLD : FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Arial");

    HFONT hFont = CreateFontIndirectW(&lf);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    DrawTextW(hdc, btn.label, -1, &btn.rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeletePen(pen);
    DeleteObject(brush);
}



// ====================== Helper Functions ======================

extern "C" void progress_callback(int64_t current, int64_t total,
    double fps, int64_t elapsed, int64_t size)
{
    std::lock_guard<std::mutex> lock(g_progressMutex);
    g_progress.currentFrame = current;
    g_progress.totalFrames = total;
    g_progress.fps = fps;
    g_progress.elapsedMs = elapsed;
    g_progress.fileSizeBytes = size;
    g_progress.isEncoding = true;
}

std::wstring FormatTime(int64_t milliseconds) {
    int64_t seconds = milliseconds / 1000;
    int64_t minutes = seconds / 60;
    int64_t hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    wchar_t buffer[32];
    swprintf_s(buffer, L"%02lld:%02lld:%02lld", hours, minutes, seconds);
    return buffer;
}

std::wstring FormatFileSize(int64_t bytes) {
    if (bytes < 1024) return std::to_wstring(bytes) + L" B";
    if (bytes < 1024 * 1024) return std::to_wstring(bytes / 1024) + L" KB";
    if (bytes < 1024 * 1024 * 1024) {
        double mb = bytes / (1024.0 * 1024.0);
        wchar_t buffer[32];
        swprintf_s(buffer, L"%.1f MB", mb);
        return buffer;
    }
    double gb = bytes / (1024.0 * 1024.0 * 1024.0);
    wchar_t buffer[32];
    swprintf_s(buffer, L"%.2f GB", gb);
    return buffer;
}


ImageData LoadPNG(const char* filename)
{
    ImageData image{};
    image.data = stbi_load(filename, &image.width, &image.height, &image.channels, 4);
    if (image.data) image.channels = 4;
    return image;
}

auto fmt_time = [](int64_t s) {
    wchar_t b[32];
    swprintf(b, 32, L"%02lld:%02lld:%02lld",
        s / 3600, (s / 60) % 60, s % 60);
    return std::wstring(b);
    };



void FreeImage(ImageData& image)
{
    if (image.data) {
        stbi_image_free(image.data);
        image.data = nullptr;
    }
}


// Add this helper function
// Replace the current LoadImageFromResource with this version that returns ImageData
ImageData LoadImageFromResource(UINT id, LPCWSTR type = L"PNG")
{
    ImageData result = {};

    HRSRC hResource = FindResource(nullptr, MAKEINTRESOURCE(id), type);
    if (!hResource) return result;

    HGLOBAL hGlobal = LoadResource(nullptr, hResource);
    if (!hGlobal) return result;

    void* pResourceData = LockResource(hGlobal);
    DWORD resourceSize = SizeofResource(nullptr, hResource);
    if (!pResourceData || !resourceSize) return result;

    // Use stbi_load_from_memory to load PNG from resource data
    result.data = stbi_load_from_memory(
        (const unsigned char*)pResourceData,
        resourceSize,
        &result.width,
        &result.height,
        &result.channels,
        4  // Force 4 channels (RGBA)
    );

    if (result.data) result.channels = 4;

    return result;
}

void ConvertRGBAToBGRA(unsigned char* data, int width, int height)
{
    for (int i = 0; i < width * height; ++i) {
        unsigned char* p = data + i * 4;
        std::swap(p[0], p[2]); // R <-> B
    }
}

HBITMAP CreateBitmapFromData(HDC hdc, unsigned char* data, int width, int height)
{
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (hBitmap && pBits && data) {
        memcpy(pBits, data, width * height * 4);
    }
    return hBitmap;
}

void SetStatus(const std::wstring& newStatus)
{
    std::lock_guard<std::mutex> lock(g_statusMutex);
    g_status = newStatus;
}

std::wstring GetStatus()
{
    std::lock_guard<std::mutex> lock(g_statusMutex);
    return g_status;
}

static int TranscodeWithExceptionHandling(int argc, char** argv, DWORD* pExceptionCode)
{
    __try {
        return transcode_main(argc, argv);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        *pExceptionCode = GetExceptionCode();
        return -1;
    }
}







void DrawProgressMeter(HDC memDC, int x, int y, int width, int height) {
    std::lock_guard<std::mutex> lock(g_progressMutex);

    if (!g_progress.isEncoding) return;

    // Background
    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 35));
    HPEN borderPen = CreatePen(PS_SOLID, 2, RGB(80, 80, 90));
    HPEN oldPen = (HPEN)SelectObject(memDC, borderPen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, bgBrush);

    RoundRect(memDC, x, y, x + width, y + height, 10, 10);

    SelectObject(memDC, oldPen);
    SelectObject(memDC, oldBrush);
    DeleteObject(borderPen);
    DeleteObject(bgBrush);

    // Progress bar
    int barX = x + 10;
    int barY = y + 10;
    int barWidth = width - 20;
    int barHeight = 25;

    // Background bar
    HBRUSH barBgBrush = CreateSolidBrush(RGB(50, 50, 55));
    RECT barBgRect = { barX, barY, barX + barWidth, barY + barHeight };
    FillRect(memDC, &barBgRect, barBgBrush);
    DeleteObject(barBgBrush);

    // Progress fill
    if (g_progress.totalFrames > 0) {
        double progress = (double)g_progress.currentFrame / g_progress.totalFrames;
        int fillWidth = (int)(barWidth * progress);

        // Gradient effect
        HBRUSH progressBrush = CreateSolidBrush(RGB(0, 180, 255));
        RECT progressRect = { barX, barY, barX + fillWidth, barY + barHeight };
        FillRect(memDC, &progressRect, progressBrush);
        DeleteObject(progressBrush);

        // Percentage text on bar
        wchar_t percentText[16];
        swprintf_s(percentText, L"%d%%", (int)(progress * 100));

        SetTextColor(memDC, RGB(255, 255, 255));
        SetBkMode(memDC, TRANSPARENT);

        LOGFONTW lf = {};
        lf.lfHeight = 16;
        lf.lfWeight = FW_BOLD;
        lf.lfQuality = CLEARTYPE_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Arial");
        HFONT hFont = CreateFontIndirectW(&lf);
        HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);

        RECT textRect = { barX, barY, barX + barWidth, barY + barHeight };
        DrawTextW(memDC, percentText, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        SelectObject(memDC, hOldFont);
        DeleteObject(hFont);
    }

    // Stats area
    int statsY = barY + barHeight + 15;

    SetTextColor(memDC, RGB(200, 200, 200));
    SetBkMode(memDC, TRANSPARENT);

    LOGFONTW statsFont = {};
    statsFont.lfHeight = 14;
    statsFont.lfWeight = FW_NORMAL;
    statsFont.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(statsFont.lfFaceName, L"Consolas");
    HFONT hStatsFont = CreateFontIndirectW(&statsFont);
    HFONT hOldStatsFont = (HFONT)SelectObject(memDC, hStatsFont);

    // Frame counter
    wchar_t frameText[64];
    swprintf_s(frameText, L"Frame: %lld / %lld", g_progress.currentFrame, g_progress.totalFrames);
    TextOutW(memDC, x + 15, statsY, frameText, wcslen(frameText));

    // FPS
    wchar_t fpsText[32];
    swprintf_s(fpsText, L"FPS: %.1f", g_progress.fps);
    TextOutW(memDC, x + 15, statsY + 20, fpsText, wcslen(fpsText));

    // Elapsed time
    std::wstring elapsedStr = L"Time: " + FormatTime(g_progress.elapsedMs);
    TextOutW(memDC, x + 15, statsY + 40, elapsedStr.c_str(), elapsedStr.length());

    // Estimated remaining time
    if (g_progress.fps > 0 && g_progress.totalFrames > g_progress.currentFrame) {
        int64_t remainingFrames = g_progress.totalFrames - g_progress.currentFrame;
        int64_t remainingMs = (int64_t)((remainingFrames / g_progress.fps) * 1000);
        std::wstring remainingStr = L"ETA: " + FormatTime(remainingMs);
        TextOutW(memDC, x + 200, statsY, remainingStr.c_str(), remainingStr.length());
    }

    // Output file size
    std::wstring sizeStr = L"Size: " + FormatFileSize(g_progress.fileSizeBytes);
    TextOutW(memDC, x + 200, statsY + 20, sizeStr.c_str(), sizeStr.length());

    SelectObject(memDC, hOldStatsFont);
    DeleteObject(hStatsFont);
}






// Draw with rotation around center (default)
void DrawRotatedImage(Graphics& graphics, Image* pImage, float x, float y, float angle, bool rotateAroundCenter = true)
{
    if (!pImage) return;

    // Save current transform
    Matrix oldMatrix;
    graphics.GetTransform(&oldMatrix);

    // Set rotation point
    float pivotX, pivotY;
    if (rotateAroundCenter) {
        pivotX = x + pImage->GetWidth() / 2.0f;
        pivotY = y + pImage->GetHeight() / 2.0f;
    }
    else {
        pivotX = x;
        pivotY = y;
    }

    // Create transform: translate to pivot, rotate, translate back
    Matrix transform;
    transform.Translate(pivotX, pivotY);
    transform.Rotate(angle);
    transform.Translate(-pivotX, -pivotY);

    // Apply transform
    graphics.SetTransform(&transform);

    // Draw image at original position
    graphics.DrawImage(pImage, x, y, pImage->GetWidth(), pImage->GetHeight());

    // Restore original transform
    graphics.SetTransform(&oldMatrix);
}

/////////////////////////////button///////////////////////////



void LoadPNGImage(HWND hwnd, UINT id, Image** img) {
    HRSRC res = FindResource(nullptr, MAKEINTRESOURCE(id), L"PNG");
    if (!res) return;
    HGLOBAL mem = LoadResource(nullptr, res);
    if (!mem) return;
    void* data = LockResource(mem);
    DWORD size = SizeofResource(nullptr, res);

    HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hGlobal) return;
    void* pBuffer = GlobalLock(hGlobal);
    if (pBuffer) {
        memcpy(pBuffer, data, size);
        GlobalUnlock(hGlobal);
        IStream* stream = nullptr;
        if (SUCCEEDED(CreateStreamOnHGlobal(hGlobal, TRUE, &stream))) {
            if (*img) delete* img;
            *img = new Image(stream);
            stream->Release();
        }
    }
}

void InitializeGDIPlus() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);
}
void ShutdownGDIPlus() {
    GdiplusShutdown(gdiplusToken);
}

void DrawInvertedImage(Graphics& graphics, Image* pImage, float x, float y, int opt) {
    if (!pImage) return;

    // Define a color matrix for inverting colors
    ColorMatrix  colorMatrix;

    if (opt == 0) {
        colorMatrix = {
              5.0f,  0.0f,  0.0f,  0.0f,  0.0f, // Red channel boosted
              0.0f,  0.0f,  0.0f,  0.0f,  0.0f, // Green channel neutralized
              0.0f,  0.0f,  0.0f,  0.0f,  0.0f, // Blue channel neutralized
              0.0f,  0.0f,  0.0f,  1.0f,  0.0f, // Alpha (transparency) unchanged
              0.0f,  0.0f,  0.0f,  0.0f,  1.0f  // No bias
        };
    }
    if (opt == 1) {

        colorMatrix = {
           0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  // Red channel neutralized
           0.0f,  0.0f,  0.0f,  0.0f,  0.0f,  // Green channel neutralized
           5.0f,  0.0f,  0.0f,  0.0f,  0.0f,  // Blue channel boosted (50% of original)
           0.0f,  0.0f,  0.0f,  1.0f,  0.0f,  // Alpha (transparency) unchanged
           0.0f,  0.0f,  0.0f,  0.0f,  1.0f   // No bias
        };
    }


    if (opt == 2) {

        colorMatrix = {
       0.3f,  0.3f,  0.3f,  0.0f,  0.0f,  // Red channel converted to grayscale
       0.3f,  0.3f,  0.3f,  0.0f,  0.0f,  // Green channel converted to grayscale
       0.3f,  0.3f,  0.3f,  0.0f,  0.0f,  // Blue channel converted to grayscale
       0.0f,  0.0f,  0.0f,  1.0f,  0.0f,  // Alpha (transparency) unchanged
       0.0f,  0.0f,  0.0f,  0.0f,  1.0f   // No bias
        };

    }


    // Create image attributes and set the color matrix
    ImageAttributes imgAttr;
    imgAttr.SetColorMatrix(&colorMatrix, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

    // Get the image dimensions
    Rect rect(static_cast<int>(x), static_cast<int>(y), pImage->GetWidth(), pImage->GetHeight());

    // Draw the image with the inverted color effect
    graphics.DrawImage(pImage, rect, 0, 0, pImage->GetWidth(), pImage->GetHeight(),
        UnitPixel, &imgAttr);
}
///////////////////////////////////////////////////////////////////////////











void DrawEncodingButton(HDC hdc, EncodingButton& btn, bool selected)
{
    // Create pen and brush
    HPEN pen = CreatePen(PS_SOLID, 2, selected ? RGB(254, 244, 100) : RGB(255,  80,  80));
    HBRUSH brush = CreateSolidBrush(selected ? RGB(233, 157, 53) : RGB(146, 83, 82));

    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, brush);

    // Draw rounded rectangle
    RoundRect(hdc, btn.rect.left, btn.rect.top, btn.rect.right, btn.rect.bottom, 10, 10);

    // Draw text
    SetTextColor(hdc, selected ? RGB(255, 255, 255) : RGB(20, 20, 20));
    SetBkMode(hdc, TRANSPARENT);

    LOGFONTW lf = {};
    lf.lfHeight = 14;
    lf.lfWeight = selected ? FW_BOLD : FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(lf.lfFaceName, L"Arial");

    HFONT hFont = CreateFontIndirectW(&lf);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    DrawTextW(hdc, btn.label, -1, &btn.rect, DT_CENTER | DT_VCENTER | DT_WORDBREAK);


    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);

    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBrush);
    DeletePen(pen);
    DeleteObject(brush);
}















// ====================== Window Procedure ======================
//IDB_PNG1  cancel
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT buttonRect = { 404, 10, 447, 53 };
    RECT ROAD1 = { 6, 33, 6+234, 33+238};
    RECT ROAD2 = { 236, 46, 236+233, 46+223 };

    RECT LED1 = { 89,  531, 91+14,  531+13 };
    RECT LED2 = { 120, 530, 121+14, 530+13 };

    RECT MLED1 = { 78,  428, 78 + 8,  428 + 8 };
    RECT MLED2 = { 92, 427, 92 + 8, 427 + 8 };






    switch (msg)
    {



    case WM_TIMER: {


        if (wParam == TIMER_MINI) {
            InvalidateRect(hwnd, &MLED1, TRUE);
            InvalidateRect(hwnd, &MLED2, TRUE);
            miniled = !miniled;

        }
        if (wParam == TIMER_BOB) {
            RECT a1 = { ROAD1.left - 2, ROAD1.top - 2, ROAD1.right + 2, ROAD1.bottom + 2 };
            InvalidateRect(hwnd, &a1, TRUE);
            RECT a2 = { ROAD2.left - 2, ROAD2.top - 2, ROAD2.right + 2, ROAD2.bottom + 2 };
            InvalidateRect(hwnd, &a2, TRUE);

            InvalidateRect(hwnd, &LED1, TRUE);
            InvalidateRect(hwnd, &LED2, TRUE);
            statusled = !statusled;




        }
        else if (wParam == TIMER_ID) {






            // Update glow effect
            if (glowIncreasing) {
                glowIntensity += 2;
                if (glowIntensity >= 60) glowIncreasing = false;
            }
            else {
                glowIntensity -= 2;
                if (glowIntensity <= 20) glowIncreasing = true;
            }

            // Invalidate text area for glow effect
            RECT textRect = { 191, 314, 340, 513 };
            InvalidateRect(hwnd, &textRect, TRUE);

            // FIXED: Always check and update progress if encoding
            if (g_progress.isEncoding) {
                RECT progressRect = { 10, 570, 470, 640 };
                InvalidateRect(hwnd, &progressRect, TRUE);
            }
        }
        return 0;
    }






    case WM_USER_PROGRESS:
    {
        RECT r = { 30, 560, 420, 625 };
        InvalidateRect(hwnd, &r, FALSE);
        return 0;
    }




    case WM_CREATE:

        DragAcceptFiles(hwnd, TRUE);

        SetTimer(hwnd, TIMER_ID, 33, nullptr);
        SetTimer(hwnd, TIMER_BOB, 5, nullptr);
        SetTimer(hwnd, TIMER_MINI, 200, nullptr);
        InitializeGDIPlus();
        LoadPNGImage(hwnd, IDB_PNG1, &pImage);
        LoadPNGImage(hwnd, IDB_PNG2, &intface);
        LoadPNGImage(hwnd, IDB_PNG3, &b1);
        LoadPNGImage(hwnd, IDB_PNG4, &b2);
        LoadPNGImage(hwnd, IDB_PNG5, &led1);
        LoadPNGImage(hwnd, IDB_PNG6, &led2);

        LoadPNGImage(hwnd, IDB_PNG7, &mled1);
        LoadPNGImage(hwnd, IDB_PNG8, &mled2);






        // g_backgroundImg = LoadPNG("web.png");  // Fixed filename
        g_backgroundImg = LoadImageFromResource(IDB_PNG2, L"PNG");


        if (g_backgroundImg.data)
        {
            ConvertRGBAToBGRA(g_backgroundImg.data, g_backgroundImg.width, g_backgroundImg.height);

            HDC hdc = GetDC(hwnd);
            g_hBackgroundBitmap = CreateBitmapFromData(hdc, g_backgroundImg.data,
                g_backgroundImg.width, g_backgroundImg.height);
            ReleaseDC(hwnd, hdc);

            if (!g_hBackgroundBitmap)
                SetStatus(L"Failed to create  interface ");
        }
        else
        {
            SetStatus(L"Failed to load interface.png ");
        }
        return 0;


 case WM_DROPFILES:
 {
     HDROP hDrop = (HDROP)wParam;
     wchar_t szFile[MAX_PATH] = {};

     if (DragQueryFileW(hDrop, 0, szFile, MAX_PATH) == 0) {
         DragFinish(hDrop);
         return 0;
     }

     DragFinish(hDrop);

     std::wstring input = szFile;

     // SIMPLE WebP check without std::transform
     std::wstring input_lower = input;
     // Manually convert to lowercase
     for (wchar_t& c : input_lower) {
         c = towlower(c);
     }

     // Check for other image formats that might cause issues
     const wchar_t* image_extensions[] = {
         L".png", L".jpg", L".jpeg", L".bmp", L".gif",
         L".tiff", L".tif", L".ico", L".svg", NULL
     };

     for (int i = 0; image_extensions[i]; i++) {
         if (input_lower.find(image_extensions[i]) != std::wstring::npos) {
             SetStatus(L"⚠ This is an IMAGE file\n\n"
                 L"Cheetah converts VIDEO files\n"
                 L"Please drop a video file instead");
             InvalidateRect(hwnd, nullptr, TRUE);
             return 0;
         }
     }

     // Check for unsupported video formats
     const wchar_t* unsupported_extensions[] = {
         L".avi", L".flv", L".swf", L".rm", L".rmvb",
         L".vob", L".ts", L".m2ts", NULL
     };

     for (int i = 0; unsupported_extensions[i]; i++) {
         if (input_lower.find(unsupported_extensions[i]) != std::wstring::npos) {
             SetStatus(L" Format may not work well\n\n"
                 L"Best supported:\n"
                 L"• MP4, MKV, MOV, WebP\n\n"
                 L"Try converting with another tool first");
             InvalidateRect(hwnd, nullptr, TRUE);
             // Don't return - let it try anyway
         }
     }

     size_t dotPos = input.find_last_of(L'.');

    // UPDATED: Use selected format extension
    std::wstring baseFilename = (dotPos != std::wstring::npos) ?
        input.substr(0, dotPos) : input;

    std::wstring output = baseFilename + L"_converted" + GetFormatExtension(g_selectedFormat);

    SetStatus(L"Starting conversion...\n" + input);
    InvalidateRect(hwnd, nullptr, TRUE);

    auto input_heap = std::make_unique<std::wstring>(input);
    auto output_heap = std::make_unique<std::wstring>(output);

    std::thread([hwnd,
        input_ptr = input_heap.release(),
        output_ptr = output_heap.release()]() mutable
        {
            std::unique_ptr<std::wstring> input_heap(input_ptr);
            std::unique_ptr<std::wstring> output_heap(output_ptr);

            {
                std::lock_guard<std::mutex> lock(g_progressMutex);
                g_progress = ProgressData();
                g_progress.isEncoding = true;
            }

            set_progress_callback(progress_callback);

            auto to_utf8 = [](const std::wstring& wstr) -> std::string {
                if (wstr.empty()) return {};
                int len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::string str(len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], len, nullptr, nullptr);
                return str;
                };

            std::string input_utf8 = to_utf8(*input_heap);
            std::string output_utf8 = to_utf8(*output_heap);
            std::string prog = "transcode";

            char* argv[3] = { &prog[0], &input_utf8[0], &output_utf8[0] };

            const wchar_t* optionDesc[] = {
                L"Unknown",
                L"H.265 Archive", L"H.264 High Quality",
                L"H.264 Balanced", L"H.264 Small"
            };

            const wchar_t* formatDesc[] = {
                L"Unknown",
                L"MP4 (Universal)", L"MKV (High Quality)",
                L"WebP (Web)", L"MOV (Apple/Editing)"
            };

            std::wstring statusMsg = L"Encoding: ";
            statusMsg += optionDesc[g_selectedEncodingOption];
            statusMsg += L"\nFormat: ";
            statusMsg += formatDesc[g_selectedFormat];
            statusMsg += L"\n\n";
            statusMsg += *input_heap;

            SetStatus(statusMsg);
            if (g_windowAlive) PostMessage(hwnd, WM_USER + 1, 0, 0);

            set_encoding_option(g_selectedEncodingOption);
            set_output_format(g_selectedFormat);

            DWORD exceptionCode = 0;
            int result = TranscodeWithExceptionHandling(3, argv, &exceptionCode);

            if (exceptionCode != 0) {
                std::wstring name;
                switch (exceptionCode) {
                case 0xC0000005: name = L"ACCESS VIOLATION"; break;
                case 0xC0000094: name = L"DIVISION BY ZERO"; break;
                case 0xC000001D: name = L"ILLEGAL INSTRUCTION"; break;
                case 0xC00000FD: name = L"STACK OVERFLOW"; break;
                case 0xC0000409: name = L"BUFFER OVERRUN"; break;
                default: name = L"UNKNOWN EXCEPTION"; break;
                }
                SetStatus(L"✗ CRASHED!\n" + name + L"\nCode: 0x" +
                    std::to_wstring(exceptionCode) + L"\n\nCheck cheetah.log");
            }
            else if (result == 0) {
                HANDLE hFile = CreateFileW(output_heap->c_str(), GENERIC_READ, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (hFile != INVALID_HANDLE_VALUE) {
                    LARGE_INTEGER size{};
                    GetFileSizeEx(hFile, &size);
                    CloseHandle(hFile);

                    if (size.QuadPart < 10000)
                        SetStatus(L"Small file (" + std::to_wstring(size.QuadPart) + L" bytes)");
                    else
                        SetStatus(L"Success!\n\n" + *output_heap +
                            L"\nSize: " + std::to_wstring(size.QuadPart / 1024) + L" KB");
                }
                else {
                    SetStatus(L"Output file not created");
                }
            }
            else {
                SetStatus(L"Failed (code " + std::to_wstring(result) + L")");
            }

            {
                std::lock_guard<std::mutex> lock(g_progressMutex);
                g_progress.isEncoding = false;
            }

            if (g_windowAlive) PostMessage(hwnd, WM_USER + 1, 0, 0);

        }).detach();

    return 0;
}











    case WM_USER + 1:
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;

    case WM_NCHITTEST: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hwnd, &pt);



        if (PtInRect(&buttonRect, pt)) { //exit
            return HTCLIENT;  // Allow clicks on button (no drag here)
        }

        for (int i = 0; i < 4; i++) { //4 button
            if (PtInRect(&g_encodingButtons[i].rect, pt)) {
                return HTCLIENT;  // Allow clicks on encoding buttons
            }
        }

        //  Allow format button clicks
        for (int i = 0; i < 4; i++) {
            if (PtInRect(&g_formatButtons[i].rect, pt)) {
                return HTCLIENT;
            }
        }


        RECT clientRect = { 0, 0, DESIGN_X, DESIGN_Y };
        if (PtInRect(&clientRect, pt)) {
            return HTCAPTION;  // Drag the window from non-button areas
        }

        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // Create memory DC for double buffering
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, DESIGN_X, DESIGN_Y);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        // Draw everything to memory DC first
        if (g_hBackgroundBitmap)
        {
            HDC bgDC = CreateCompatibleDC(memDC);
            HBITMAP old = (HBITMAP)SelectObject(bgDC, g_hBackgroundBitmap);
            //BitBlt(memDC, 0, 0, g_backgroundImg.width, g_backgroundImg.height,
            //    bgDC, 0, 0, SRCCOPY);


            int offset = 2;
            BitBlt(memDC, -offset, -offset,
                g_backgroundImg.width - offset  ,
                g_backgroundImg.height - offset  ,
                bgDC, 0, 0, SRCCOPY);


            SelectObject(bgDC, old);
            DeleteDC(bgDC);



            Graphics graphics(memDC);


            graphics.DrawImage(pImage,int (buttonRect.left + ix1), int (buttonRect.top + iy1), pImage->GetWidth(), pImage->GetHeight());



            // Draw text to memory DC
            std::wstring text = GetStatus();
            int glowR = min(222, 125 + glowIntensity *2);
            int glowG = min(222, 100 + glowIntensity *2 );
            int glowB = min(222, 25 + glowIntensity *2);

            LOGFONTW lf = {};
            lf.lfHeight = 16;
            lf.lfEscapement = -30;
            lf.lfOrientation = -30;
            lf.lfWeight = FW_NORMAL;
            lf.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf.lfFaceName, L"Arial");

            HFONT hFont = CreateFontIndirectW(&lf);
            HFONT hOldFont = (HFONT)SelectObject(memDC, hFont);

            SetTextColor(memDC, RGB(glowR, glowG, glowB));
            SetBkMode(memDC, TRANSPARENT);
            RECT textRect = { 191, 314, 340, 513 };//text
            DrawTextW(memDC, text.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_WORDBREAK);

////////////////////////////////////////

           // SetTextColor(memDC, RGB(238, 88, 38));
           // SetBkMode(memDC, TRANSPARENT);

            LOGFONTW lf2 = {};
            lf2.lfHeight = 16;
            lf2.lfWeight = FW_BOLD;
            lf2.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(lf2.lfFaceName, L"Arial");

            HFONT hFont2 = CreateFontIndirectW(&lf2);
            HFONT hOldFont2 = (HFONT)SelectObject(memDC, hFont2);

            // Draw encoding option buttons
            for (int i = 0; i < 4; i++) {
                bool isSelected = (g_encodingButtons[i].option == g_selectedEncodingOption);
                DrawEncodingButton(memDC, g_encodingButtons[i], isSelected);
            }

            //RECT labelRect = { 10, 525, 450, 545 };
            //DrawTextW(memDC, L"Encoding Quality:", -1, &labelRect, DT_LEFT | DT_VCENTER);
            SetTextColor(memDC, RGB(0, 0, 0));
            SetBkMode(memDC, TRANSPARENT);
            SelectObject(memDC, hOldFont2);
            DeleteObject(hFont2);
///////////////////////////////////////////////////////////////



            if (miniled == 0) {

                graphics.DrawImage(mled1, (int)MLED1.left, (int)MLED1.top, mled1->GetWidth(), mled1->GetHeight());

                graphics.DrawImage(mled2, (int)MLED2.left, (int)MLED2.top, mled2->GetWidth(), mled2->GetHeight());


            }
            else {

                graphics.DrawImage(mled2, (int)MLED1.left, (int)MLED1.top, mled1->GetWidth(), mled1->GetHeight());

                graphics.DrawImage(mled1, (int)MLED2.left, (int)MLED2.top, mled2->GetWidth(), mled2->GetHeight());

            }



            if (statusled == 0) {

               graphics.DrawImage(led1,(int) LED1.left , (int)LED1.top , led1->GetWidth(), led1->GetHeight());

               graphics.DrawImage(led2, (int)LED2.left, (int)LED2.top, led2->GetWidth(), led2->GetHeight());


            }
            else {

                graphics.DrawImage(led2, (int)LED1.left, (int)LED1.top, led1->GetWidth(), led1->GetHeight());

                graphics.DrawImage(led1, (int)LED2.left, (int)LED2.top, led2->GetWidth(), led2->GetHeight());

            }



            ///////////////////////////////////////////
            // Draw rotating images
            static float rotationAngle1 = 0.0f;
            static float rotationAngle2 = 0.0f;

            // Increment rotation angles (you can do this in WM_TIMER)
            rotationAngle1 += 1.0f;  // 1 degree per frame
            rotationAngle2 += 2.0f;  // 2 degrees per frame

            if (rotationAngle1 >= 360.0f) rotationAngle1 -= 360.0f;
            if (rotationAngle2 >= 360.0f) rotationAngle2 -= 360.0f;


            if (b1) {
                DrawRotatedImage(graphics, b1, ROAD1.left, ROAD1.top, rotationAngle1);
            }

            // Draw b2 at position (200, 200) with rotation
            if (b2) {
                DrawRotatedImage(graphics, b2, ROAD2.left, ROAD2.top, rotationAngle2);
            }

            /////////////////////////////format output button
            // Draw format selection buttons
            for (int i = 0; i < 4; i++) {
                bool isSelected = (g_formatButtons[i].format == g_selectedFormat);
                DrawFormatButton(memDC, g_formatButtons[i], isSelected);
            }

            // Optional: Add label above format buttons
           // SetTextColor(memDC, RGB(200, 200, 200));
           // RECT formatLabelRect = { 370, 495, 450, 510 };
           // DrawTextW(memDC, L"Output Format:", -1, &formatLabelRect, DT_LEFT | DT_VCENTER);


            //////////////////progress bar
            if (g_progress.isEncoding) {
                DrawProgressMeter(memDC, 1, 570, 460, 70);
            }


            ///////////////////////////////////////////////////////////////////


            SetTextColor(memDC, RGB(0, 0, 0));
            SetBkMode(memDC, TRANSPARENT);
            SelectObject(memDC, hOldFont);
            DeleteObject(hFont);
        }

        // Copy everything at once to screen
        BitBlt(hdc, 0, 0, DESIGN_X, DESIGN_Y, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        return 0;
    }




    case WM_MOUSEMOVE: {
        mx = LOWORD(lParam);
        my = HIWORD(lParam);


        POINT pt = { mx, my };
        for (int i = 0; i < 4; i++) {
            if (PtInRect(&g_encodingButtons[i].rect, pt)) {
                // Could show tooltip here if desired
                SetCursor(LoadCursor(nullptr, IDC_HAND));
                return 0;
            }
        }

        SetCursor(LoadCursor(nullptr, IDC_ARROW));



        // Check if mouse is over the current button position
        if (mx >= buttonRect.left && mx <= buttonRect.right &&
            my >= buttonRect.top && my <= buttonRect.bottom) {
            // Move the button randomly
            ix1 = (rand() % 2 == 0) ? 2 : -2;
            iy1 = (rand() % 2 == 0) ? 2 : -2;


             RECT  butt = { buttonRect.left - 2,buttonRect.top - 2,buttonRect.right + 2,buttonRect.bottom + 2 };
             InvalidateRect(hwnd, &butt, TRUE);

        }
        else {


        }


        return 0;
    }





    case WM_LBUTTONDOWN:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        // Check close button (existing code)
        if (PtInRect(&buttonRect, pt)) {
            DestroyWindow(hwnd);
            return 0;
        }

        //forma button
        for (int i = 0; i < 4; i++) {
            if (PtInRect(&g_formatButtons[i].rect, pt)) {
                g_selectedFormat = g_formatButtons[i].format;

                const wchar_t* formatNames[] = {
                    L"Unknown",
                    L"MP4 (Universal - works everywhere)",
                    L"MKV (High quality archival)",
                    L"WebP (Web optimized)",
                    L"MOV (Apple/editing workflow)"
                };

                std::wstring statusMsg = L"Output Format: ";
                statusMsg += formatNames[g_selectedFormat];
                statusMsg += L"\n\nDrop a video file to convert...";
                SetStatus(statusMsg);

                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
        }

        // NEW: Check encoding option buttons
        for (int i = 0; i < 4; i++) {
            if (PtInRect(&g_encodingButtons[i].rect, pt)) {
                g_selectedEncodingOption = g_encodingButtons[i].option;

                // Update status text
                const wchar_t* optionNames[] = {
                       L"Unknown",
                       L"H.265 Archive - CRF 20-22 (Best compression)",
                       L"H.264 High - CRF 18-20 (Best quality)",
                       L"H.264 Balanced - CRF 22-24 (Recommended)",
                       L"H.264 Small - CRF 27-30 (Smallest files)"
                };

                std::wstring newStatus = L"Selected: ";
                newStatus += optionNames[g_selectedEncodingOption];
                newStatus += L"\n\nDrop a video file to convert...";
                SetStatus(newStatus);

                // Redraw to show selection
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
        }

        return 0;
    }





    case WM_DESTROY:
        g_windowAlive = false;

        FreeImage(g_backgroundImg);
        if (g_hBackgroundBitmap) {
            DeleteObject(g_hBackgroundBitmap);
            g_hBackgroundBitmap = nullptr;
        }
        delete pImage;
        delete intface;
        delete b1;
        delete b2;
        delete led1;
        delete led2;
        delete mled1;
        delete mled2;
        KillTimer(hwnd, TIMER_ID);
        KillTimer(hwnd, TIMER_BOB);
        KillTimer(hwnd, TIMER_MINI);
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// ====================== Entry Point ======================

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)
{
    WNDCLASSW wc{};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"Cheetah";

    RegisterClassW(&wc);



    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - DESIGN_X) / 2;
    int posY = (screenH - DESIGN_Y) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        L"Cheetah", L"Cheetah",
        WS_POPUP,
        posX, posY, DESIGN_X, DESIGN_Y,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd) return -1;

    // Make black (0,0,0) transparent
    SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);



    // === THIS BLOCK TO REMOVE THE THIN BORDER ===
    if (hwnd) {
        // Make the entire window act as client area (no frame)
        MARGINS margins = { -1, -1, -1, -1 };  // -1 means extend to full window
        DwmExtendFrameIntoClientArea(hwnd, &margins);


        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        // Force redraw
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    }


    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;

}
