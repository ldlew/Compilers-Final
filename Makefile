CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

TARGET = mtg_engine
SRCS = main.cpp tokenizer.cpp ability_parser.cpp parser.cpp engine.cpp
OBJS = $(SRCS:.cpp=.o)
HEADERS = tokenizer.h ability_parser.h parser.h engine.h types.h

INPUT_FILE = data/input.json

CLANG_TIDY ?= clang-tidy
CLANG_FORMAT ?= clang-format
CLANG_TIDY_CHECKS ?= '-*,clang-analyzer-*,performance-*,readability-*,bugprone-*,modernize-*'
CLANG_TIDY_COMPILE_FLAGS ?= -std=c++17 -x c++

TARGET_FILES := $(filter-out lint fix format,$(MAKECMDGOALS))
TARGET_SRCS := $(wildcard $(addsuffix .cpp,$(TARGET_FILES)))
TARGET_HDRS := $(wildcard $(addsuffix .h,$(TARGET_FILES)))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: all
	./$(TARGET) $(INPUT_FILE)

run-debug: all
	./$(TARGET) --debug $(INPUT_FILE)

lint:
ifeq ($(TARGET_FILES),)
	$(CLANG_TIDY) $(SRCS) --checks=$(CLANG_TIDY_CHECKS) -- $(CLANG_TIDY_COMPILE_FLAGS)
else
	$(CLANG_TIDY) $(TARGET_SRCS) $(TARGET_HDRS) --checks=$(CLANG_TIDY_CHECKS) -- $(CLANG_TIDY_COMPILE_FLAGS)
endif

fix:
ifeq ($(TARGET_FILES),)
	$(CLANG_TIDY) --fix $(SRCS) --checks=$(CLANG_TIDY_CHECKS) -- $(CLANG_TIDY_COMPILE_FLAGS)
else
	$(CLANG_TIDY) --fix $(TARGET_SRCS) $(TARGET_HDRS) --checks=$(CLANG_TIDY_CHECKS) -- $(CLANG_TIDY_COMPILE_FLAGS)
endif

format:
ifeq ($(TARGET_FILES),)
	$(CLANG_FORMAT) -i $(SRCS) $(HEADERS)
else
	$(CLANG_FORMAT) -i $(TARGET_SRCS) $(TARGET_HDRS)
endif

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all run run-debug lint fix format clean
