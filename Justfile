set shell := ["bash", "-uc"]

DEFAULT_TARGET := "coconut-milk"
TEST_TARGET := "coconut-milk-tests"
GENERATOR_TARGET := "coconut-milk-generators"

default:
	@just --list

build:
	xmake build {{DEFAULT_TARGET}}

run:
	xmake run {{DEFAULT_TARGET}}

test:
	xmake build {{TEST_TARGET}}
	xmake run {{TEST_TARGET}}

run-gen INP="samples/commands/hello.lua":
	xmake build {{GENERATOR_TARGET}}
	xmake run {{GENERATOR_TARGET}} "{{INP}}"

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
