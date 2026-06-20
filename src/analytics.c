#include "analytics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <windows.h>

extern void output_printf(const char *fmt, ...);
Market *g_market = NULL;

// Automatically instruct the MSVC Linker to include the Win32 GUI Subsystems
#ifdef _MSC_VER
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

#define ROWS 15
#define MAX_COLS 80

// Global structures to pass dataset data safely into the Win32 Window message pump thread context
typedef struct {
    const Stock *stock;
    int datapoints;
    int start_idx;
    int num_points;
    double min_val;
    double max_val;
} GraphRenderContext;

static GraphRenderContext g_ctx;

double calculate_sma(const Stock *stock, int target_index, int window) {
    if (!stock || target_index < 0 || target_index >= stock->record_count || window <= 0) {
        return 0.0;
    }
    int start = target_index - window + 1;
    if (start < 0) {
        return 0.0; 
    }
    double sum = 0.0;
    for (int i = start; i <= target_index; i++) {
        sum += stock->records[i].close;
    }
    return sum / window;
}

void generate_trading_signal(const Stock *stock, char *output_signal, size_t buf_size) {
    if (!stock || stock->record_count <= 0 || buf_size < SIGNAL_BUF) {
        if (buf_size > 0) {
            strncpy(output_signal, "HOLD", buf_size - 1);
            output_signal[buf_size - 1] = '\0';
        }
        return;
    }

    int latest_idx = stock->record_count - 1;
    double sma5 = calculate_sma(stock, latest_idx, 5);
    double sma20 = calculate_sma(stock, latest_idx, 20);

    if (sma5 == 0.0 || sma20 == 0.0) {
        strncpy(output_signal, "HOLD", buf_size - 1);
    } else if (sma5 > sma20) {
        strncpy(output_signal, "BUY", buf_size - 1);
    } else if (sma5 < sma20) {
        strncpy(output_signal, "SELL", buf_size - 1);
    } else {
        strncpy(output_signal, "HOLD", buf_size - 1);
    }
    output_signal[buf_size - 1] = '\0';
}

double calculate_rsi(const Stock *stock, int target_index, int period) {
    if (!stock || target_index < period || target_index >= stock->record_count || period <= 0)
        return 50.0;  

    double avg_gain = 0.0, avg_loss = 0.0;

    for (int i = target_index - period + 1; i <= target_index; i++) {
        double change = stock->records[i].close - stock->records[i - 1].close;
        if (change > 0) avg_gain += change;
        else            avg_loss -= change;  
    }
    avg_gain /= period;
    avg_loss /= period;

    if (avg_loss == 0.0) return 100.0;  
    double rs = avg_gain / avg_loss;
    return 100.0 - (100.0 / (1.0 + rs));
}

void generate_enhanced_signal(const Stock *stock, char *output_signal, size_t buf_size) {
    if (!stock || stock->record_count <= 0 || buf_size < SIGNAL_BUF) {
        if (buf_size > 0) {
            strncpy(output_signal, "HOLD", buf_size - 1);
            output_signal[buf_size - 1] = '\0';
        }
        return;
    }

    int latest_idx = stock->record_count - 1;
    double sma5 = calculate_sma(stock, latest_idx, 5);
    double sma20 = calculate_sma(stock, latest_idx, 20);
    double rsi = calculate_rsi(stock, latest_idx, 14);

    if (sma5 == 0.0 || sma20 == 0.0) {
        strncpy(output_signal, "HOLD", buf_size - 1);
    } else if (sma5 > sma20 && rsi < 70.0) {
        strncpy(output_signal, "BUY", buf_size - 1);
    } else if (sma5 < sma20 && rsi > 30.0) {
        strncpy(output_signal, "SELL", buf_size - 1);
    } else {
        strncpy(output_signal, "HOLD", buf_size - 1);
    }
    output_signal[buf_size - 1] = '\0';
}

