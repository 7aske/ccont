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
	mkdir -p $(out) && $(CC) $(include) -o $(out)/$(name) $(src) $(libs) && ln -f $(shell pwd)/$(out)/$(name) $(name) && sudo valgrind --leak-check=full ./ccont ubuntu