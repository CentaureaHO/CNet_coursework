INCLUDE_DIR := ./include ../utils/include
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

SERVER := server
CLIENT := client
CXX ?= g++

INCLUDES = -I./include -I../utils/include

CXX_STANDARD = -std=c++20
DBGFLAGS = -g -static
WERROR_FLAGS := -Wall -Wextra -Wpedantic # -Werror
WARNINGS_IGNORE := -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-unused-value

ifeq ($(OS),Windows_NT)
    LDFLAGS := -L../utils/lib -lws2_32 -lpthread -lftxui-component-w -lftxui-dom-w -lftxui-screen-w
    MKDIR := powershell -Command "New-Item -ItemType Directory -Force"
    RMDIR := powershell -Command "Remove-Item -Recurse -Force"
    RM := del /Q
    SEP := /
    COMMON_SOURCES := src/common/net/socket_defs.cpp src/common/console/utf8_console.cpp src/common/thread/pool.cpp src/common/thread/lock.cpp
    SERVER_SOURCES := src/server/main.cpp src/server/net/message_dispatcher.cpp src/server/net/session_listener.cpp src/server/net/session_manager.cpp
    CLIENT_SOURCES := src/client/main.cpp
else
    LDFLAGS := -L../utils/lib -lftxui-component -lftxui-dom -lftxui-screen -lncurses
    MKDIR := mkdir -p
    RMDIR := rm -rf
    RM := rm -f
    SEP := /
    COMMON_SOURCES := $(shell find $(SRC_DIR)/common -name '*.cpp' -o -name '*.c')
    SERVER_SOURCES := $(shell find $(SRC_DIR)/server -name '*.cpp' -o -name '*.c')
    CLIENT_SOURCES := $(shell find $(SRC_DIR)/client -name '*.cpp' -o -name '*.c')
endif

CXXFLAGS = $(CXX_STANDARD) $(INCLUDES) $(WERROR_FLAGS) $(DBGFLAGS) $(WARNINGS_IGNORE)

COMMON_OBJECTS := $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(COMMON_SOURCES:.cpp=.o))
SERVER_OBJECTS := $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(SERVER_SOURCES:.cpp=.o))
CLIENT_OBJECTS := $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(CLIENT_SOURCES:.cpp=.o))

TEST_SOURCES := test.cpp
TEST_EXEC = test

.PHONY: all
all: obj server client

.PHONY: obj
obj: $(COMMON_OBJECTS) $(SERVER_OBJECTS) $(CLIENT_OBJECTS)
$(OBJ_DIR)$(SEP)%.o: $(SRC_DIR)$(SEP)%.cpp
	$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)$(SEP)%.o: $(SRC_DIR)$(SEP)%.c
	$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: server
server: $(COMMON_OBJECTS) $(SERVER_OBJECTS)
	$(MKDIR) $(BIN_DIR)
	$(CXX) $(COMMON_OBJECTS) $(SERVER_OBJECTS) -o $(BIN_DIR)/$(SERVER) $(CXXFLAGS) $(LDFLAGS)

.PHONY: client
client: $(COMMON_OBJECTS) $(CLIENT_OBJECTS)
	$(MKDIR) $(BIN_DIR)
	$(CXX) $(COMMON_OBJECTS) $(CLIENT_OBJECTS) -o $(BIN_DIR)/$(CLIENT) $(CXXFLAGS) $(LDFLAGS)

.PHONY: test
test: $(TEST_SOURCES) $(COMMON_OBJECTS)
$(TEST_EXEC): $(TEST_SOURCES) $(COMMON_OBJECTS)
	$(CXX) $(COMMON_OBJECTS) $(TEST_SOURCES) -o $(TEST_EXEC) $(CXXFLAGS) $(LDFLAGS)

.PHONY: clean
clean:
	-$(RMDIR) $(OBJ_DIR)
	-$(RMDIR) $(BIN_DIR)
	-$(RM) $(TEST_EXEC)

.PHONY: print-objects
print-objects:
	@echo COMMON_OBJECTS: $(COMMON_OBJECTS)
	@echo SERVER_OBJECTS: $(SERVER_OBJECTS)
	@echo CLIENT_OBJECTS: $(CLIENT_OBJECTS)