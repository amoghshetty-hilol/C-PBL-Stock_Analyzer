CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
TARGET = stock.exe
SRCS = src/main.c src/parser.c src/analytics.c
OBJS = src/main.o src/parser.o src/analytics.o

.PHONY: all clean

all: $(TARGET)

# FIX: Appended -luser32 and -lgdi32 at the end of the linking instruction
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -luser32 -lgdi32

src/main.o: src/main.c include/common.h include/parser.h include/analytics.h
	$(CC) $(CFLAGS) -c src/main.c -o src/main.o

src/parser.o: src/parser.c include/parser.h include/common.h
	$(CC) $(CFLAGS) -c src/parser.c -o src/parser.o

src/analytics.o: src/analytics.c include/analytics.h include/common.h
	$(CC) $(CFLAGS) -c src/analytics.c -o src/analytics.o

clean:
	@del /q /f $(TARGET) src\*.o 2>nul || rm -f $(TARGET) src/*.o
