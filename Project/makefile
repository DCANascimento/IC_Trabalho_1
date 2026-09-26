CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -std=c11 -Iinclude
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude

GTK_CFLAGS = $(shell pkg-config --cflags gtk4)
GTK_LIBS = $(shell pkg-config --libs gtk4)

LDFLAGS = -lm

TARGET = wav_hist
QUANT_TARGET = wav_quant
GUI_TARGET = wav_app

SRC = src/wav.c src/histogram.c src/quantization.c
HIST_PROGRAM = programs/wav_hist.c
QUANT_PROGRAM = programs/wav_quant.c

SRC_OBJ = $(SRC:.c=.o)
HIST_OBJ = $(HIST_PROGRAM:.c=.o)
QUANT_OBJ = $(QUANT_PROGRAM:.c=.o)

GUI_SRC = gui/main.cpp gui/app.cpp gui/histogram_view.cpp
GUI_OBJ = $(GUI_SRC:.cpp=.o)


all: $(TARGET) $(QUANT_TARGET) $(GUI_TARGET)


$(TARGET): $(SRC_OBJ) $(HIST_OBJ)
	$(CC) $(CFLAGS) -o $@ $(SRC_OBJ) $(HIST_OBJ) $(LDFLAGS)


$(QUANT_TARGET): src/wav.o src/quantization.o $(QUANT_OBJ)
	$(CC) $(CFLAGS) -o $@ src/wav.o src/quantization.o $(QUANT_OBJ) $(LDFLAGS)


$(GUI_TARGET): $(GUI_OBJ) src/wav.o src/histogram.o src/quantization.o
	$(CXX) -o $@ $(GUI_OBJ) src/wav.o src/histogram.o src/quantization.o $(GTK_LIBS) $(LDFLAGS)


src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@


programs/%.o: programs/%.c
	$(CC) $(CFLAGS) -c $< -o $@


gui/%.o: gui/%.cpp
	$(CXX) $(CXXFLAGS) $(GTK_CFLAGS) -c $< -o $@


clean:
	rm -f $(SRC_OBJ) $(HIST_OBJ) $(QUANT_OBJ) $(GUI_OBJ)
	rm -f $(TARGET) $(QUANT_TARGET) $(GUI_TARGET)
	rm -f output/*.png