CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -std=c11 -Iinclude
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude

GTK_CFLAGS = $(shell pkg-config --cflags gtk4)
GTK_LIBS = $(shell pkg-config --libs gtk4)

LDFLAGS = -lm

TARGET = wav_hist
GUI_TARGET = wav_app

SRC = src/wav.c src/histogram.c
PROGRAM = programs/wav_hist.c

OBJ = $(SRC:.c=.o) $(PROGRAM:.c=.o)

GUI_SRC = gui/main.cpp gui/app.cpp gui/histogram_view.cpp
GUI_OBJ = $(GUI_SRC:.cpp=.o)


all: $(TARGET) $(GUI_TARGET)


$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)


$(GUI_TARGET): $(GUI_OBJ) src/wav.o src/histogram.o
	$(CXX) -o $@ $(GUI_OBJ) src/wav.o src/histogram.o $(GTK_LIBS) $(LDFLAGS)


src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@


programs/%.o: programs/%.c
	$(CC) $(CFLAGS) -c $< -o $@


gui/%.o: gui/%.cpp
	$(CXX) $(CXXFLAGS) $(GTK_CFLAGS) -c $< -o $@


clean:
	rm -f $(OBJ) $(GUI_OBJ)
	rm -f $(TARGET) $(GUI_TARGET)
	rm -f output/*.png