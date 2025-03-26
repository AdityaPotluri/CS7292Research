# Compiler
CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17

# Target executable
TARGET = OurCacheSimulator

# Source files
SRCS = main.cpp

# Object files
OBJS = $(SRCS:.cpp=.o)

# Build the executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

# Compile source files into object files
%.o: %.cpp Cache.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
