CC=gcc
out=./build
src=source/main.cpp
flags=-g

default=ccont

ccont: $(src)
	mkdir -p $(out) && $(CXX) -o $(out)/ccont $(src)

install: ccont
	sudo ln -sf $(shell pwd)/$(out) /usr/bin/$(out) 

run: ccont
	./build/ccont $(ARGS)