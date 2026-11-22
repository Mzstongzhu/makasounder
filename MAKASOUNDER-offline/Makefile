CXX ?= g++
WINDRES ?= windres
CXXFLAGS ?= -std=c++17 -O2 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -DNDEBUG -Wall -Wextra \
	-Wno-cast-function-type -Wno-unused-parameter -Isrc
LIBS = -s -static -static-libgcc -static-libstdc++ -mwindows -lcomctl32 -lgdiplus \
	-lshlwapi -lwinmm -lole32 -loleaut32 -lstrmiids -lquartz -luuid -lshell32 -luser32 -lgdi32 -lmsimg32 \
	-ladvapi32 -lcomdlg32

SOURCES = common fileio lyrics metadata artwork audio settings association \
	ui_main ui_float app main
OBJECTS = $(addprefix build/,$(addsuffix .o,$(SOURCES)))
TARGET = build/MAKASOUNDER.exe

.PHONY: all clean run
all: $(TARGET)

build:
	if not exist build mkdir build

build/%.o: src/%.cpp | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/app_res.o: MAKASOUNDER.rc app.manifest assets/app.ico assets/placeholder.png | build
	$(WINDRES) -i MAKASOUNDER.rc -o $@ --input-format=rc --output-format=coff --codepage=65001

$(TARGET): $(OBJECTS) build/app_res.o
	$(CXX) $(OBJECTS) build/app_res.o -o $@ $(LIBS)

run: $(TARGET)
	./build/MAKASOUNDER.exe

clean:
	-del /Q build\*.o build\MAKASOUNDER.exe 2>NUL
