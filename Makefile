CXX=g++
out=ccont
src=source/main.cpp
flags=-g

default_recipe=ccont

ccont: $(src)
	$(CXX) -o $(out) $(src)

install: ccont
	sudo ln -sf $(shell pwd)/$(out) /usr/bin/$(out) 
