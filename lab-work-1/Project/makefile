CC = gcc
CXX = g++

CFLAGS = -Wall -Wextra -std=c11 -Iinclude
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude

GTK_CFLAGS = $(shell pkg-config --cflags gtk4)
GTK_LIBS = $(shell pkg-config --libs gtk4)

LDFLAGS = -lm

TARGET = wav_hist
QUANT_TARGET = wav_quant
CMP_TARGET = wav_cmp
EFFECTS_TARGET = wav_effects
GUI_TARGET = wav_app

SRC = src/wav.cpp src/histogram.cpp src/quantization.cpp src/comparison.cpp src/effects.cpp

HIST_PROGRAM = programs/wav_hist.cpp
QUANT_PROGRAM = programs/wav_quant.cpp
CMP_PROGRAM = programs/wav_cmp.cpp
EFFECTS_PROGRAM = programs/wav_effects.cpp

SRC_OBJ = $(SRC:.cpp=.o)
HIST_OBJ = $(HIST_PROGRAM:.cpp=.o)
QUANT_OBJ = $(QUANT_PROGRAM:.cpp=.o)
CMP_OBJ = $(CMP_PROGRAM:.cpp=.o)
EFFECTS_OBJ = $(EFFECTS_PROGRAM:.cpp=.o)

GUI_SRC = gui/main.cpp gui/app.cpp gui/histogram_view.cpp
GUI_OBJ = $(GUI_SRC:.cpp=.o)


all: $(TARGET) $(QUANT_TARGET) $(CMP_TARGET) $(EFFECTS_TARGET) $(GUI_TARGET)


$(TARGET): src/wav.o src/histogram.o $(HIST_OBJ)
	$(CXX) -o $@ src/wav.o src/histogram.o $(HIST_OBJ) $(LDFLAGS)


$(QUANT_TARGET): src/wav.o src/quantization.o $(QUANT_OBJ)
	$(CXX) -o $@ src/wav.o src/quantization.o $(QUANT_OBJ) $(LDFLAGS)


$(CMP_TARGET): src/wav.o src/comparison.o $(CMP_OBJ)
	$(CXX) -o $@ src/wav.o src/comparison.o $(CMP_OBJ) $(LDFLAGS)


$(EFFECTS_TARGET): src/wav.o src/effects.o $(EFFECTS_OBJ)
	$(CXX) -o $@ src/wav.o src/effects.o $(EFFECTS_OBJ) $(LDFLAGS)


$(GUI_TARGET): $(GUI_OBJ) src/wav.o src/histogram.o src/quantization.o src/comparison.o src/effects.o
	$(CXX) -o $@ $(GUI_OBJ) src/wav.o src/histogram.o src/quantization.o src/comparison.o src/effects.o $(GTK_LIBS) $(LDFLAGS)


src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


programs/%.o: programs/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


gui/%.o: gui/%.cpp
	$(CXX) $(CXXFLAGS) $(GTK_CFLAGS) -c $< -o $@


clean:
	rm -f $(SRC_OBJ) $(HIST_OBJ) $(QUANT_OBJ) $(CMP_OBJ) $(EFFECTS_OBJ) $(GUI_OBJ)
	rm -f $(TARGET) $(QUANT_TARGET) $(CMP_TARGET) $(EFFECTS_TARGET) $(GUI_TARGET)
	rm -f output/*.png