static double calculate_growth_30d(const Stock *stock) {
    if (!stock || stock->record_count <= 1) {
        return 0.0;
    }
    int latest_idx = stock->record_count - 1;
    int prev_idx = latest_idx - 30;
    if (prev_idx < 0) {
        prev_idx = 0; 
    }
    double old_close = stock->records[prev_idx].close;
    double new_close = stock->records[latest_idx].close;
    if (old_close == 0.0) {
        return 0.0;
    }
    return ((new_close - old_close) / old_close) * 100.0;
}

static int compare_performance(const void *a, const void *b) {
    Stock *stockA = *(Stock**)a;
    Stock *stockB = *(Stock**)b;
    double growthA = calculate_growth_30d(stockA);
    double growthB = calculate_growth_30d(stockB);
    if (growthA < growthB) return 1;
    if (growthA > growthB) return -1;
    return 0;
}

void rank_market_performers(Market *market) {
    if (!market || market->stock_count <= 0) {
        output_printf("No assets loaded in market database.\n");
        return;
    }

    qsort(market->stocks, market->stock_count, sizeof(Stock*), compare_performance);
    
    output_printf("=======================================================================\n");
    output_printf("                   30-DAY MARKET PERFORMANCE LEADERBOARD               \n");
    output_printf("=======================================================================\n");

    output_printf("%-5s | %-10s | %-12s | %-15s | %-10s\n", "Rank", "Ticker", "Latest Close", "30d Growth %", "Signal");
    output_printf("-----------------------------------------------------------------------\n");

    for (int i = 0; i < market->stock_count; i++) {
        Stock *s = market->stocks[i];
        double latest_close = s->records[s->record_count - 1].close;
        double growth = calculate_growth_30d(s);
        char signal[SIGNAL_BUF];
        generate_enhanced_signal(s, signal, sizeof(signal));

        output_printf("%-5d | %-10s | $%-11.2f | ", i + 1, s->ticker, latest_close);

        if (growth > 0) {
            output_printf("+%-14.2f%%", growth);
        } else {
            output_printf("%-15.2f%%", growth);
        }
        output_printf(" | ");

        if (strcmp(signal, "BUY") == 0) {
            output_printf("[BUY]");
        } else if (strcmp(signal, "SELL") == 0) {
            output_printf("[SELL]");
        } else {
            output_printf("[HOLD]");
        }
        output_printf("\n");
    }
    output_printf("-----------------------------------------------------------------------\n");
}

