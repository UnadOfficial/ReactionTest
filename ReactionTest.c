#include <windows.h>
#include <mmsystem.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdlib.h>

typedef enum {
    STATE_WAIT_START,
    STATE_WAIT_GREEN,
    STATE_WAIT_REACT,
    STATE_SHOW_RESULT,
    STATE_TOO_EARLY
} State;

static State g_state = STATE_WAIT_START;
static LARGE_INTEGER g_freq;
static LARGE_INTEGER g_green_time;
static LARGE_INTEGER g_click_time;
static double g_reaction_ms = 0.0;
static MMRESULT g_mm_timer = 0;
static HANDLE g_green_event = NULL;
static HWND g_hwnd = NULL;

static IDirect3D9* g_d3d = NULL;
static IDirect3DDevice9* g_dev = NULL;
static IDirect3DSurface9* g_backbuf = NULL;
static D3DPRESENT_PARAMETERS g_pp = {0};

static void RenderGreen(void) {
    g_dev->lpVtbl->Clear(g_dev, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 128, 0), 1.0f, 0);
    g_dev->lpVtbl->Present(g_dev, NULL, NULL, NULL, NULL);
    QueryPerformanceCounter(&g_green_time);
}

static void Render(void) {
    if (!g_dev) return;

    if (g_state == STATE_WAIT_REACT) {
        RenderGreen();
        return;
    }

    D3DCOLOR color;
    const char* text = NULL;
    BOOL is_result = FALSE;
    BOOL is_green = FALSE;

    switch (g_state) {
    case STATE_WAIT_START:
        color = D3DCOLOR_XRGB(255, 255, 255);
        text = "Click to start";
        break;
    case STATE_WAIT_GREEN:
        color = D3DCOLOR_XRGB(255, 255, 255);
        break;
    case STATE_WAIT_REACT:
        color = D3DCOLOR_XRGB(0, 128, 0);
        is_green = TRUE;
        break;
    case STATE_SHOW_RESULT:
        color = D3DCOLOR_XRGB(0, 128, 0);
        is_result = TRUE;
        is_green = TRUE;
        break;
    case STATE_TOO_EARLY:
        color = D3DCOLOR_XRGB(255, 255, 255);
        text = "Too early! Click to retry";
        break;
    default:
        color = D3DCOLOR_XRGB(255, 255, 255);
        break;
    }

    g_dev->lpVtbl->Clear(g_dev, 0, NULL, D3DCLEAR_TARGET, color, 1.0f, 0);

    if (text || is_result) {
        HDC hdc;
        if (SUCCEEDED(g_backbuf->lpVtbl->GetDC(g_backbuf, &hdc))) {
            RECT rc;
            GetClientRect(g_hwnd, &rc);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, is_green ? RGB(255, 255, 255) : RGB(0, 0, 0));

            if (is_result) {
                HFONT f1 = CreateFontA(64, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Arial");
                HFONT f2 = CreateFontA(32, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Arial");
                HFONT of = SelectObject(hdc, f1);
                char buf[128];
                snprintf(buf, sizeof(buf), "%.3f ms", g_reaction_ms);
                RECT line1 = rc;
                line1.bottom = rc.bottom / 2;
                DrawTextA(hdc, buf, -1, &line1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, f2);
                RECT line2 = rc;
                line2.top = rc.bottom / 2;
                DrawTextA(hdc, "Click to restart", -1, &line2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, of);
                DeleteObject(f1);
                DeleteObject(f2);
            } else {
                HFONT f = CreateFontA(48, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, "Arial");
                HFONT of = SelectObject(hdc, f);
                DrawTextA(hdc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdc, of);
                DeleteObject(f);
            }

            g_backbuf->lpVtbl->ReleaseDC(g_backbuf, hdc);
        }
    }

    HRESULT hr = g_dev->lpVtbl->Present(g_dev, NULL, NULL, NULL, NULL);
    if (hr == D3DERR_DEVICELOST) {
        if (g_dev->lpVtbl->TestCooperativeLevel(g_dev) == D3DERR_DEVICENOTRESET) {
            if (g_backbuf) { g_backbuf->lpVtbl->Release(g_backbuf); g_backbuf = NULL; }
            if (SUCCEEDED(g_dev->lpVtbl->Reset(g_dev, &g_pp))) {
                g_dev->lpVtbl->GetBackBuffer(g_dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &g_backbuf);
            }
        }
    }
}

static void CALLBACK MMTimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2) {
    SetEvent(g_green_event);
}

