CC ?= clang
CFLAGS = -O3 -Wall -Wextra -DNDEBUG -fPIC -I./src
LDFLAGS = -lz

# Support pkg-config for macOS brew paths if pkg-config is available
PKG_CONFIG ?= pkg-config
ifeq ($(shell command -v $(PKG_CONFIG) >/dev/null 2>&1; echo $$?), 0)
    PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags liblz4 zlib libllhttp 2>/dev/null)
    ifneq ($(PKG_CFLAGS),)
        CFLAGS += $(PKG_CFLAGS)
    endif
    PKG_LDFLAGS := $(shell $(PKG_CONFIG) --libs liblz4 zlib libllhttp 2>/dev/null)
    ifneq ($(PKG_LDFLAGS),)
        LDFLAGS = $(PKG_LDFLAGS)
    endif
endif

# If pkg-config fails or isn't used, we can add default Homebrew paths for macOS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # Check for arm64 brew or x86_64 brew
    ifeq ($(shell test -d /opt/homebrew && echo 1),1)
        CFLAGS += -I/opt/homebrew/include
        LDFLAGS += -L/opt/homebrew/lib
    else ifeq ($(shell test -d /usr/local/include && echo 1),1)
        CFLAGS += -I/usr/local/include
        LDFLAGS += -L/usr/local/lib
    endif
endif

SRC_FILES = src/main.c src/http_server.c src/auth.c src/flow_control.c src/compression.c src/json_handler.c src/yyjson.c src/llhttp.c src/api.c src/http.c src/lz4.c src/ae.c src/logging.c src/metrics.c
OBJ_FILES = $(SRC_FILES:.c=.o)
TARGET = uds_server

all: $(TARGET)

$(TARGET): $(OBJ_FILES)
	$(CC) $(OBJ_FILES) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)
