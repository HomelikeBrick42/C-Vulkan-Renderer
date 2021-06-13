#pragma once

#include "Typedefs.h"

#include <Windows.h>

typedef struct Window_t {
	HMODULE Instance;
	HWND Handle;
} *Window;