// ============================================================================
// NATIVE ADVANCED WINDOWS EXTENSION: GDI GRAPHICS WINDOW PIPELINE
// ============================================================================
LRESULT CALLBACK GraphWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;

            // 1. Draw UI Background Frame (Deep Slate Theme)
            HBRUSH bgBrush = CreateSolidBrush(RGB(18, 18, 20));
            FillRect(hdc, &rect, bgBrush);
            DeleteObject(bgBrush);

            // Establish Layout Canvas Boundaries
            int padding_left = 80;
            int padding_right = 40;
            int padding_top = 60;
            int padding_bottom = 60;

            int graph_w = width - padding_left - padding_right;
            int graph_h = height - padding_top - padding_bottom;

            if (g_ctx.num_points <= 1 || graph_w <= 0 || graph_h <= 0) {
                TextOutA(hdc, 20, 20, "Insufficient stock matrix parameters.", 37);
                EndPaint(hwnd, &ps);
                return 0;
            }

            // 2. Draw Vector Grid Mapping Lines
            HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(45, 45, 50));
            HPEN oldPen = (HPEN)SelectObject(hdc, gridPen);
            
            int grid_lines = 5;
            for (int i = 0; i <= grid_lines; i++) {
                int y = padding_top + (graph_h * i / grid_lines);
                MoveToEx(hdc, padding_left, y, NULL);
                LineTo(hdc, width - padding_right, y);

                // Compute prices along track segments
                double price_val = g_ctx.max_val - ((g_ctx.max_val - g_ctx.min_val) * i / grid_lines);
                char price_lbl[32];
                snprintf(price_lbl, sizeof(price_lbl), "$%.2f", price_val);
                
                SetTextColor(hdc, RGB(140, 140, 150));
                SetBkMode(hdc, TRANSPARENT);
                TextOutA(hdc, 15, y - 8, price_lbl, (int)strlen(price_lbl));
            }
            SelectObject(hdc, oldPen);
            DeleteObject(gridPen);

            // 3. Render Title Overlay Meta Text
            HFONT titleFont = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            HFONT oldFont = (HFONT)SelectObject(hdc, titleFont);
            SetTextColor(hdc, RGB(0, 180, 216));
            
            char title_text[128];
            snprintf(title_text, sizeof(title_text), "%s Advanced Technical Overview Dashboard", g_ctx.stock->ticker);
            TextOutA(hdc, padding_left, 15, title_text, (int)strlen(title_text));

            // Render Chart Legend Descriptions
            HFONT legendFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            SelectObject(hdc, legendFont);
            
            SetTextColor(hdc, RGB(0, 180, 216)); TextOutA(hdc, padding_left + 350, 22, "[-] Close Price", 15);
            SetTextColor(hdc, RGB(255, 183, 3));  TextOutA(hdc, padding_left + 470, 22, "[-] 5-Day SMA", 13);
            SetTextColor(hdc, RGB(232, 63, 91));  TextOutA(hdc, padding_left + 580, 22, "[-] 20-Day SMA", 14);

            // 4. Plot Vector Trend Lines (Close, SMA5, SMA20)
            HPEN pricePen = CreatePen(PS_SOLID, 3, RGB(0, 180, 216)); // Cyan
            HPEN sma5Pen  = CreatePen(PS_DASH, 1, RGB(255, 183, 3));  // Amber Yellow
            HPEN sma20Pen = CreatePen(PS_SOLID, 2, RGB(232, 63, 91)); // Rose Crimson

            // -- Lambda Coordinate Mapping Macro --
            #define GET_X_COORD(idx) (padding_left + ((idx) * graph_w / (g_ctx.num_points - 1)))
            #define GET_Y_COORD(val) (padding_top + graph_h - (int)(((val) - g_ctx.min_val) / (g_ctx.max_val - g_ctx.min_val) * graph_h))

            // Line 1: Actual Closing Stock Value Path
            SelectObject(hdc, pricePen);
            for (int i = 0; i < g_ctx.num_points; i++) {
                int data_idx = g_ctx.start_idx + i;
                int x = GET_X_COORD(i);
                int y = GET_Y_COORD(g_ctx.stock->records[data_idx].close);
                if (i == 0) MoveToEx(hdc, x, y, NULL); else LineTo(hdc, x, y);
            }

            // Line 2: 5-Day Technical Simple Moving Average
            SelectObject(hdc, sma5Pen);
            int moved_sma5 = 0;
            for (int i = 0; i < g_ctx.num_points; i++) {
                int data_idx = g_ctx.start_idx + i;
                double s5 = calculate_sma(g_ctx.stock, data_idx, 5);
                if (s5 > 0.0) {
                    int x = GET_X_COORD(i);
                    int y = GET_Y_COORD(s5);
                    if (!moved_sma5) { MoveToEx(hdc, x, y, NULL); moved_sma5 = 1; } else LineTo(hdc, x, y);
                }
            }

            // Line 3: 20-Day Technical Simple Moving Average
            SelectObject(hdc, sma20Pen);
            int moved_sma20 = 0;
            for (int i = 0; i < g_ctx.num_points; i++) {
                int data_idx = g_ctx.start_idx + i;
                double s20 = calculate_sma(g_ctx.stock, data_idx, 20);
                if (s20 > 0.0) {
                    int x = GET_X_COORD(i);
                    int y = GET_Y_COORD(s20);
                    if (!moved_sma20) { MoveToEx(hdc, x, y, NULL); moved_sma20 = 1; } else LineTo(hdc, x, y);
                }
            }

            // 5. Draw Axis Bottom Timeline String Boundaries
            SetTextColor(hdc, RGB(140, 140, 150));
            TextOutA(hdc, padding_left, height - padding_bottom + 10, g_ctx.stock->records[g_ctx.start_idx].date, 10);
            
            char end_date_str[16];
            snprintf(end_date_str, sizeof(end_date_str), "End: %s", g_ctx.stock->records[g_ctx.stock->record_count - 1].date);
            TextOutA(hdc, width - padding_right - 110, height - padding_bottom + 10, end_date_str, (int)strlen(end_date_str));

            // Clean allocated structural memory assets
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldFont);
            DeleteObject(pricePen);
            DeleteObject(sma5Pen);
            DeleteObject(sma20Pen);
            DeleteObject(titleFont);
            DeleteObject(legendFont);

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void render_native_windows_graph(const Stock *stock, int datapoints) {
    if (!stock || stock->record_count <= 0 || datapoints <= 0) {
        printf("Error: Missing internal tracking dataset parameters.\n");
        return;
    }

    // Populate Thread Context Framework structures
    g_ctx.stock = stock;
    g_ctx.datapoints = datapoints;
    g_ctx.start_idx = stock->record_count - datapoints;
    if (g_ctx.start_idx < 0) g_ctx.start_idx = 0;
    g_ctx.num_points = stock->record_count - g_ctx.start_idx;

    // Evaluate vector scaling boundaries
    g_ctx.min_val = stock->records[g_ctx.start_idx].close;
    g_ctx.max_val = stock->records[g_ctx.start_idx].close;
    for (int i = 0; i < g_ctx.num_points; i++) {
        int idx = g_ctx.start_idx + i;
        double v = stock->records[idx].close;
        double s5 = calculate_sma(stock, idx, 5);
        double s20 = calculate_sma(stock, idx, 20);

        if (v < g_ctx.min_val) g_ctx.min_val = v;
        if (v > g_ctx.max_val) g_ctx.max_val = v;
        if (s5 > 0.0 && s5 < g_ctx.min_val) g_ctx.min_val = s5;
        if (s5 > 0.0 && s5 > g_ctx.max_val) g_ctx.max_val = s5;
        if (s20 > 0.0 && s20 < g_ctx.min_val) g_ctx.min_val = s20;
        if (s20 > 0.0 && s20 > g_ctx.max_val) g_ctx.max_val = s20;
    }

    // Defensive Check: Prevent division by zero if stock pricing matches perfectly flat lines
    if (g_ctx.max_val == g_ctx.min_val) {
        g_ctx.min_val -= 1.0;
        g_ctx.max_val += 1.0;
    }

    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    // Clear out/Unregister an older registration instance to prevent collision locks
    UnregisterClassA("StockGraphWindowClass", hInstance);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = GraphWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "StockGraphWindowClass";

    if (!RegisterClassExA(&wc)) {
        printf("[GUI Error] Failed to register window class! Error Code: %lu\n", GetLastError());
        return;
    }

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        "StockGraphWindowClass",
        "Micro-Stock Trend Engine (Native GUI Mode)",
        WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 550,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        printf("[GUI Error] CreateWindowExA returned NULL! Error Code: %lu\n", GetLastError());
        return;
    }

    printf("\n[GUI Launcher] Spawning dedicated Win32 canvas window successfully.\n");
    printf("[GUI Launcher] Close window to return back to the command prompt menu.\n");

    // Force initialization visibility rules explicitly down to OS Desktop window manager
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    // Process OS Message Pump Loops safely
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Unregister window class cleanly for structural memory re-allocation tasks
    UnregisterClassA("StockGraphWindowClass", hInstance);
}

