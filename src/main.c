#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "parser.h"
#include "analytics.h"

// Unique GUI Element IDs
#define IDC_TXT_FILEPATH    301
#define IDC_TXT_TICKER      302
#define IDC_BTN_LOAD        303
#define IDC_BTN_PORTFOLIO   304
#define IDC_BTN_GRAPH       305
#define IDC_TXT_OUTPUT      306

HWND hwndFilePath, hwndTicker, hwndOutputBox;
Market *g_market = NULL;
char g_gui_terminal_buffer[65536]; 

void gui_printf(const char *format, ...) {
    char temp[512];
    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    
    strcat(g_gui_terminal_buffer, temp);
    SetWindowTextA(hwndOutputBox, g_gui_terminal_buffer);
}

void gui_clear() {
    g_gui_terminal_buffer[0] = '\0';
    SetWindowTextA(hwndOutputBox, "");
}

LRESULT CALLBACK MainGuiWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            HFONT hNormalFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            HFONT hGridFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                                          FIXED_PITCH | FF_MODERN, "Courier New");

            // Input Fields
            HWND lbl1 = CreateWindowExA(0, "STATIC", "CSV File Path:", WS_CHILD | WS_VISIBLE, 20, 22, 100, 20, hwnd, NULL, NULL, NULL);
            SendMessageA(lbl1, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
            hwndFilePath = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "AAPL.csv", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 120, 20, 200, 24, hwnd, (HMENU)IDC_TXT_FILEPATH, NULL, NULL);
            SendMessageA(hwndFilePath, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

            HWND lbl2 = CreateWindowExA(0, "STATIC", "Ticker Symbol:", WS_CHILD | WS_VISIBLE, 340, 22, 100, 20, hwnd, NULL, NULL, NULL);
            SendMessageA(lbl2, WM_SETFONT, (WPARAM)hNormalFont, TRUE);
            hwndTicker = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "AAPL", WS_CHILD | WS_VISIBLE | ES_UPPERCASE, 440, 20, 90, 24, hwnd, (HMENU)IDC_TXT_TICKER, NULL, NULL);
            SendMessageA(hwndTicker, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

            // Action Buttons
            HWND btnLoad = CreateWindowExA(0, "BUTTON", "Load Stock CSV Data", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 550, 18, 250, 28, hwnd, (HMENU)IDC_BTN_LOAD, NULL, NULL);
            SendMessageA(btnLoad, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

            HWND btnPortfolio = CreateWindowExA(0, "BUTTON", "Display Loaded Portfolio Grid", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 60, 380, 32, hwnd, (HMENU)IDC_BTN_PORTFOLIO, NULL, NULL);
            SendMessageA(btnPortfolio, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

            HWND btnGraph = CreateWindowExA(0, "BUTTON", "Open Advanced Visual GDI Chart Window", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 420, 60, 380, 32, hwnd, (HMENU)IDC_BTN_GRAPH, NULL, NULL);
            SendMessageA(btnGraph, WM_SETFONT, (WPARAM)hNormalFont, TRUE);

            // Main Display Box
            hwndOutputBox = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL, 20, 110, 780, 470, hwnd, (HMENU)IDC_TXT_OUTPUT, NULL, NULL);
            SendMessageA(hwndOutputBox, WM_SETFONT, (WPARAM)hGridFont, TRUE);

            gui_printf("=========================================================================\r\n");
            gui_printf("             STOCK ANALYSIS ENGINE - NATIVE WINDOWS FRONTEND             \r\n");
            gui_printf("=========================================================================\r\n");
            gui_printf("Status: Operational. No command prompt required.\r\n\r\n");
            break;
        }

        case WM_COMMAND: {
            int controlId = LOWORD(wParam);
            switch (controlId) {
                case IDC_BTN_LOAD: {
                    char path[260] = {0};
                    char ticker[16] = {0};
                    GetWindowTextA(hwndFilePath, path, sizeof(path));
                    GetWindowTextA(hwndTicker, ticker, sizeof(ticker));

                    int overwrite_idx = -1;
                    for (int i = 0; i < g_market->stock_count; i++) {
                        if (_stricmp(g_market->stocks[i]->ticker, ticker) == 0) {
                            overwrite_idx = i;
                            break;
                        }
                    }

                    gui_printf("Loading file: '%s'...\r\n", path);
                    Stock *loaded = load_stock_from_csv(path, ticker);
                    if (loaded) {
                        if (overwrite_idx != -1) {
                            free_stock(g_market->stocks[overwrite_idx]);
                            g_market->stocks[overwrite_idx] = loaded;
                            gui_printf("[Update] Re-loaded data memory fields for '%s'.\r\n\r\n", ticker);
                        } else {
                            if (g_market->stock_count >= g_market->max_capacity) {
                                g_market->max_capacity *= 2;
                                g_market->stocks = (Stock**)realloc(g_market->stocks, g_market->max_capacity * sizeof(Stock*));
                            }
                            g_market->stocks[g_market->stock_count++] = loaded;
                            gui_printf("[Success] Asset '%s' loaded perfectly (%d records parsed).\r\n\r\n", ticker, loaded->record_count);
                        }
                    } else {
                        gui_printf("[Error] Failed to open or parse file structure.\r\n\r\n");
                    }
                    break;
                }

                case IDC_BTN_PORTFOLIO: {
                    gui_clear();
                    if (g_market->stock_count == 0) {
                        gui_printf("No data records loaded in memory dashboard yet.\r\n");
                        break;
                    }

                    gui_printf("==================================================================================\r\n");
                    gui_printf("                       ACTIVE PORTFOLIO REALTIME INDICATOR MATRIX                 \r\n");
                    gui_printf("==================================================================================\r\n");
                    gui_printf("TICKER   | SESSIONS | LATEST CLOSE | SMA 5    | TRADING SIGNAL\r\n");
                    gui_printf("---------+----------+--------------+----------+-----------------------------------\r\n");

                    for (int i = 0; i < g_market->stock_count; i++) {
                        Stock *s = g_market->stocks[i];
                        int last = s->record_count - 1;
                        double close_v = s->records[last].close;
                        double sm5 = calculate_sma(s, last, 5);
                        
                        char sig_str[256] = {0};
                        generate_trading_signal(s, sig_str, sizeof(sig_str));

                        gui_printf("%-8s | %-8d | $%-11.2f | %-8.2f | %s\r\n", 
                                   s->ticker, s->record_count, close_v, sm5, sig_str);
                    }
                    gui_printf("==================================================================================\r\n");
                    break;
                }

                case IDC_BTN_GRAPH: {
                    char ticker[16] = {0};
                    GetWindowTextA(hwndTicker, ticker, sizeof(ticker));

                    Stock *target = NULL;
                    for (int i = 0; i < g_market->stock_count; i++) {
                        if (_stricmp(g_market->stocks[i]->ticker, ticker) == 0) {
                            target = g_market->stocks[i];
                            break;
                        }
                    }

                    if (target) {
                        gui_printf("[UI Pipeline] Requesting window focus change. Launching graph window for %s...\r\n", ticker);
                        render_native_windows_graph(target, 60);
                    } else {
                        gui_printf("[Error] Graph aborted. Ticker symbol '%s' must be loaded first.\r\n\r\n", ticker);
                    }
                    break;
                }
            }
            break;
        }

        case WM_DESTROY: {
            free_market(g_market);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_market = (Market*)malloc(sizeof(Market));
    g_market->stock_count = 0;
    g_market->max_capacity = INITIAL_CAPACITY;
    g_market->stocks = (Stock**)malloc(g_market->max_capacity * sizeof(Stock*));

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = MainGuiWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "MainFrontendStockDashboardClass";

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Window Registration Execution Failure!", "Fatal Architecture Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        "MainFrontendStockDashboardClass",
        "Stock Trading Analysis Engine & Live Matrix Controls Panel",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 835, 640,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
