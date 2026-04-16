CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic
SRC = src
TARGET = benchmark

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)/benchmark.cpp $(SRC)/*.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)/benchmark.cpp

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) results.csv latex_coords.txt