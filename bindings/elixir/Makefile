# Makefile for building the DSQLEX NIF
.PHONY: all clean

# Erlang/Elixir paths
ERTS_INCLUDE_DIR ?= $(shell erl -noshell -eval 'io:format("~s/erts-~s/include", [code:root_dir(), erlang:system_info(version)])' -s init stop)

# mpdecimal paths (via pkg-config)
MPDEC_CFLAGS := $(shell pkg-config --cflags libmpdec libmpdec++ 2>/dev/null)

# DSQLEX C library paths
DSQLEX_ROOT := $(abspath ../..)
DSQLEX_INCLUDE := $(DSQLEX_ROOT)/include
DSQLEX_BUILD := $(DSQLEX_ROOT)/build

CXX := c++
CXXFLAGS := -std=c++17 -O2 -fPIC -shared \
            -I$(ERTS_INCLUDE_DIR) \
            -I$(DSQLEX_INCLUDE) \
            $(MPDEC_CFLAGS) \
            -undefined dynamic_lookup

LDFLAGS := -L$(DSQLEX_BUILD) -ldsqlex -Wl,-rpath,$(DSQLEX_BUILD)

# Output
PRIV_DIR := priv
NIF_SO := $(PRIV_DIR)/dsqlex_nif.so

all: $(NIF_SO)

$(PRIV_DIR):
	mkdir -p $(PRIV_DIR)

$(NIF_SO): c_src/dsqlex_nif.cpp | $(PRIV_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -rf $(PRIV_DIR)
