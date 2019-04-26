#!/bin/bash
echo $1
if [ $1 = "c" ]
	then
		sudo rm -rf containers/*
fi

if [ $1 = "cc" ]
	then
		sudo rm -rf cache/*
fi

if [ $1 = "b" ]
	then
		sudo rm -rf cache/build/*
fi