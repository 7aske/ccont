#pragma once

#include <stdio.h>

void readcmd(char* data, char* const cmd);

char* allocate_stack(int size);

void panic(const char* message);

