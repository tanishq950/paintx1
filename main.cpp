#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "Comdlg32.lib")

enum Tool {
    TOOL_PENCIL,
    TOOL_LINE,
    TOOL_RECTANGLE,
    TOOL_ELLIPSE,
    TOOL_TEXT,
    TOOL_ERASER
};

const int ID_TOOL_PENCIL = 1001;
const int ID_TOOL_LINE = 1002;
const int ID_TOOL_RECTANGLE = 1003;
const int ID_TOOL_ELLIPSE = 1004;
const int ID_TOOL_TEXT = 1005;
const int ID_TOOL_ERASER = 1006;
const int ID_COLOR_BASE = 1100;
const int ID_SAVE = 1201;
const int ID_CLEAR = 1202;
const int ID_UNDO = 1203;
const int ID_REDO = 1204;
const int ID_SIZE = 1205;

struct ColorItem {
    COLORREF color;
    const wchar_t* name;
};

const ColorItem COLORS[] = {
    {RGB(0, 0, 0), L"Black"}, {RGB(255, 255, 255), L"White"},
    {RGB(220, 20, 60), L"Red"}, {RGB(255, 140, 0), L"Orange"},
    {RGB(255, 215, 0), L"Yellow"}, {RGB(34, 139, 34), L"Green"},
    {RGB(30, 144, 255), L"Blue"}, {RGB(75, 0, 130), L"Purple"},
    {RGB(255, 105, 180), L"Pink"}, {RGB(139, 69, 19), L"Brown"},
    {RGB(128, 128, 128), L"Gray"}, {RGB(0, 206, 209), L"Cyan"}
};

HWND g_window = nullptr;
HWND g_canvas = nullptr;
HBITMAP g_bitmap = nullptr;
HBITMAP g_preview = nullptr;
int g_canvasWidth = 900;
int g_canvasHeight = 560;
Tool g_tool = TOOL_PENCIL;
COLORREF g_color = RGB(0, 0, 0);
int g_brushSize = 4;
bool g_drawing = false;
POINT g_start = {};
POINT g_last = {};
std::vector<HBITMAP> g_undo;
std::vector<HBITMAP> g_redo;

HBITMAP CopyBitmap(HBITMAP source) {
    if (!source) return nullptr;
    BITMAP info = {};
    GetObject(source, sizeof(info), &info);
    HDC screen = GetDC(nullptr);
    HDC sourceDC = CreateCompatibleDC(screen);
    HDC targetDC = CreateCompatibleDC(screen);
    HBITMAP copy = CreateCompatibleBitmap(screen, info.bmWidth, info.bmHeight);
    HGDIOBJ oldSource = SelectObject(sourceDC, source);
    HGDIOBJ oldTarget = SelectObject(targetDC, copy);
    BitBlt(targetDC, 0, 0, info.bmWidth, info.bmHeight, sourceDC, 0, 0, SRCCOPY);
    SelectObject(sourceDC, oldSource);
    SelectObject(targetDC, oldTarget);
    DeleteDC(sourceDC);
    DeleteDC(targetDC);
    ReleaseDC(nullptr, screen);
    return copy;
}

void DeleteBitmapVector(std::vector<HBITMAP>& items) {
    for (HBITMAP item : items) DeleteObject(item);
    items.clear();
}

void PushUndoState() {
    if (!g_bitmap) return;
    g_undo.push_back(CopyBitmap(g_bitmap));
    DeleteBitmapVector(g_redo);
    if (g_undo.size() > 30) {
        DeleteObject(g_undo.front());
        g_undo.erase(g_undo.begin());
    }
}

void ReplaceBitmap(HBITMAP replacement) {
    if (!replacement) return;
    if (g_bitmap) DeleteObject(g_bitmap);
    g_bitmap = replacement;
    InvalidateRect(g_canvas, nullptr, FALSE);
}

void Undo() {
    if (g_undo.empty()) return;
    g_redo.push_back(CopyBitmap(g_bitmap));
    HBITMAP previous = g_undo.back();
    g_undo.pop_back();
    ReplaceBitmap(previous);
}

void Redo() {
    if (g_redo.empty()) return;
    g_undo.push_back(CopyBitmap(g_bitmap));
    HBITMAP next = g_redo.back();
    g_redo.pop_back();
    ReplaceBitmap(next);
}

void FillCanvas(HBITMAP bitmap, COLORREF color) {
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(dc, bitmap);
    RECT area = {0, 0, g_canvasWidth, g_canvasHeight};
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &area, brush);
    DeleteObject(brush);
    SelectObject(dc, old);
    DeleteDC(dc);
}

