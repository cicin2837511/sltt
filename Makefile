CFLAGS = -lSDL3 -lSDL3_ttf -Wall -Wextra -std=c99 -pedantic -lfontconfig -DVERSION='"0.0.1"'
SRC = ./src
TARGET = ./target

sltt: $(SRC)/main.c | $(TARGET)
	gcc $(CFLAGS) -o $(TARGET)/sltt $(SRC)/main.c

$(TARGET):
	mkdir ./target

run: sltt
	$(TARGET)/sltt

.PHONY: run
