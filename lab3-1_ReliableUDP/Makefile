INCLUDE_DIR := ./include
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
    LDFLAGS := -lws2_32
    MKDIR := powershell -Command "New-Item -ItemType Directory -Force"
    RMDIR := powershell -Command "Remove-Item -Recurse -Force"
    RM := del /Q
    SEP := /
    COMMON_SOURCES := src/net/socket_defs.cpp src/net/nb_socket.cpp
else
    LDFLAGS := 
    MKDIR := mkdir -p
    RMDIR := rm -rf
    RM := rm -f
    SEP := /
    COMMON_SOURCES := $(shell find $(SRC_DIR)/common -name '*.cpp' -o -name '*.c')
	COMMON_SOURCES += $(shell find $(SRC_DIR)/net -name '*.cpp' -o -name '*.c')
	TRANSFER_SOURCES := $(shell find $(SRC_DIR)/transfer -name '*.cpp' -o -name '*.c')
endif

CXXFLAGS = $(CXX_STANDARD) $(INCLUDES) $(WERROR_FLAGS) $(DBGFLAGS) $(WARNINGS_IGNORE)

COMMON_OBJECTS := $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(COMMON_SOURCES:.cpp=.o))
TRANSFER_OBJECTS := $(patsubst $(SRC_DIR)/%, $(OBJ_DIR)/%, $(TRANSFER_SOURCES:.cpp=.o))

TEST_SOURCES := test.cpp
TEST_EXEC = test

SERVER_SOURCES := server.cpp
SERVER_EXEC = server

CLIENT_SOURCES := client.cpp
CLIENT_EXEC = client

.PHONY: all
all: obj

.PHONY: obj
obj: $(COMMON_OBJECTS) $(TRANSFER_OBJECTS)
$(OBJ_DIR)$(SEP)%.o: $(SRC_DIR)$(SEP)%.cpp
	$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR)$(SEP)%.o: $(SRC_DIR)$(SEP)%.c
	$(MKDIR) $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: server
test: $(SERVER_SOURCES) $(COMMON_OBJECTS)
$(SERVER_EXEC): $(SERVER_SOURCES) $(COMMON_OBJECTS)
	$(CXX) $(COMMON_OBJECTS) $(SERVER_SOURCES) -o $(SERVER_EXEC) $(CXXFLAGS) $(LDFLAGS)

.PHONY: client
client: $(CLIENT_SOURCES) $(COMMON_OBJECTS)
$(CLIENT_EXEC): $(CLIENT_SOURCES) $(COMMON_OBJECTS)
	$(CXX) $(COMMON_OBJECTS) $(CLIENT_SOURCES) -o $(CLIENT_EXEC) $(CXXFLAGS) $(LDFLAGS)

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
	@echo TRANSFER_OBJECTS: $(TRANSFER_OBJECTS)