static void ScheduleGreen(void) {
    LARGE_INTEGER seed;
    QueryPerformanceCounter(&seed);
    srand((unsigned int)(seed.LowPart ^ seed.HighPart));
    int delay_ms = 1000 + (rand() % 4000);
    ResetEvent(g_green_event);
    g_mm_timer = timeSetEvent(delay_ms, 1, MMTimerCallback, 0, TIME_CALLBACK_FUNCTION | TIME_ONESHOT);
}

static void CancelGreen(void) {
    if (g_mm_timer) {
        timeKillEvent(g_mm_timer);
        g_mm_timer = 0;
    }
    ResetEvent(g_green_event);
}

static void BoostPriority(void) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
}

static void RestorePriority(void) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        switch (g_state) {
        case STATE_WAIT_START:
            g_state = STATE_WAIT_GREEN;
            BoostPriority();
            ScheduleGreen();
            Render();
            break;
        case STATE_WAIT_GREEN:
            CancelGreen();
            g_state = STATE_TOO_EARLY;
            RestorePriority();
            Render();
            break;
        case STATE_WAIT_REACT:
            QueryPerformanceCounter(&g_click_time);
            g_reaction_ms = (double)(g_click_time.QuadPart - g_green_time.QuadPart) * 1000.0 / (double)g_freq.QuadPart;
            g_state = STATE_SHOW_RESULT;
            RestorePriority();
            Render();
            break;
        case STATE_SHOW_RESULT:
        case STATE_TOO_EARLY:
            g_state = STATE_WAIT_START;
            Render();
            break;
        default:
            break;
        }
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        break;
    case WM_ACTIVATEAPP:
        if (wParam && g_dev) {
            HRESULT hr = g_dev->lpVtbl->TestCooperativeLevel(g_dev);
            if (hr == D3DERR_DEVICENOTRESET) {
                if (g_backbuf) { g_backbuf->lpVtbl->Release(g_backbuf); g_backbuf = NULL; }
                if (SUCCEEDED(g_dev->lpVtbl->Reset(g_dev, &g_pp))) {
                    g_dev->lpVtbl->GetBackBuffer(g_dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &g_backbuf);
                }
            }
            Render();
        }
        break;
    case WM_DESTROY:
        CancelGreen();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int nShow) {
    timeBeginPeriod(1);
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    QueryPerformanceFrequency(&g_freq);

    g_green_event = CreateEvent(NULL, TRUE, FALSE, NULL);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = "ReactionTest";
    RegisterClassExA(&wc);

    int sx = GetSystemMetrics(SM_CXSCREEN);
    int sy = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExA(0, "ReactionTest", "ReactionTest",
        WS_POPUP | WS_VISIBLE,
        0, 0, sx, sy,
        NULL, NULL, hInst, NULL);

    g_d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!g_d3d) exit(1);

    g_pp.Windowed = FALSE;
    g_pp.BackBufferWidth = sx;
    g_pp.BackBufferHeight = sy;
    g_pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    g_pp.BackBufferCount = 2;
    g_pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    g_pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    g_pp.hDeviceWindow = g_hwnd;
    g_pp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

    HRESULT hr = g_d3d->lpVtbl->CreateDevice(g_d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, g_hwnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &g_pp, &g_dev);
    if (FAILED(hr)) exit(1);

    g_dev->lpVtbl->GetBackBuffer(g_dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &g_backbuf);

    Render();

    MSG msg;
    for (;;) {
        DWORD result = MsgWaitForMultipleObjects(1, &g_green_event, FALSE, INFINITE, QS_ALLINPUT);

        if (result == WAIT_OBJECT_0) {
            if (g_state == STATE_WAIT_GREEN) {
                g_state = STATE_WAIT_REACT;
                Render();
            }
            ResetEvent(g_green_event);
            continue;
        }

        if (result == WAIT_OBJECT_0 + 1) {
            while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    CancelGreen();
                    if (g_backbuf) g_backbuf->lpVtbl->Release(g_backbuf);
                    if (g_dev) g_dev->lpVtbl->Release(g_dev);
                    if (g_d3d) g_d3d->lpVtbl->Release(g_d3d);
                    CloseHandle(g_green_event);
                    timeEndPeriod(1);
                    return (int)msg.wParam;
                }
                TranslateMessage(&msg);
                DispatchMessageA(&msg);
            }
            if (WaitForSingleObject(g_green_event, 0) == WAIT_OBJECT_0) {
                if (g_state == STATE_WAIT_GREEN) {
                    g_state = STATE_WAIT_REACT;
                    Render();
                }
                ResetEvent(g_green_event);
            }
        }
    }
    return 0;
}
