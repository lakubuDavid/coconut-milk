set shell := ["bash", "-uc"]

DEFAULT_TARGET := "coconut-milk"
TEST_TARGET := "coconut-milk-tests"

default:
	@just --list

build:
	xmake build {{DEFAULT_TARGET}}

run:
	xmake run {{DEFAULT_TARGET}}

test:
	xmake build {{TEST_TARGET}}
	xmake run {{TEST_TARGET}}

clean:
	xmake clean

rebuild:
	just clean
	just build

debug:
	xmake f -m debug
	just build
	just run

format:
	clang-format -i src/**/*.h src/**/*.cpp tests/**/*.cpp
