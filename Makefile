CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2
INCLUDES := -Iinclude
TARGET := shm_migrate_demo
SRCS := src/main.cpp
OBJS := $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
