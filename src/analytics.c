#include "analytics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

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
        printf("No assets loaded in market database.\n");
        return;
    }

    qsort(market->stocks, market->stock_count, sizeof(Stock*), compare_performance);
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    
    SetConsoleTextAttribute(hConsole, 11);
    printf("=======================================================================\n");
    printf("                   30-DAY MARKET PERFORMANCE LEADERBOARD               \n");
    printf("=======================================================================\n");
    SetConsoleTextAttribute(hConsole, 7);

    printf("%-5s | %-10s | %-12s | %-15s | %-10s\n", "Rank", "Ticker", "Latest Close", "30d Growth %", "Signal");
    printf("-----------------------------------------------------------------------\n");

    for (int i = 0; i < market->stock_count; i++) {
        Stock *s = market->stocks[i];
        double latest_close = s->records[s->record_count - 1].close;
        double growth = calculate_growth_30d(s);
        char signal[SIGNAL_BUF];
        generate_enhanced_signal(s, signal, sizeof(signal));

        printf("%-5d | %-10s | $%-11.2f | ", i + 1, s->ticker, latest_close);

        if (growth > 0) {
            SetConsoleTextAttribute(hConsole, 2); 
            printf("+%-14.2f%%", growth);
        } else if (growth < 0) {
            SetConsoleTextAttribute(hConsole, 4); 
            printf("%-15.2f%%", growth);
        } else {
            SetConsoleTextAttribute(hConsole, 7); 
            printf("%-15.2f%%", growth);
        }
        SetConsoleTextAttribute(hConsole, 7);
        printf(" | ");

        if (strcmp(signal, "BUY") == 0) {
            SetConsoleTextAttribute(hConsole, 2); 
            printf("[BUY]");
        } else if (strcmp(signal, "SELL") == 0) {
            SetConsoleTextAttribute(hConsole, 4); 
            printf("[SELL]");
        } else {
            SetConsoleTextAttribute(hConsole, 7); 
            printf("[HOLD]");
        }
        SetConsoleTextAttribute(hConsole, 7);
        printf("\n");
    }
    printf("-----------------------------------------------------------------------\n");
}

void render_ascii_trend(const Stock *stock, int datapoints) {
    if (!stock || stock->record_count <= 0 || datapoints <= 0) {
        printf("No data to plot.\n");
        return;
    }

    int start_idx = stock->record_count - datapoints;
    if (start_idx < 0) start_idx = 0;
    int num_points = stock->record_count - start_idx;

    if (num_points <= 1) {
        printf("Not enough data to plot a trend.\n");
        return;
    }

    double min_val = stock->records[start_idx].close;
    double max_val = stock->records[start_idx].close;
    for (int i = 0; i < num_points; i++) {
        double val = stock->records[start_idx + i].close;
        if (val < min_val) min_val = val;
        if (val > max_val) max_val = val;
    }

    if (max_val == min_val) {
        min_val -= 1.0;
        max_val += 1.0;
    }

    char grid[ROWS][MAX_COLS];
    memset(grid, ' ', sizeof(grid));

    for (int i = 0; i < num_points; i++) {
        double val = stock->records[start_idx + i].close;
        double ratio = (val - min_val) / (max_val - min_val);
        int r = (ROWS - 1) - (int)(ratio * (ROWS - 1) + 0.5);
        if (r < 0) r = 0;
        if (r >= ROWS) r = ROWS - 1;
        grid[r][i] = '*';
    }

    printf("\n%s Closing Trend Graph (%d days):\n", stock->ticker, num_points);
    for (int r = 0; r < ROWS; r++) {
        if (r == 0) {
            printf("%9.2f | ", max_val);
        } else if (r == ROWS - 1) {
            printf("%9.2f | ", min_val);
        } else if (r == ROWS / 2) {
            printf("%9.2f | ", (max_val + min_val) / 2.0);
        } else {
            printf("          | ");
        }

        for (int c = 0; c < num_points; c++) {
            printf("%c", grid[r][c]);
        }
        printf("\n");
    }
    printf("          +");
    for (int i = 0; i < num_points; i++) {
        printf("-");
    }
    printf("\n");
    printf("           Date: %s to %s\n", stock->records[start_idx].date, stock->records[stock->record_count - 1].date);
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

void export_analytical_report(const Stock *stock) {
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
    fprintf(file, "========================================================\n");

    int datapoints = 40; 
    int start_idx = stock->record_count - datapoints;
    if (start_idx < 0) start_idx = 0;
    int num_points = stock->record_count - start_idx;

    if (num_points > 1) {
        double min_val = stock->records[start_idx].close;
        double max_val = stock->records[start_idx].close;
        for (int i = 0; i < num_points; i++) {
            double val = stock->records[start_idx + i].close;
            if (val < min_val) min_val = val;
            if (val > max_val) max_val = val;
        }
        if (max_val == min_val) {
            min_val -= 1.0;
            max_val += 1.0;
        }

        char grid[ROWS][MAX_COLS];
        memset(grid, ' ', sizeof(grid));

        for (int i = 0; i < num_points; i++) {
            double val = stock->records[start_idx + i].close;
            double ratio = (val - min_val) / (max_val - min_val);
            int r = (ROWS - 1) - (int)(ratio * (ROWS - 1) + 0.5);
            if (r < 0) r = 0;
            if (r >= ROWS) r = ROWS - 1;
            grid[r][i] = '*';
        }

        fprintf(file, "\n%s Closing Trend Graph (%d days):\n", stock->ticker, num_points);
        for (int r = 0; r < ROWS; r++) {
            if (r == 0) {
                fprintf(file, "%9.2f | ", max_val);
            } else if (r == ROWS - 1) {
                fprintf(file, "%9.2f | ", min_val);
            } else if (r == ROWS / 2) {
                fprintf(file, "%9.2f | ", (max_val + min_val) / 2.0);
            } else {
                fprintf(file, "          | ");
            }

            for (int c = 0; c < num_points; c++) {
                fprintf(file, "%c", grid[r][c]);
            }
            fprintf(file, "\n");
        }
        fprintf(file, "          +");
        for (int i = 0; i < num_points; i++) {
            fprintf(file, "-");
        }
        fprintf(file, "\n");
        fprintf(file, "           Date: %s to %s\n", stock->records[start_idx].date, stock->records[stock->record_count - 1].date);
        fprintf(file, "========================================================\n");
    }

    fclose(file);
    printf("Report successfully exported to: %s\n", filepath);
}
