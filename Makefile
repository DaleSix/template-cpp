CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2
INCLUDES := -Iinclude
PYTHON := python3
CLANG := clang
GEN := tools/gen_shm_meta.py
STRUCTS_HEADER := include/shm_structs.h
GEN_HEADER := include/shm_meta_generated.h
TARGET := shm_migrate_demo
SRCS := src/main.cpp
OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

gen: $(GEN_HEADER)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

$(GEN_HEADER): $(STRUCTS_HEADER) $(GEN)
	$(PYTHON) $(GEN) --input $(STRUCTS_HEADER) --output $(GEN_HEADER) --clang $(CLANG) --clang-args -std=c++17 -Iinclude

%.o: %.cpp $(GEN_HEADER)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) $(GEN_HEADER)

.PHONY: all clean gen