static void format_commas(double val, char *buf, size_t buf_size) {
    char temp[64];
    snprintf(temp, sizeof(temp), "%.0f", val);
    int len = (int)strlen(temp);
    int commas = (len - 1) / 3;
    int new_len = len + commas;
    if ((size_t)new_len >= buf_size) {
        strncpy(buf, temp, buf_size - 1);
        buf[buf_size - 1] = '\0';
        return;
    }
    int src = len - 1;
    int dst = new_len - 1;
    buf[new_len] = '\0';
    int count = 0;
    while (src >= 0) {
        buf[dst--] = temp[src--];
        count++;
        if (count == 3 && src >= 0) {
            buf[dst--] = ',';
            count = 0;
        }
    }
}

void export_analytical_report(const Stock *stock) {
    int datapoints = 60;
    if (!stock || stock->record_count <= 0) {
        printf("No stock data to export.\n");
        return;
    }

    CreateDirectoryA("exports", NULL);

    char filepath[64];
    snprintf(filepath, sizeof(filepath), "exports/%s_report.txt", stock->ticker);

    FILE *file = fopen(filepath, "w");
    if (!file) {
        printf("Failed to create report file: %s\n", filepath);
        return;
    }

    double latest_close = stock->records[stock->record_count - 1].close;
    double latest_open = stock->records[stock->record_count - 1].open;
    double latest_high = stock->records[stock->record_count - 1].high;
    double latest_low = stock->records[stock->record_count - 1].low;
    unsigned long latest_vol = stock->records[stock->record_count - 1].volume;
    double growth = calculate_growth_30d(stock);
    char signal[SIGNAL_BUF];
    generate_enhanced_signal(stock, signal, sizeof(signal));

    double sma5 = calculate_sma(stock, stock->record_count - 1, 5);
    double sma20 = calculate_sma(stock, stock->record_count - 1, 20);
    double rsi = calculate_rsi(stock, stock->record_count - 1, 14);

    time_t rawtime;
    struct tm *timeinfo;
    char time_buffer[80];
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

    fprintf(file, "========================================================\n");
    fprintf(file, "               STOCK ANALYTICS REPORT: %s\n", stock->ticker);
    fprintf(file, "========================================================\n");
    fprintf(file, "Export Date/Time: %s\n", time_buffer);
    fprintf(file, "Ticker:           %s\n", stock->ticker);
    fprintf(file, "Total Records:    %d days\n", stock->record_count);
    fprintf(file, "Date Range:       %s to %s\n", stock->records[0].date, stock->records[stock->record_count - 1].date);
    fprintf(file, "--------------------------------------------------------\n");
    fprintf(file, "LATEST SESSION VALUES (%s):\n", stock->records[stock->record_count - 1].date);
    fprintf(file, "  Open:           $%.2f\n", latest_open);
    fprintf(file, "  High:           $%.2f\n", latest_high);
    fprintf(file, "  Low:            $%.2f\n", latest_low);
    fprintf(file, "  Close:          $%.2f\n", latest_close);
    fprintf(file, "  Volume:         %lu shares\n", latest_vol);
    fprintf(file, "--------------------------------------------------------\n");
    fprintf(file, "TECHNICAL INDICATORS:\n");
    fprintf(file, "  5-day SMA:      $%.2f\n", sma5);
    fprintf(file, "  20-day SMA:     $%.2f\n", sma20);
    fprintf(file, "  RSI-14:         %.2f\n", rsi);
    fprintf(file, "  30-day Growth:  %.2f%%\n", growth);
    fprintf(file, "  Current Signal: %s\n", signal);
    fprintf(file, "--------------------------------------------------------\n");

    // Compute summary statistics
    double highest_close = stock->records[0].close;
    double lowest_close = stock->records[0].close;
    double sum_close = 0.0;
    unsigned long sum_volume = 0;
    int highest_idx = 0, lowest_idx = 0;

    for (int i = 0; i < stock->record_count; i++) {
        double c = stock->records[i].close;
        sum_close += c;
        sum_volume += stock->records[i].volume;
        if (c > highest_close) { highest_close = c; highest_idx = i; }
        if (c < lowest_close)  { lowest_close = c;  lowest_idx = i; }
    }

    double avg_close = sum_close / stock->record_count;
    double avg_volume = (double)sum_volume / stock->record_count;
    double price_range = highest_close - lowest_close;

    // 30-day volatility = standard deviation of daily returns over 30 days
    double volatility = 0.0;
    int vol_start = stock->record_count - 30;
    if (vol_start < 0) vol_start = 1;  // need at least 2 points
    int vol_count = 0;
    double sum_return = 0.0;
    double sum_return_sq = 0.0;
    for (int i = vol_start; i < stock->record_count; i++) {
        double prev = stock->records[i-1].close;
        double curr = stock->records[i].close;
        if (prev != 0.0) {
            double ret = (curr - prev) / prev;
            sum_return += ret;
            sum_return_sq += ret * ret;
            vol_count++;
        }
    }
    if (vol_count > 1) {
        double mean = sum_return / vol_count;
        volatility = sqrt((sum_return_sq - vol_count * mean * mean) / (vol_count - 1)) * 100.0;
    }

    char vol_buf[32];
    format_commas(avg_volume, vol_buf, sizeof(vol_buf));

    fprintf(file, "SUMMARY STATISTICS:\n");
    fprintf(file, "  Highest Close:     $%.2f  (%s)\n", highest_close, stock->records[highest_idx].date);
    fprintf(file, "  Lowest Close:      $%.2f  (%s)\n", lowest_close, stock->records[lowest_idx].date);
    fprintf(file, "  Average Close:     $%.2f\n", avg_close);
    fprintf(file, "  Average Volume:    %s shares\n", vol_buf);
    fprintf(file, "  Price Range:       $%.2f\n", price_range);
    fprintf(file, "  Volatility (30d):  %.2f%%\n", volatility);
    fprintf(file, "========================================================\n");

    fclose(file);
    printf("Report successfully exported to: %s and chart to exports/%s_chart.bmp\n", filepath, stock->ticker);

    // --- Save GDI chart as BMP ---
    {
        int chart_width = 800;
        int chart_height = 450;
        int padding_left = 70;
        int padding_right = 40;
        int padding_top = 40;
        int padding_bottom = 50;

        // Calculate min/max same way as render_native_windows_graph
        int start_idx = stock->record_count - datapoints;
        if (start_idx < 0) start_idx = 0;
        int num_points = stock->record_count - start_idx;
        if (num_points <= 1) return;  // skip chart, not enough data

        double min_val = stock->records[start_idx].close;
        double max_val = stock->records[start_idx].close;
        for (int i = 0; i < num_points; i++) {
            int idx = start_idx + i;
            double v = stock->records[idx].close;
            double s5 = calculate_sma(stock, idx, 5);
            double s20 = calculate_sma(stock, idx, 20);
            if (v < min_val) min_val = v;
            if (v > max_val) max_val = v;
            if (s5 > 0 && s5 < min_val) min_val = s5;
            if (s5 > 0 && s5 > max_val) max_val = s5;
            if (s20 > 0 && s20 < min_val) min_val = s20;
            if (s20 > 0 && s20 > max_val) max_val = s20;
        }
        if (max_val == min_val) { min_val -= 1.0; max_val += 1.0; }

        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, chart_width, chart_height);
        SelectObject(hdcMem, hBitmap);

        // White background
        RECT rect = { 0, 0, chart_width, chart_height };
        HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdcMem, &rect, whiteBrush);
        DeleteObject(whiteBrush);

        // --- Draw the chart lines (same as render_native_windows_graph) ---
        // Price line
        HPEN pricePen = CreatePen(PS_SOLID, 2, RGB(30, 100, 200));
        HPEN sma5Pen = CreatePen(PS_DASH, 1, RGB(50, 180, 50));
        HPEN sma20Pen = CreatePen(PS_DASH, 1, RGB(220, 50, 50));

        // Map function: stock value → pixel Y
        // chart_y = padding_top + (1 - ratio) * (chart_height - padding_top - padding_bottom)
        // where ratio = (val - min_val) / (max_val - min_val)

        // --- Draw price line ---
        SelectObject(hdcMem, pricePen);
        MoveToEx(hdcMem,
            padding_left,
            padding_top + (int)((1.0 - (stock->records[start_idx].close - min_val) / (max_val - min_val)) * (chart_height - padding_top - padding_bottom)),
            NULL);
        for (int i = 1; i < num_points; i++) {
            double ratio = (stock->records[start_idx + i].close - min_val) / (max_val - min_val);
            int x = padding_left + (int)((double)i / (num_points - 1) * (chart_width - padding_left - padding_right));
            int y = padding_top + (int)((1.0 - ratio) * (chart_height - padding_top - padding_bottom));
            LineTo(hdcMem, x, y);
        }

        // --- Draw SMA5 line ---
        SelectObject(hdcMem, sma5Pen);
        int first_sma5 = -1;
        for (int i = 0; i < num_points; i++) {
            double s5 = calculate_sma(stock, start_idx + i, 5);
            if (s5 <= 0.0) continue;
            int x = padding_left + (int)((double)i / (num_points - 1) * (chart_width - padding_left - padding_right));
            double ratio = (s5 - min_val) / (max_val - min_val);
            int y = padding_top + (int)((1.0 - ratio) * (chart_height - padding_top - padding_bottom));
            if (first_sma5 == -1) {
                MoveToEx(hdcMem, x, y, NULL);
                first_sma5 = 1;
            } else {
                LineTo(hdcMem, x, y);
            }
        }

        // --- Draw SMA20 line ---
        SelectObject(hdcMem, sma20Pen);
        int first_sma20 = -1;
        for (int i = 0; i < num_points; i++) {
            double s20 = calculate_sma(stock, start_idx + i, 20);
            if (s20 <= 0.0) continue;
            int x = padding_left + (int)((double)i / (num_points - 1) * (chart_width - padding_left - padding_right));
            double ratio = (s20 - min_val) / (max_val - min_val);
            int y = padding_top + (int)((1.0 - ratio) * (chart_height - padding_top - padding_bottom));
            if (first_sma20 == -1) {
                MoveToEx(hdcMem, x, y, NULL);
                first_sma20 = 1;
            } else {
                LineTo(hdcMem, x, y);
            }
        }

        // --- Draw axes ---
        SelectObject(hdcMem, GetStockObject(BLACK_PEN));
        // Y axis
        MoveToEx(hdcMem, padding_left, padding_top, NULL);
        LineTo(hdcMem, padding_left, chart_height - padding_bottom);
        // X axis
        MoveToEx(hdcMem, padding_left, chart_height - padding_bottom, NULL);
        LineTo(hdcMem, chart_width - padding_right, chart_height - padding_bottom);

        // --- Y axis labels ---
        HFONT axisFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                     OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                     FIXED_PITCH | FF_MODERN, "Courier New");
        SelectObject(hdcMem, axisFont);
        SetBkMode(hdcMem, TRANSPARENT);
        SetTextColor(hdcMem, RGB(60, 60, 60));

        char label[32];
        snprintf(label, sizeof(label), "%.2f", max_val);
        TextOutA(hdcMem, 2, padding_top, label, (int)strlen(label));
        snprintf(label, sizeof(label), "%.2f", (max_val + min_val) / 2.0);
        TextOutA(hdcMem, 2, padding_top + (chart_height - padding_top - padding_bottom) / 2 - 7, label, (int)strlen(label));
        snprintf(label, sizeof(label), "%.2f", min_val);
        TextOutA(hdcMem, 2, chart_height - padding_bottom - 14, label, (int)strlen(label));

        // --- Title ---
        HFONT titleFont = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                      FIXED_PITCH | FF_MODERN, "Courier New");
        SelectObject(hdcMem, titleFont);
        SetTextColor(hdcMem, RGB(20, 20, 20));
        char title[128];
        snprintf(title, sizeof(title), "%s Closing Price Chart (%d days)", stock->ticker, num_points);
        TextOutA(hdcMem, padding_left + 10, 8, title, (int)strlen(title));

        // --- Legend ---
        SelectObject(hdcMem, axisFont);
        SetTextColor(hdcMem, RGB(30, 100, 200));
        TextOutA(hdcMem, chart_width - padding_right - 250, chart_height - padding_bottom + 12, "Close Price (solid)", 18);
        SelectObject(hdcMem, sma5Pen);
        MoveToEx(hdcMem, chart_width - padding_right - 280, chart_height - padding_bottom + 28, NULL);
        LineTo(hdcMem, chart_width - padding_right - 260, chart_height - padding_bottom + 28);
        SelectObject(hdcMem, axisFont);
        SetTextColor(hdcMem, RGB(50, 180, 50));
        TextOutA(hdcMem, chart_width - padding_right - 130, chart_height - padding_bottom + 12, "SMA5 (dashed)", 13);
        SelectObject(hdcMem, sma20Pen);
        MoveToEx(hdcMem, chart_width - padding_right - 160, chart_height - padding_bottom + 28, NULL);
        LineTo(hdcMem, chart_width - padding_right - 140, chart_height - padding_bottom + 28);

        // --- Date labels at bottom ---
        SelectObject(hdcMem, axisFont);
        SetTextColor(hdcMem, RGB(100, 100, 100));
        char start_date[32], end_date[32];
        snprintf(start_date, sizeof(start_date), "Start: %s", stock->records[start_idx].date);
        snprintf(end_date, sizeof(end_date), "End: %s", stock->records[stock->record_count - 1].date);
        TextOutA(hdcMem, padding_left, chart_height - padding_bottom + 12, start_date, (int)strlen(start_date));
        TextOutA(hdcMem, chart_width - padding_right - 110, chart_height - padding_bottom + 12, end_date, (int)strlen(end_date));

        // --- Cleanup ---
        DeleteObject(pricePen);
        DeleteObject(sma5Pen);
        DeleteObject(sma20Pen);
        DeleteObject(axisFont);
        DeleteObject(titleFont);
        SelectObject(hdcMem, hBitmap);

        // --- Save as BMP ---
        BITMAP bmp;
        GetObject(hBitmap, sizeof(bmp), &bmp);

        BITMAPFILEHEADER bmfh = {0};
        BITMAPINFOHEADER bmih = {0};

        bmfh.bfType = 0x4D42;  // "BM"
        bmfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bmih.biSize = sizeof(BITMAPINFOHEADER);
        bmih.biWidth = bmp.bmWidth;
        bmih.biHeight = bmp.bmHeight;
        bmih.biPlanes = 1;
        bmih.biBitCount = 32;
        bmih.biCompression = BI_RGB;
        bmih.biSizeImage = bmp.bmWidth * bmp.bmHeight * 4;

        bmfh.bfSize = bmfh.bfOffBits + bmih.biSizeImage;

        char bmp_path[64];
        snprintf(bmp_path, sizeof(bmp_path), "exports/%s_chart.bmp", stock->ticker);

        DWORD bytes_written;
        HANDLE hFile = CreateFileA(bmp_path, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            WriteFile(hFile, &bmfh, sizeof(bmfh), &bytes_written, NULL);
            WriteFile(hFile, &bmih, sizeof(bmih), &bytes_written, NULL);

            // Get pixel data
            DWORD pitch = bmp.bmWidth * 4;
            BYTE *pixels = (BYTE*)malloc(pitch * bmp.bmHeight);
            if (pixels) {
                GetBitmapBits(hBitmap, pitch * bmp.bmHeight, pixels);
                // BMP stores rows bottom-to-top, GDI stores top-to-bottom
                // GDI top-to-bottom is fine for 32-bit with no compression
                WriteFile(hFile, pixels, pitch * bmp.bmHeight, &bytes_written, NULL);
                free(pixels);
            }
            CloseHandle(hFile);
            printf("Chart saved: %s\n", bmp_path);
        }

        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DeleteObject(hBitmap);
    }
}
