# Makefile - hệ thống mô phỏng chăm sóc vườn cây tự động
#
#   make             build device_sim, controller (nếu đã có main.c)
#   make test        build và chạy mọi test trong tests/unit và tests/integration
#   make coverage    chạy test với gcov và xuất báo cáo (cần: sudo apt install gcovr)
#   make loadtest    build và chạy tests/load/loadtest.c
#   make clean       xóa thư mục build/
#   make help        in trợ giúp
#
# Thêm một module mới chỉ cần tạo file .c trong src/common, src/device hoặc src/controller
# và file test tests/unit/test_<tên>.c. Makefile tự nhận, không cần sửa.

CC      ?= gcc
COV     ?= 0
COV_MIN ?= 0

# _POSIX_C_SOURCE cho poll/getaddrinfo/..., _DEFAULT_SOURCE cho các hàm BSD (vd: usleep)
CFLAGS  := -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
           -Wall -Wextra -Wpedantic -Wshadow -Iinclude -Itests -MMD -MP
LDFLAGS :=
LDLIBS  :=

ifeq ($(COV),1)
BUILD   := build/cov
CFLAGS  += -O0 -g --coverage
LDFLAGS += --coverage
else
BUILD   := build
CFLAGS  += -O2 -g
endif

COMMON_SRC := $(wildcard src/common/*.c)
DEVICE_SRC := $(filter-out src/device/main.c,$(wildcard src/device/*.c))
CTRL_SRC   := $(filter-out src/controller/main.c,$(wildcard src/controller/*.c))
LIB_SRC    := $(COMMON_SRC) $(DEVICE_SRC) $(CTRL_SRC)
LIB_OBJ    := $(LIB_SRC:%.c=$(BUILD)/%.o)

UNIT_SRC   := $(wildcard tests/unit/test_*.c)
UNIT_BIN   := $(UNIT_SRC:%.c=$(BUILD)/%)
INTEG_SRC  := $(wildcard tests/integration/test_*.c)
INTEG_BIN  := $(INTEG_SRC:%.c=$(BUILD)/%)

PROGS :=
ifneq ($(wildcard src/device/main.c),)
PROGS += $(BUILD)/device_sim
endif
ifneq ($(wildcard src/controller/main.c),)
PROGS += $(BUILD)/controller
endif

.PHONY: all test coverage loadtest clean clean-cov help

all: $(PROGS)

$(BUILD)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/device_sim: src/device/main.c $(LIB_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/controller: src/controller/main.c $(LIB_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# Mỗi file tests/{unit,integration}/test_*.c thành một chương trình test độc lập
$(BUILD)/tests/unit/test_%: tests/unit/test_%.c $(LIB_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(BUILD)/tests/integration/test_%: tests/integration/test_%.c $(LIB_OBJ)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

test: $(PROGS) $(UNIT_BIN) $(INTEG_BIN)
	@fail=0; \
	for t in $(UNIT_BIN) $(INTEG_BIN); do \
	    echo "== $$t"; \
	    ./$$t || fail=1; \
	done; \
	if [ $$fail -ne 0 ]; then echo "CÓ TEST HỎNG"; exit 1; fi; \
	echo "TẤT CẢ TEST ĐẠT"

coverage:
	$(MAKE) COV=1 clean-cov
	$(MAKE) COV=1 test
	@mkdir -p build/coverage
	gcovr -r . --object-directory build/cov --filter 'src/' \
	    --txt --html-details build/coverage/index.html \
	    --fail-under-line $(COV_MIN)
	@echo "Báo cáo HTML: build/coverage/index.html"

clean-cov:
	rm -rf build/cov build/coverage

loadtest: $(PROGS)
	@if [ -f tests/load/loadtest.c ]; then \
	    mkdir -p build/tests/load; \
	    $(CC) $(CFLAGS) -pthread tests/load/loadtest.c $(LIB_SRC) -o build/tests/load/loadtest $(LDLIBS) && \
	    ./build/tests/load/loadtest; \
	else \
	    echo "Chưa có tests/load/loadtest.c"; \
	fi

clean:
	rm -rf build

help:
	@sed -n '2,9p' Makefile

-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
