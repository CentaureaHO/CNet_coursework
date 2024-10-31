SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

CXX ?= g++

INCLUDES = -I./include -I../utils/include

CXX_STANDARD = -std=c++20
DBGFLAGS = -g
WERROR_FLAGS := -Wall -Wextra -Wpedantic # -Werror
WARNINGS_IGNORE := -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-value

ifeq ($(OS),Windows_NT)
    LDFLAGS := -L../utils/lib/win/boost -lws2_32 -lpthread -lmswsock
    MKDIR := powershell -Command "New-Item -ItemType Directory -Force"
    RMDIR := powershell -Command "Remove-Item -Recurse -Force"
    RM := del /Q
    SEP := /
    SOURCES := src/http_message.cpp src/http_server.cpp src/socket_defs.cpp
else
    LDFLAGS := -L../utils/lib/linux/boost 
    MKDIR := mkdir -p
    RMDIR := rm -rf
    RM := rm -f
    SEP := /
    SOURCES := $(shell find $(SRC_DIR) -name '*.cpp' -o -name '*.c')
endif

CXXFLAGS = $(CXX_STANDARD) $(INCLUDES) $(WERROR_FLAGS) $(DBGFLAGS) $(WARNINGS_IGNORE)

OBJECTS := $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(SOURCES:.cpp=.o))

TEST_SOURCES := test.cpp
TEST_EXEC = test

.PHONY: all
all: obj server_boost

.PHONY: obj
obj: $(OBJECTS)
$(OBJ_DIR)$(SEP)%.o: $(SRC_DIR)$(SEP)%.cpp
	$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)$(SEP)%.o: $(SRC_DIR)$(SEP)%.c
	$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: server
server: $(OBJECTS)
	$(MKDIR) $(BIN_DIR)
	$(CXX) http_server.cpp $(OBJECTS) -o $(BIN_DIR)$(SEP)server $(CXXFLAGS) $(LDFLAGS)

.PHONY: server_boost
server_boost: $(OBJECTS)
	$(MKDIR) $(BIN_DIR)
	$(CXX) http_server_boost.cpp -o $(BIN_DIR)$(SEP)server_boost $(CXXFLAGS) $(LDFLAGS)

.PHONY: test
test: $(TEST_SOURCES) $(OBJECTS)
	$(CXX) $(OBJECTS) $(TEST_SOURCES) -o $(TEST_EXEC) $(CXXFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	-$(RMDIR) $(OBJ_DIR)
	-$(RMDIR) $(BIN_DIR)
	-$(RM) $(TEST_EXEC)

.PHONY: print-objects
print-objects:
	@echo OBJECTS: $(OBJECTS)