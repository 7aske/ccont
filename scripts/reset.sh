#!/bin/bash
echo "$1"
if [ "$1" == "c" ]
then
	sudo rm -rf ./build/containers/*
fi

if [ "$1" == "cc" ]
then
  sudo rm -rf ./build/cache/*
fi

if [ "$1" == "b" ]
then
  sudo rm -rf ./build/cache/build/*
fi