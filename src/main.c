#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "parser.h"
#include "analytics.h"

// Control IDs
#define IDC_FILEPATH    101
#define IDC_TICKER      102
#define IDC_BTN_LOAD    201
#define IDC_BTN_LEADER  202
#define IDC_BTN_GRAPH   203
#define IDC_BTN_EXPORT  204
#define IDC_BTN_PORT    205
#define IDC_BTN_EXIT    206
#define IDC_OUTPUT      301
#define IDC_STOCK_LIST  107

// Global Win32 control handles
HWND g_hwndOutput = NULL;
HWND g_hwndFilePath = NULL;
HWND g_hwndTicker = NULL;
HWND g_hwndStockList = NULL;

// Font handles
static HFONT g_hNormalFont = NULL;
static HFONT g_hMonospaceFont = NULL;

// Global market database pointer defined in analytics.c
extern Market *g_market;

void output_printf(const char *fmt, ...) {
    if (!g_hwndOutput) return;

    char buf[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Convert \n to \r\n for Win32 EDIT control compatibility
    char final_buf[4096];
    int j = 0;
    for (int i = 0; buf[i] != '\0' && j < 4094; i++) {
        if (buf[i] == '\n' && (i == 0 || buf[i-1] != '\r')) {
            final_buf[j++] = '\r';
        }
        final_buf[j++] = buf[i];
    }
    final_buf[j] = '\0';

    int len = GetWindowTextLengthA(g_hwndOutput);
    SendMessageA(g_hwndOutput, EM_SETSEL, len, len);
    SendMessageA(g_hwndOutput, EM_REPLACESEL, FALSE, (LPARAM)final_buf);
    SendMessageA(g_hwndOutput, EM_SCROLLCARET, 0, 0);
}

void output_clear() {
    if (g_hwndOutput) {
        SetWindowTextA(g_hwndOutput, "");
    }
}

void refresh_stock_list() {
    if (!g_hwndStockList) return;
    
    // Clear combobox
    SendMessageA(g_hwndStockList, CB_RESETCONTENT, 0, 0);
    
    // Add each loaded stock's ticker
    for (int i = 0; i < g_market->stock_count; i++) {
        SendMessageA(g_hwndStockList, CB_ADDSTRING, 0, (LPARAM)g_market->stocks[i]->ticker);
    }
    
    // Select first item if any
    if (g_market->stock_count > 0) {
        SendMessageA(g_hwndStockList, CB_SETCURSEL, 0, 0);
    }
}

LRESULT CALLBACK MainGuiWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hNormalFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
            g_hMonospaceFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                                          FIXED_PITCH | FF_MODERN, "Courier New");

            // Row 1 Input field labels
            HWND lblFilePath = CreateWindowExA(0, "STATIC", "CSV File:", WS_CHILD | WS_VISIBLE, 20, 18, 70, 20, hwnd, NULL, NULL, NULL);
            SendMessageA(lblFilePath, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            g_hwndFilePath = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "AAPL.csv", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 95, 15, 200, 24, hwnd, (HMENU)IDC_FILEPATH, NULL, NULL);
            SendMessageA(g_hwndFilePath, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            HWND lblTicker = CreateWindowExA(0, "STATIC", "Load as:", WS_CHILD | WS_VISIBLE, 315, 18, 70, 20, hwnd, NULL, NULL, NULL);
            SendMessageA(lblTicker, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            g_hwndTicker = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "AAPL", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_UPPERCASE, 390, 15, 80, 24, hwnd, (HMENU)IDC_TICKER, NULL, NULL);
            SendMessageA(g_hwndTicker, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            // Row 2 Dropdown (COMBOBOX)
            HWND lblStockList = CreateWindowExA(0, "STATIC", "Select Stock:", WS_CHILD | WS_VISIBLE, 20, 48, 100, 20, hwnd, NULL, NULL, NULL);
            SendMessageA(lblStockList, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            g_hwndStockList = CreateWindowExA(0, "COMBOBOX", "", CBS_DROPDOWNLIST | WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_SORT, 130, 45, 185, 200, hwnd, (HMENU)IDC_STOCK_LIST, NULL, NULL);
            SendMessageA(g_hwndStockList, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            // Row 1 Buttons (Load + Leaderboard)
            HWND btnLoad = CreateWindowExA(0, "BUTTON", "1. Load Stock CSV", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 80, 380, 30, hwnd, (HMENU)IDC_BTN_LOAD, NULL, NULL);
            SendMessageA(btnLoad, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            HWND btnLeader = CreateWindowExA(0, "BUTTON", "2. 30-Day Leaderboard", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 415, 80, 380, 30, hwnd, (HMENU)IDC_BTN_LEADER, NULL, NULL);
            SendMessageA(btnLeader, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            // Row 2 Buttons (View+Graph + Export)
            HWND btnGraph = CreateWindowExA(0, "BUTTON", "3. View + Graph", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 115, 380, 30, hwnd, (HMENU)IDC_BTN_GRAPH, NULL, NULL);
            SendMessageA(btnGraph, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            HWND btnExport = CreateWindowExA(0, "BUTTON", "4. Export Report", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 415, 115, 380, 30, hwnd, (HMENU)IDC_BTN_EXPORT, NULL, NULL);
            SendMessageA(btnExport, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            // Row 3 Buttons (Portfolio + Exit)
            HWND btnPort = CreateWindowExA(0, "BUTTON", "5. Portfolio Table", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 150, 380, 30, hwnd, (HMENU)IDC_BTN_PORT, NULL, NULL);
            SendMessageA(btnPort, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            HWND btnExit = CreateWindowExA(0, "BUTTON", "6. Exit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 415, 150, 380, 30, hwnd, (HMENU)IDC_BTN_EXIT, NULL, NULL);
            SendMessageA(btnExit, WM_SETFONT, (WPARAM)g_hNormalFont, TRUE);

            // Log / Console display box
            g_hwndOutput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | ES_AUTOHSCROLL, 20, 195, 775, 345, hwnd, (HMENU)IDC_OUTPUT, NULL, NULL);
            SendMessageA(g_hwndOutput, WM_SETFONT, (WPARAM)g_hMonospaceFont, TRUE);

            output_printf("=========================================================================\n");
            output_printf("             STOCK ANALYSIS ENGINE - PURE WINDOWS GUI MODE               \n");
            output_printf("=========================================================================\n");
            output_printf("Ready. Use buttons 1-6 to control the application.\n\n");
            break;
        }

        case WM_COMMAND: {
            int controlId = LOWORD(wParam);
            switch (controlId) {
                case IDC_BTN_LOAD: {
                    char path[260] = {0};
                    char ticker[16] = {0};
                    GetWindowTextA(g_hwndFilePath, path, sizeof(path));
                    GetWindowTextA(g_hwndTicker, ticker, sizeof(ticker));

                    int exists_idx = -1;
                    for (int i = 0; i < g_market->stock_count; i++) {
                        if (_stricmp(g_market->stocks[i]->ticker, ticker) == 0) {
                            exists_idx = i;
                            break;
                        }
                    }

                    output_printf("Loading CSV: '%s' with ticker '%s'...\n", path, ticker);
                    Stock *loaded = load_stock_from_csv(path, ticker);
                    if (loaded) {
                        if (exists_idx != -1) {
                            free_stock(g_market->stocks[exists_idx]);
                            g_market->stocks[exists_idx] = loaded;
                            output_printf("[Success] Replaced existing stock '%s' (%d records parsed).\n\n", ticker, loaded->record_count);
                        } else {
                            if (g_market->stock_count >= g_market->max_capacity) {
                                g_market->max_capacity *= 2;
                                Stock **new_stocks = (Stock**)realloc(g_market->stocks, g_market->max_capacity * sizeof(Stock*));
                                if (!new_stocks) {
                                    output_printf("[Error] Memory allocation failed while resizing market database.\n\n");
                                    free_stock(loaded);
                                    break;
                                }
                                g_market->stocks = new_stocks;
                            }
                            g_market->stocks[g_market->stock_count++] = loaded;
                            output_printf("[Success] Loaded stock '%s' (%d records parsed).\n\n", ticker, loaded->record_count);
                        }
                        refresh_stock_list();
                    } else {
                        output_printf("[Error] Failed to load stock CSV from '%s'. Check file path/format.\n\n", path);
                    }
                    break;
                }

                case IDC_BTN_LEADER: {
                    output_clear();
                    rank_market_performers(g_market);
                    break;
                }

                case IDC_BTN_GRAPH: {
                    char ticker[16] = {0};
                    int sel = (int)SendMessageA(g_hwndStockList, CB_GETCURSEL, 0, 0);
                    if (sel == CB_ERR) {
                        output_printf("No stock selected. Load a stock first.\n\n");
                        break;
                    }
                    SendMessageA(g_hwndStockList, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)ticker);

                    Stock *target = NULL;
                    for (int i = 0; i < g_market->stock_count; i++) {
                        if (_stricmp(g_market->stocks[i]->ticker, ticker) == 0) {
                            target = g_market->stocks[i];
                            break;
                        }
                    }

                    if (target) {
                        int latest_idx = target->record_count - 1;
                        double latest_close = target->records[latest_idx].close;
                        double sma5 = calculate_sma(target, latest_idx, 5);
                        double sma20 = calculate_sma(target, latest_idx, 20);
                        double rsi = calculate_rsi(target, latest_idx, 14);
                        char signal[SIGNAL_BUF];
                        generate_enhanced_signal(target, signal, sizeof(signal));

                        output_printf(">>> Viewing Analytics for %s <<<\n", target->ticker);
                        output_printf("Latest Session Close: $%.2f (%s)\n", latest_close, target->records[latest_idx].date);
                        output_printf("5-Day SMA:            $%.2f\n", sma5);
                        output_printf("20-Day SMA:           $%.2f\n", sma20);
                        output_printf("RSI (14):             %.2f\n", rsi);
                        output_printf("Signal:               %s\n", signal);
                        output_printf("Launching GUI chart window...\n\n");

                        render_native_windows_graph(target, 60);
                    } else {
                        output_printf("[Error] Ticker '%s' not found.\n\n", ticker);
                    }
                    break;
                }

                case IDC_BTN_EXPORT: {
                    char ticker[16] = {0};
                    int sel = (int)SendMessageA(g_hwndStockList, CB_GETCURSEL, 0, 0);
                    if (sel == CB_ERR) {
                        output_printf("No stock selected. Load a stock first.\n\n");
                        break;
                    }
                    SendMessageA(g_hwndStockList, CB_GETLBTEXT, (WPARAM)sel, (LPARAM)ticker);

                    Stock *target = NULL;
                    for (int i = 0; i < g_market->stock_count; i++) {
                        if (_stricmp(g_market->stocks[i]->ticker, ticker) == 0) {
                            target = g_market->stocks[i];
                            break;
                        }
                    }

                    if (target) {
                        export_analytical_report(target);
                        output_printf("Report exported to 'exports/%s_report.txt' and chart saved to 'exports/%s_chart.bmp'.\n\n", target->ticker, target->ticker);
                    } else {
                        output_printf("[Error] Ticker '%s' not found.\n\n", ticker);
                    }
                    break;
                }

                case IDC_BTN_PORT: {
                    output_clear();
                    if (g_market->stock_count == 0) {
                        output_printf("No data records loaded in memory dashboard yet.\n");
                    } else {
                        output_printf("==================================================================================\n");
                        output_printf("                       ACTIVE PORTFOLIO REALTIME INDICATOR MATRIX                 \n");
                        output_printf("==================================================================================\n");
                        output_printf("%-8s | %-8s | %-12s | %-8s | %-8s | %-6s | %s\n", 
                                      "TICKER", "RECORDS", "LATEST CLOSE", "SMA 5", "SMA 20", "RSI", "SIGNAL");
                        output_printf("---------+----------+--------------+----------+----------+--------+-------------------\n");

                        for (int i = 0; i < g_market->stock_count; i++) {
                            Stock *s = g_market->stocks[i];
                            int last = s->record_count - 1;
                            double close_v = s->records[last].close;
                            double sm5 = calculate_sma(s, last, 5);
                            double sm20 = calculate_sma(s, last, 20);
                            double rsi_v = calculate_rsi(s, last, 14);
                            
                            char sig_str[SIGNAL_BUF] = {0};
                            generate_enhanced_signal(s, sig_str, sizeof(sig_str));

                            output_printf("%-8s | %-8d | $%-11.2f | %-8.2f | %-8.2f | %-6.2f | %s\n", 
                                          s->ticker, s->record_count, close_v, sm5, sm20, rsi_v, sig_str);
                        }
                        output_printf("==================================================================================\n");
                    }
                    break;
                }

                case IDC_BTN_EXIT: {
                    SendMessageA(hwnd, WM_CLOSE, 0, 0);
                    break;
                }
            }
            break;
        }

        case WM_CLOSE: {
            DestroyWindow(hwnd);
            break;
        }

        case WM_DESTROY: {
            if (g_market) {
                free_market(g_market);
                g_market = NULL;
            }
            if (g_hMonospaceFont) {
                DeleteObject(g_hMonospaceFont);
                g_hMonospaceFont = NULL;
            }
            PostQuitMessage(0);
            break;
        }

        default:
            return DefWindowProcA(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    // Initialize global market pointer structure
    g_market = (Market*)malloc(sizeof(Market));
    if (!g_market) {
        MessageBoxA(NULL, "Fatal: Memory allocation failed for Market.", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }
    g_market->stock_count = 0;
    g_market->max_capacity = INITIAL_CAPACITY;
    g_market->stocks = (Stock**)malloc(g_market->max_capacity * sizeof(Stock*));
    if (!g_market->stocks) {
        free(g_market);
        MessageBoxA(NULL, "Fatal: Memory allocation failed for Market stocks.", "Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = MainGuiWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "StockAnalyserMainClass";

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Window Registration Execution Failure!", "Fatal Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    // Determine window size and center positioning
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 835;
    int windowHeight = 600;
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        "StockAnalyserMainClass",
        "Stock Exchange Analyser",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, windowWidth, windowHeight,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Window Creation Failure!", "Fatal Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
