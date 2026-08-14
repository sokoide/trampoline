# x86_64 cooperative threading sample.
# macOS は OrbStack の x64 Linux、ARM Linux は qemu-user、x86_64 Linux は native。

HOST_OS := $(shell uname -s)
HOST_ARCH := $(shell uname -m)
ORBSTACK := $(shell command -v orbctl 2>/dev/null)
NATIVE_X64 := $(if $(and $(filter Linux,$(HOST_OS)),$(filter x86_64,$(HOST_ARCH))),1,)
CROSS_QEMU := $(if $(and $(filter Linux,$(HOST_OS)),$(filter-out x86_64,$(HOST_ARCH)),$(shell command -v x86_64-linux-gnu-gcc 2>/dev/null),$(shell command -v qemu-x86_64-static 2>/dev/null)),1,)
LINUX_MACHINE := x64-linux-env

.DEFAULT_GOAL := help
.PHONY: all build run clean help linux-machines

ifeq ($(NATIVE_X64),1)
CC := cc
RUN :=
ROUTE := native (x86_64 Linux)
else ifneq ($(ORBSTACK),)
ROUTE := OrbStack ($(LINUX_MACHINE))
else ifeq ($(CROSS_QEMU),1)
CC := x86_64-linux-gnu-gcc
RUN := qemu-x86_64-static -L /usr/x86_64-linux-gnu
ROUTE := qemu-user (x86_64 on $(HOST_ARCH))
else
$(error 対応する実行経路がありません: x86_64 Linux、OrbStack、または ARM Linux の qemu-user toolchain が必要です)
endif

ifeq ($(NATIVE_X64),1)
trampolin_sample: st.c ctx.S main.c st.h ctx.h internal.h safe_helpers.h
	$(CC) -std=c11 -Wall -Wextra -O2 -g $^ -o $@
build: trampolin_sample
run: trampolin_sample
	@echo "[route: $(ROUTE)]"
	./trampolin_sample
else ifeq ($(CROSS_QEMU),1)
trampolin_sample: st.c ctx.S main.c st.h ctx.h internal.h safe_helpers.h
	$(CC) -std=c11 -Wall -Wextra -O2 -g $^ -o $@
build: trampolin_sample
run: trampolin_sample
	@echo "[route: $(ROUTE)]"
	$(RUN) ./trampolin_sample
else
build run:
	@echo "[route: $(ROUTE)]"
	@scripts/in-linux.sh $(LINUX_MACHINE) "make NATIVE_X64=1 $@"
endif

all: build

linux-machines:
	@if orbctl list 2>/dev/null | awk '{print $$1}' | grep -qx '$(LINUX_MACHINE)'; then \
		echo '[skip] machine $(LINUX_MACHINE) already exists'; \
	else \
		echo '[create] machine $(LINUX_MACHINE) (amd64)'; \
		orbctl create -a amd64 ubuntu:24.04 $(LINUX_MACHINE); \
	fi

clean:
	rm -f trampolin_sample

help:
	@echo 'make build  x86_64 バイナリをビルド'
	@echo 'make run    A/B/C が永遠に yield するサンプルを実行'
	@echo 'make linux-machines  OrbStack の x64 VM を作成'
