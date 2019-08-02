CC=gcc
out=build
src=source/ccontutils.c source/jail.c source/shortid.c source/main.c
name=ccont
flags=-g
include=-I$(shell pwd)/headers
libs=-lm

default=ccont

.PHONY: ccont
ccont: $(src)
	mkdir -p $(out) && $(CC) $(include) -o $(out)/$(name) $(src) $(libs)
	cp -r ./config $(out)/
	mkdir -p $(out)/cache/build
	mkdir -p $(out)/containers

install: ccont
	sudo ln -sf $(shell pwd)/$(out)/$(name) /usr/bin/$(name)

val: ccont
	sudo valgrind --show-leak-kinds=all --leak-check=full $(out)/ccont ubuntu-test