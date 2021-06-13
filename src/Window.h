#pragma once

#include "Typedefs.h"

typedef struct Window_t *Window;

Window Window_Create(u32 width, u32 height, const char* title);
void Window_Destroy(Window window);
void Window_Show(Window window);
void Window_GetSize(Window window, u32* width, u32* height);
b8 Window_PollEvents();
