CXX ?= g++
CXXFLAGS ?= -O3 -march=native -std=c++17 -Wall -Wextra -pthread -fPIC -static-libstdc++ -static-libgcc

GLIB_CFLAGS := $(shell pkg-config --cflags gio-2.0 gobject-2.0 glib-2.0)
GLIB_LIBS := -l:libibus-1.0.so.5 $(shell pkg-config --libs gio-2.0 gobject-2.0 glib-2.0)

SRC_DIR := src
BIN_DIR := bin
LIB_DIR := lib

ENGINE_SRCS := $(SRC_DIR)/engine.cpp \
               $(SRC_DIR)/devanagari_data.cpp \
               $(SRC_DIR)/chromeos_deva_data.cpp \
               $(SRC_DIR)/chromeos_phonetic.cpp \
               $(SRC_DIR)/dict_trie.cpp \
               $(SRC_DIR)/emergency_api.cpp

ENGINE_DEPS := $(SRC_DIR)/engine.hpp \
               $(SRC_DIR)/devanagari_data.hpp \
               $(SRC_DIR)/chromeos_deva_data.hpp \
               $(SRC_DIR)/dict_trie.hpp \
               $(SRC_DIR)/emergency_api.hpp

all: $(BIN_DIR)/ibus-engine-google-input-tools $(LIB_DIR)/libgoogleinputtools.so $(BIN_DIR)/test_engine $(BIN_DIR)/git-cli

# Build C++ Shared Library
$(LIB_DIR)/libgoogleinputtools.so: $(ENGINE_SRCS) $(ENGINE_DEPS)
	@mkdir -p $(LIB_DIR)
	$(CXX) $(CXXFLAGS) -shared -o $@ $(ENGINE_SRCS)

# Build Native C++ IBus Input Method Engine (Pure daemon, zero GUI overhead)
$(BIN_DIR)/ibus-engine-google-input-tools: $(SRC_DIR)/ibus_main.cpp $(ENGINE_SRCS) $(ENGINE_DEPS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(GLIB_CFLAGS) $(SRC_DIR)/ibus_main.cpp $(ENGINE_SRCS) -o $@ $(GLIB_LIBS)

# Build Benchmark & Verification Binary
$(BIN_DIR)/test_engine: $(SRC_DIR)/benchmark.cpp $(ENGINE_SRCS) $(ENGINE_DEPS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/benchmark.cpp $(ENGINE_SRCS) -o $@

# Build Pure CLI Interactive & Batch Typing Tool (Zero GUI)
$(BIN_DIR)/git-cli: $(SRC_DIR)/cli_main.cpp $(ENGINE_SRCS) $(ENGINE_DEPS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(SRC_DIR)/cli_main.cpp $(ENGINE_SRCS) -o $@

strip: all
	strip --strip-unneeded $(BIN_DIR)/ibus-engine-google-input-tools
	strip --strip-unneeded $(LIB_DIR)/libgoogleinputtools.so
	strip --strip-unneeded $(BIN_DIR)/test_engine
	strip --strip-unneeded $(BIN_DIR)/git-cli

clean:
	rm -rf $(LIB_DIR) $(BIN_DIR)/*

.PHONY: all clean strip
