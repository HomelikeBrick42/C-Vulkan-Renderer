#include "Window.h"
#include "Win32Window.h"

#if defined(_WIN32) || defined(_WIN64)

#include <Windows.h>

static LRESULT Win32WindowCallback(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;

	Window window = cast(Window) GetWindowLongPtrA(windowHandle, GWLP_USERDATA);
	if (window) {
		switch (message) {
			case WM_CLOSE:
			case WM_QUIT: {
				PostQuitMessage(0);
			} break;

			default: {
				result = DefWindowProcA(window->Handle, message, wParam, lParam);
			} break;
		}
	} else {
		result = DefWindowProcA(windowHandle, message, wParam, lParam);
	}

	return result;
}

static b8 WindowClassInitialized = false;
Window Window_Create(u32 width, u32 height, const char* title) {
	const char* WindowClassName = "TestRendererWindowClassName"; // TODO:

	Window window = calloc(1, sizeof(*window));
	window->Instance = GetModuleHandle(NULL);

	if (!WindowClassInitialized) {
		WNDCLASSEXA windowClass = {
			.cbSize = sizeof(windowClass),
			.lpfnWndProc = Win32WindowCallback,
			.hInstance = window->Instance,
			.hCursor = LoadCursor(NULL, IDC_ARROW),
			.lpszClassName = WindowClassName,
		};

		if (!RegisterClassExA(&windowClass)) {
			free(window);
			return NULL;
		}

		WindowClassInitialized = true;
	}

	const DWORD windowStyle = WS_OVERLAPPEDWINDOW;

	RECT windowRect = {};
	windowRect.left = 100;
	windowRect.right = windowRect.left + width;
	windowRect.top = 100;
	windowRect.bottom = windowRect.top + height;

	AdjustWindowRect(&windowRect, windowStyle, false);

	u32 windowWidth = windowRect.right - windowRect.left;
	u32 windowHeight = windowRect.bottom - windowRect.top;

	window->Handle = CreateWindowExA(
		0,
		WindowClassName,
		title,
		windowStyle,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		windowWidth,
		windowHeight,
		NULL,
		NULL,
		window->Instance,
		NULL
	);

	if (!window->Handle) {
		free(window);
		return NULL;
	}

	SetWindowLongPtrA(window->Handle, GWLP_USERDATA, (LONG_PTR)window);

	return window;
}

void Window_Destroy(Window window) {
	DestroyWindow(window->Handle);
	free(window);
}

void Window_Show(Window window) {
	ShowWindow(window->Handle, SW_SHOW);
}

void Window_GetSize(Window window, u32* width, u32* height) {
	RECT clientRect = {};
	GetClientRect(window->Handle, &clientRect);

	if (width) {
		*width = cast(u32) (clientRect.right - clientRect.left);
	}

	if (height) {
		*height = cast(u32) (clientRect.bottom - clientRect.top);
	}
}

b8 Window_PollEvents() {
	MSG message;
	while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			return false;
		}

		TranslateMessage(&message);
		DispatchMessageA(&message);
	}

	return true;
}

#else
	#error This platform is not supported
#endif
