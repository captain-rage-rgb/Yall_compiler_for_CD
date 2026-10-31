CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic

TARGET = yall

SRCS = src/main.cpp src/lexer.cpp src/parser.cpp src/ast_printer.cpp src/symbol_table.cpp \
       src/semantic.cpp src/tac_printer.cpp src/irgen.cpp src/optimizer.cpp \
       src/codegen.cpp src/disassembler.cpp src/vm.cpp src/diagnostics.cpp

HEADERS = src/token.h src/lexer.h src/ast.h src/parser.h src/ast_printer.h src/types.h \
          src/symbol_table.h src/semantic.h src/numeric.h src/tac.h src/tac_printer.h \
          src/irgen.h src/optimizer.h src/bytecode.h src/codegen.h src/disassembler.h \
          src/vm.h src/diagnostics.h

SHELL = D:/mysys/usr/bin/sh.exe
SH = D:/mysys/usr/bin/sh.exe
export PATH := D:/mysys/usr/bin:$(PATH)
RM = D:/mysys/usr/bin/rm.exe -f

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) src/*.cpp

test: $(TARGET)
	@$(SH) tests/run_all.sh

sanitize:
	$(CXX) -std=c++17 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g -o $(TARGET) src/*.cpp
	@$(SH) tests/run_all.sh

clean:
	$(RM) $(TARGET) $(TARGET).exe

.PHONY: all test sanitize clean
