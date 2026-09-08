CC = clang
CFLAGS = -O2 -g -Wall -Wextra -Werror
MACFLAGS = -arch x86_64 -mmacosx-version-min=12.0
WINCC ?= x86_64-w64-mingw32-gcc
WINCXX ?= x86_64-w64-mingw32-g++

.PHONY: all test clean windows
all: build/libnop_bridge.dylib build/native_probe build/wine_bootstrap
build:
	mkdir -p build
build/libnop_bridge.dylib: src/bridge.c src/nop_decode.h src/priv_decode.h | build
	$(CC) $(CFLAGS) $(MACFLAGS) -std=c11 -dynamiclib $< -o $@.new
	mv $@.new $@
build/native_probe: tests/native_probe.c | build
	$(CC) $(CFLAGS) $(MACFLAGS) -mno-red-zone $< -o $@
build/wine_bootstrap: src/wine_bootstrap.c src/Info.plist | build
	mkdir -p build/NopBridgeLab.app/Contents/MacOS
	cp src/Info.plist build/NopBridgeLab.app/Contents/Info.plist
	$(CC) $(CFLAGS) $(MACFLAGS) $< -o build/NopBridgeLab.app/Contents/MacOS/wine_bootstrap.new -framework CoreFoundation -Wl,-no_pie,-image_base,0x200000000,-pagezero_size,0x1000,-no_huge,-no_fixup_chains,-segalign,0x1000,-segaddr,WINE_RESERVE,0x1000,-segaddr,WINE_TOP_DOWN,0x7ff000000000 -Wl,-exported_symbol,_wine_main_preload_info -Wl,-sectcreate,__TEXT,__info_plist,src/Info.plist
	mv build/NopBridgeLab.app/Contents/MacOS/wine_bootstrap.new build/NopBridgeLab.app/Contents/MacOS/wine_bootstrap
	ln -sf NopBridgeLab.app/Contents/MacOS/wine_bootstrap $@
build/decoder_test: tests/decoder_test.c src/nop_decode.h | build
	$(CC) $(CFLAGS) $< -o $@
test: all build/decoder_test
	python3 scripts/test_native.py
windows: build/windows_probe.exe build/windows_early.exe
build/windows_probe.exe: tests/windows_probe.c | build
	$(WINCC) $(CFLAGS) -static $< -o $@
build/early.dll: tests/windows_early.c | build
	$(WINCC) $(CFLAGS) -static -shared -DBUILD_EARLY_DLL $< -Wl,--out-implib,build/libearly.a -o $@
build/windows_early.exe: tests/windows_early.c build/early.dll | build
	$(WINCC) $(CFLAGS) -static $< -Lbuild -learly -o $@
build/shared_texture.exe: tests/shared_texture.cpp | build
	$(WINCXX) $(CFLAGS) -static $< -o $@ -ld3d11 -ldxgi -luuid
build/guarded_mutex.exe: tests/guarded_mutex.c | build
	$(WINCC) $(CFLAGS) -static $< -o $@
build/video_reader.exe: tests/video_reader.cpp | build
	$(WINCXX) $(CFLAGS) -static -municode $< -o $@ -ld3d11 -lmfreadwrite -lmfplat -lmfuuid -lole32 -luuid
build/process_image_name.exe: tests/process_image_name.c | build
	$(WINCC) $(CFLAGS) -static $< -o $@
build/privileged_probe.exe: tests/privileged_probe.c | build
	$(WINCC) $(CFLAGS) -static $< -o $@
build/bugcheck_registration.exe: tests/bugcheck_registration.c | build
	$(WINCC) $(CFLAGS) -static $< -o $@
build/physical_ranges.exe: tests/physical_ranges.c | build
	$(WINCC) $(CFLAGS) -static $< -o $@
build/physical_mapping.exe: tests/physical_mapping.c | build
	$(WINCC) $(CFLAGS) -static $< -o $@
build/thread_process.exe: tests/thread_process.c | build
	$(WINCC) $(CFLAGS) -static $< -o $@
clean:
	rm -rf build
