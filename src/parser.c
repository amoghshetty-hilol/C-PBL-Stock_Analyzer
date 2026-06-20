#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_and_clean(char *str) {
    char *start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n' || *start == '\"') {
        start++;
    }
    int len = strlen(start);
    while (len > 0 && (start[len - 1] == ' ' || start[len - 1] == '\t' || start[len - 1] == '\r' || start[len - 1] == '\n' || start[len - 1] == '\"')) {
        start[--len] = '\0';
    }
    if (start != str) {
        memmove(str, start, len + 1);
    }
}

Stock* load_stock_from_csv(const char *filepath, const char *ticker) {
    char clean_path[512];
    char clean_ticker[32];

    strncpy(clean_path, filepath, sizeof(clean_path) - 1);
    clean_path[sizeof(clean_path) - 1] = '\0';
    trim_and_clean(clean_path);

    strncpy(clean_ticker, ticker, sizeof(clean_ticker) - 1);
    clean_ticker[sizeof(clean_ticker) - 1] = '\0';
    trim_and_clean(clean_ticker);

    FILE *file = fopen(clean_path, "r");
    if (!file) {
        return NULL;
    }

    Stock *stock = (Stock*)malloc(sizeof(Stock));
    if (!stock) {
        fclose(file);
        return NULL;
    }

    strncpy(stock->ticker, clean_ticker, sizeof(stock->ticker) - 1);
    stock->ticker[sizeof(stock->ticker) - 1] = '\0';
    stock->capacity = INITIAL_CAPACITY;
    stock->record_count = 0;
    stock->records = (TradeRecord*)malloc(stock->capacity * sizeof(TradeRecord));
    if (!stock->records) {
        free(stock);
        fclose(file);
        return NULL;
    }

    char line[512];
    int is_header = 1;

    while (fgets(line, sizeof(line), file)) {
        if (is_header) {
            is_header = 0;
            continue; 
        }

        char *token;
        char *tokens[7];
        int token_count = 0;

        token = strtok(line, ",");
        while (token && token_count < 7) {
            tokens[token_count++] = token;
            token = strtok(NULL, ",");
        }

        if (token_count < 7) {
            continue; 
        }

        for (int i = 0; i < 7; i++) {
            trim_and_clean(tokens[i]);
        }

        if (stock->record_count >= stock->capacity) {
            int new_capacity = stock->capacity * 2;
            TradeRecord *new_records = (TradeRecord*)realloc(stock->records, new_capacity * sizeof(TradeRecord));
            if (!new_records) {
                free(stock->records);
                free(stock);
                fclose(file);
                return NULL;
            }
            stock->records = new_records;
            stock->capacity = new_capacity;
        }

        TradeRecord *rec = &stock->records[stock->record_count];
        strncpy(rec->date, tokens[0], 10);
        rec->date[10] = '\0';

        rec->open = strtod(tokens[1], NULL);
        rec->high = strtod(tokens[2], NULL);
        rec->low = strtod(tokens[3], NULL);
        rec->close = strtod(tokens[4], NULL);
        rec->volume = strtoul(tokens[6], NULL, 10);

        stock->record_count++;
    }

    fclose(file);

    if (stock->record_count == 0) {
        free(stock->records);
        free(stock);
        return NULL;
    }

    return stock;
}

void free_stock(Stock *stock) {
    if (stock) {
        if (stock->records) {
            free(stock->records);
        }
        free(stock);
    }
}

void free_market(Market *market) {
    if (market) {
        if (market->stocks) {
            for (int i = 0; i < market->stock_count; i++) {
                if (market->stocks[i]) {
                    free_stock(market->stocks[i]);
                }
            }
            free(market->stocks);
        }
        free(market);
    }
}