void DrawSegment(HBITMAP bitmap, POINT a, POINT b, COLORREF color, int width) {
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(dc, bitmap);
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, a.x, a.y, nullptr);
    LineTo(dc, b.x, b.y);
    SelectObject(dc, oldPen);
    SelectObject(dc, old);
    DeleteObject(pen);
    DeleteDC(dc);
}

void DrawShape(HBITMAP bitmap, Tool tool, POINT a, POINT b, COLORREF color, int width) {
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(dc, bitmap);
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    int left = (std::min)(a.x, b.x);
    int right = (std::max)(a.x, b.x);
    int top = (std::min)(a.y, b.y);
    int bottom = (std::max)(a.y, b.y);
    if (tool == TOOL_RECTANGLE) Rectangle(dc, left, top, right, bottom);
    if (tool == TOOL_ELLIPSE) Ellipse(dc, left, top, right, bottom);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    SelectObject(dc, old);
    DeleteObject(pen);
    DeleteDC(dc);
}

void DrawTextOnCanvas(const std::wstring& text, POINT position) {
    if (text.empty()) return;
    PushUndoState();
    HDC dc = CreateCompatibleDC(nullptr);
    HGDIOBJ old = SelectObject(dc, g_bitmap);
    HFONT font = CreateFontW(
        (std::max)(14, g_brushSize * 5), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    HGDIOBJ oldFont = SelectObject(dc, font);
    SetTextColor(dc, g_color);
    SetBkMode(dc, TRANSPARENT);
    TextOutW(dc, position.x, position.y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(dc, oldFont);
    SelectObject(dc, old);
    DeleteObject(font);
    DeleteDC(dc);
    InvalidateRect(g_canvas, nullptr, FALSE);
}

bool GetTextInput(std::wstring& result) {
    wchar_t buffer[256] = {};
    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME, L"EDIT", L"Enter text (press Enter to place it)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | ES_LEFT | ES_AUTOHSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, 360, 28, g_window, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!dialog) return false;
    ShowWindow(dialog, SW_SHOW);
    SetFocus(dialog);
    MSG message = {};
    while (IsWindow(dialog) && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN) {
            GetWindowTextW(dialog, buffer, 256);
            DestroyWindow(dialog);
            result = buffer;
            return !result.empty();
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
            DestroyWindow(dialog);
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return false;
}

void SaveBitmap() {
    OPENFILENAMEW dialog = {};
    wchar_t fileName[MAX_PATH] = L"my-paint-picture.bmp";
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = g_window;
    dialog.lpstrFilter = L"BMP Image (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0";
    dialog.lpstrFile = fileName;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    dialog.lpstrDefExt = L"bmp";
    if (!GetSaveFileNameW(&dialog)) return;

    BITMAP bitmap = {};
    GetObject(g_bitmap, sizeof(bitmap), &bitmap);
    BITMAPFILEHEADER fileHeader = {};
    BITMAPINFOHEADER infoHeader = {};
    infoHeader.biSize = sizeof(infoHeader);
    infoHeader.biWidth = bitmap.bmWidth;
    infoHeader.biHeight = bitmap.bmHeight;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = bitmap.bmWidth * bitmap.bmHeight * 4;
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(infoHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + infoHeader.biSizeImage;
    std::vector<unsigned char> pixels(infoHeader.biSizeImage);
    HDC dc = GetDC(nullptr);
    BITMAPINFO info = {};
    info.bmiHeader = infoHeader;
    GetDIBits(dc, g_bitmap, 0, bitmap.bmHeight, pixels.data(), &info, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    HANDLE file = CreateFileW(fileName, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, &fileHeader, sizeof(fileHeader), &written, nullptr);
    WriteFile(file, &infoHeader, sizeof(infoHeader), &written, nullptr);
    WriteFile(file, pixels.data(), static_cast<DWORD>(pixels.size()), &written, nullptr);
    CloseHandle(file);
    MessageBoxW(g_window, L"Your drawing was saved as a BMP image.", L"Save complete", MB_OK | MB_ICONINFORMATION);
}

void CreateCanvas(int width, int height) {
    HDC screen = GetDC(nullptr);
    g_bitmap = CreateCompatibleBitmap(screen, width, height);
    ReleaseDC(nullptr, screen);
    g_canvasWidth = width;
    g_canvasHeight = height;
    FillCanvas(g_bitmap, RGB(255, 255, 255));
}

void SetTool(Tool tool) {
    g_tool = tool;
    const wchar_t* title = L"Computer Graphics Paint";
    switch (tool) {
        case TOOL_PENCIL: title = L"Computer Graphics Paint - Pencil"; break;
        case TOOL_LINE: title = L"Computer Graphics Paint - Line"; break;
        case TOOL_RECTANGLE: title = L"Computer Graphics Paint - Rectangle"; break;
        case TOOL_ELLIPSE: title = L"Computer Graphics Paint - Ellipse"; break;
        case TOOL_TEXT: title = L"Computer Graphics Paint - Text"; break;
        case TOOL_ERASER: title = L"Computer Graphics Paint - Eraser"; break;
    }
    SetWindowTextW(g_window, title);
}

void AddButton(HWND parent, int id, const wchar_t* label, int x, int width) {
    CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                  x, 10, width, 30, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                  GetModuleHandleW(nullptr), nullptr);
}

void AddColorButton(HWND parent, int index, int x, int y) {
    HWND button = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                x, y, 26, 26, parent,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COLOR_BASE + index)),
                                GetModuleHandleW(nullptr), nullptr);
    (void)button;
}

LRESULT CALLBACK CanvasProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_LBUTTONDOWN: {
            g_drawing = true;
            g_start = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            g_last = g_start;
            if (g_tool == TOOL_TEXT) {
                std::wstring text;
                if (GetTextInput(text)) DrawTextOnCanvas(text, g_start);
                g_drawing = false;
            } else {
                PushUndoState();
            }
            return 0;
        }
        case WM_MOUSEMOVE:
            if (g_drawing && (wParam & MK_LBUTTON)) {
                POINT current = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                if (g_tool == TOOL_PENCIL || g_tool == TOOL_ERASER) {
                    COLORREF color = g_tool == TOOL_ERASER ? RGB(255, 255, 255) : g_color;
                    DrawSegment(g_bitmap, g_last, current, color, g_tool == TOOL_ERASER ? g_brushSize * 3 : g_brushSize);
                    g_last = current;
                    InvalidateRect(window, nullptr, FALSE);
                } else {
                    if (g_preview) DeleteObject(g_preview);
                    g_preview = CopyBitmap(g_bitmap);
                    if (g_tool == TOOL_LINE) DrawSegment(g_preview, g_start, current, g_color, g_brushSize);
                    else DrawShape(g_preview, g_tool, g_start, current, g_color, g_brushSize);
                    InvalidateRect(window, nullptr, FALSE);
                }
            }
            return 0;
        case WM_LBUTTONUP: {
            if (!g_drawing) return 0;
            POINT current = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (g_tool == TOOL_LINE) DrawSegment(g_bitmap, g_start, current, g_color, g_brushSize);
            else if (g_tool == TOOL_RECTANGLE || g_tool == TOOL_ELLIPSE)
                DrawShape(g_bitmap, g_tool, g_start, current, g_color, g_brushSize);
            if (g_preview) {
                DeleteObject(g_preview);
                g_preview = nullptr;
            }
            g_drawing = false;
            InvalidateRect(window, nullptr, FALSE);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT paint = {};
            HDC dc = BeginPaint(window, &paint);
            HBITMAP image = g_preview ? g_preview : g_bitmap;
            if (image) {
                HDC memory = CreateCompatibleDC(dc);
                HGDIOBJ old = SelectObject(memory, image);
                BitBlt(dc, 0, 0, g_canvasWidth, g_canvasHeight, memory, 0, 0, SRCCOPY);
                SelectObject(memory, old);
                DeleteDC(memory);
            }
            EndPaint(window, &paint);
            return 0;
        }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK MainProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            g_window = window;
            AddButton(window, ID_TOOL_PENCIL, L"Pencil", 10, 70);
            AddButton(window, ID_TOOL_LINE, L"Line", 85, 60);
            AddButton(window, ID_TOOL_RECTANGLE, L"Rectangle", 150, 85);
            AddButton(window, ID_TOOL_ELLIPSE, L"Ellipse", 240, 70);
            AddButton(window, ID_TOOL_TEXT, L"Text", 315, 60);
            AddButton(window, ID_TOOL_ERASER, L"Eraser", 380, 70);
            AddButton(window, ID_UNDO, L"Undo", 460, 60);
            AddButton(window, ID_REDO, L"Redo", 525, 60);
            AddButton(window, ID_SAVE, L"Save BMP", 590, 80);
            AddButton(window, ID_CLEAR, L"Clear", 675, 60);
            CreateWindowW(L"STATIC", L"Size:", WS_CHILD | WS_VISIBLE,
                          750, 17, 35, 20, window, nullptr, GetModuleHandleW(nullptr), nullptr);
            CreateWindowW(L"EDIT", L"4", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_NUMBER,
                          785, 12, 35, 24, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SIZE)),
                          GetModuleHandleW(nullptr), nullptr);
            for (int i = 0; i < static_cast<int>(sizeof(COLORS) / sizeof(COLORS[0])); ++i)
                AddColorButton(window, i, 10 + i * 31, 48);
            CreateCanvas(g_canvasWidth, g_canvasHeight);
            return 0;
        }
        case WM_SIZE:
            if (g_canvas) MoveWindow(g_canvas, 10, 85, LOWORD(lParam) - 20, HIWORD(lParam) - 95, TRUE);
            return 0;
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id >= ID_COLOR_BASE && id < ID_COLOR_BASE + static_cast<int>(sizeof(COLORS) / sizeof(COLORS[0]))) {
                g_color = COLORS[id - ID_COLOR_BASE].color;
                return 0;
            }
            if (id == ID_TOOL_PENCIL) SetTool(TOOL_PENCIL);
            else if (id == ID_TOOL_LINE) SetTool(TOOL_LINE);
            else if (id == ID_TOOL_RECTANGLE) SetTool(TOOL_RECTANGLE);
            else if (id == ID_TOOL_ELLIPSE) SetTool(TOOL_ELLIPSE);
            else if (id == ID_TOOL_TEXT) SetTool(TOOL_TEXT);
            else if (id == ID_TOOL_ERASER) SetTool(TOOL_ERASER);
            else if (id == ID_UNDO) Undo();
            else if (id == ID_REDO) Redo();
            else if (id == ID_SAVE) SaveBitmap();
            else if (id == ID_CLEAR) {
                PushUndoState();
                FillCanvas(g_bitmap, RGB(255, 255, 255));
                InvalidateRect(g_canvas, nullptr, FALSE);
            } else if (id == ID_SIZE && HIWORD(wParam) == EN_CHANGE) {
                wchar_t value[16] = {};
                GetWindowTextW(reinterpret_cast<HWND>(lParam), value, 16);
                int size = _wtoi(value);
                if (size >= 1 && size <= 50) g_brushSize = size;
            }
            return 0;
        }
        case WM_DRAWITEM: {
            DRAWITEMSTRUCT* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            if (item->CtlID >= ID_COLOR_BASE && item->CtlID < ID_COLOR_BASE + 12) {
                COLORREF color = COLORS[item->CtlID - ID_COLOR_BASE].color;
                HBRUSH brush = CreateSolidBrush(color);
                FillRect(item->hDC, &item->rcItem, brush);
                DeleteObject(brush);
                FrameRect(item->hDC, &item->rcItem,
                          static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                return TRUE;
            }
            break;
        }
        case WM_DESTROY:
            DeleteObject(g_bitmap);
            DeleteObject(g_preview);
            DeleteBitmapVector(g_undo);
            DeleteBitmapVector(g_redo);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    WNDCLASSW canvasClass = {};
    canvasClass.hInstance = instance;
    canvasClass.lpfnWndProc = CanvasProc;
    canvasClass.lpszClassName = L"PaintCanvasClass";
    canvasClass.hCursor = LoadCursor(nullptr, IDC_CROSS);
    canvasClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    RegisterClassW(&canvasClass);

    WNDCLASSW mainClass = {};
    mainClass.hInstance = instance;
    mainClass.lpfnWndProc = MainProc;
    mainClass.lpszClassName = L"ComputerGraphicsPaintClass";
    mainClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    mainClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(LTGRAY_BRUSH));
    RegisterClassW(&mainClass);

    HWND window = CreateWindowW(
        L"ComputerGraphicsPaintClass", L"Computer Graphics Paint",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 940, 720,
        nullptr, nullptr, instance, nullptr);
    if (!window) return 0;

    g_canvas = CreateWindowW(
        L"PaintCanvasClass", L"", WS_CHILD | WS_VISIBLE | WS_BORDER,
        10, 85, g_canvasWidth, g_canvasHeight, window, nullptr, instance, nullptr);

    ShowWindow(window, showCommand);
    UpdateWindow(window);
    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
