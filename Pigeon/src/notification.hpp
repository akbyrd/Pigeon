#define RGBA(r, g, b, a) ((b << 0) | (g << 8) | (r << 16) | (a << 24))
#define WM_PROCESSQUEUE WM_USER + 0

enum struct AnimPhase
{
	Showing,
	Shown,
	Hiding,
	Hidden
};

enum struct Severity
{
	Info,
	Warning,
	Error
};

struct Notification
{
	Severity severity;
	c16      text[256];
};

struct NotificationState
{
	Notification queue[4]              = {};
	u8           queueStart            = 0;
	u8           queueCount            = 0;

	b32          isDirty               = false;
	AnimPhase    animPhase             = AnimPhase::Hidden;
	f64          animStartTick         = 0;
	f64          animShowTicks         = 0;
	f64          animIdleTicks         = 0;
	f64          animHideTicks         = 0;
	u16          animUpdateMS          = 0;
	f64          tickFrequency         = 0;

	u16          windowMinWidth        = 0;
	u16          windowMaxWidth        = 0;
	SIZE         windowSize            = {};
	POINT        windowPosition        = {};

	COLORREF     backgroundColor       = {};
	COLORREF     textColorNormal       = {};
	COLORREF     textColorError        = {};
	COLORREF     textColorWarning      = {};
	u8           textPadding           = 0;

	b32          isInitialized         = false;
	HWND         hwnd                  = nullptr;
	HDC          bitmapDC              = nullptr;
	HFONT        font                  = nullptr;
	HBITMAP      bitmap                = nullptr;
	u32*         pixels                = nullptr;
	u64          timerID               = 0;
	c16          logFilePath[MAX_PATH] = {};

	HFONT        previousFont          = nullptr;
	HBITMAP      previousBitmap        = nullptr;
};

b32 ProcessNotificationQueue(NotificationState* state);
u8 LogicalToActualIndex(NotificationState* state, u8 index);

#define NOTIFY_IF(expression, severity, reaction, string, ...) \
	if (expression) \
	{ \
		NotifyFormat(state, severity, string, ## __VA_ARGS__); \
		reaction; \
	} \

#define WARN_IF(expression, reaction, string, ...) \
	NOTIFY_IF(expression, Severity::Warning, reaction, string, ## __VA_ARGS__)

#define ERROR_IF(expression, reaction, string, ...) \
	NOTIFY_IF(expression, Severity::Error, reaction, string, ## __VA_ARGS__)

#define NOTIFY_IF_FAILED(hr, severity, reaction, string) \
	if (FAILED(hr)) \
	{ \
		NotifyWindowsError(state, hr, severity, string); \
		reaction; \
	}

#define WARN_IF_FAILED(hr, reaction, string) \
	NOTIFY_IF_FAILED(hr, Severity::Warning, reaction, string)

#define ERROR_IF_FAILED(hr, reaction, string) \
	NOTIFY_IF_FAILED(hr, Severity::Error, reaction, string)

#define NOTIFY_IF_INVALID_HANDLE(handle, severity, reaction, string) \
	if (handle == INVALID_HANDLE_VALUE) \
	{ \
		NotifyWindowsError(state, severity, string); \
		reaction; \
	}

#define WARN_IF_INVALID_HANDLE(handle, reaction, string) \
	NOTIFY_IF_INVALID_HANDLE(handle, Severity::Warning, reaction, string)

#define ERROR_IF_INVALID_HANDLE(handle, reaction, string) \
	NOTIFY_IF_INVALID_HANDLE(handle, Severity::Error, reaction, string)

#define WINDOWS_NOTIFY_IF(expression, severity, reaction, string) \
	if (expression) \
	{ \
		NotifyWindowsError(state, severity, string); \
		reaction; \
	} \

#define WINDOWS_WARN_IF(expression, reaction, string) \
	WINDOWS_NOTIFY_IF(expression, Severity::Warning, reaction, string)

#define WINDOWS_ERROR_IF(expression, reaction, string) \
	WINDOWS_NOTIFY_IF(expression, Severity::Error, reaction, string)

#define NOTHING

void
Notify(NotificationState* state, Severity severity, c16* text)
{
	// Supercede existing non-error notifications
	for (u8 i = 0; i < state->queueCount; i++)
	{
		u8 actualIndex = LogicalToActualIndex(state, i);
		if (state->queue[actualIndex].severity == Severity::Info)
		{
			for (u8 j = i; j < state->queueCount-1; j++)
			{
				u8 actualIndex = LogicalToActualIndex(state, j);
				u8 nextIndex = LogicalToActualIndex(state, j+1);
				state->queue[actualIndex] = state->queue[nextIndex];
			}

			state->queueCount--;
			break;
		}
	}

	// Overflow
	u8 maxQueueCount = ArrayCount(state->queue) - 1;
	if (state->queueCount == maxQueueCount)
	{
		severity = Severity::Warning;
		text = L"Queue is overflowing";
	}
	else if (state->queueCount > maxQueueCount)
	{
		Assert(state->queueCount == ArrayCount(state->queue));
		return;
	}

	// Queue
	u8 nextIndex = LogicalToActualIndex(state, state->queueCount);
	state->queue[nextIndex].severity = severity;
	StrCpyW(state->queue[nextIndex].text, text);
	state->queueCount++;


	if (state->queueCount == 1)
	{
		ProcessNotificationQueue(state);
	}
	else
	{
		b32 isCurrentUnimportant = state->queue[state->queueStart].severity == Severity::Info;
		b32 isCurrentHiding = state->animPhase == AnimPhase::Hiding || state->animPhase == AnimPhase::Hidden;

		if (isCurrentUnimportant || isCurrentHiding)
			ProcessNotificationQueue(state);
	}
}

inline void
NotifyFormat(NotificationState* state, Severity severity, c16* format, va_list args)
{
	c16 buffer[ArrayCount(Notification::text)] = {};
	_vsnwprintf_s(buffer, ArrayCount(buffer), format, args);

	Notify(state, severity, buffer);
}

inline void
NotifyFormat(NotificationState* state, Severity severity, c16* format, ...)
{
	va_list args;
	va_start(args, format);

	NotifyFormat(state, severity, format, args);

	va_end(args);
}

inline b32
NotifyWindowsError(NotificationState* state, u32 errorCode, Severity severity, c16* text)
{
	u32 uResult = 0;

	c16 tempBuffer[ArrayCount(Notification::text)] = {};
	uResult = FormatMessageW(
		FORMAT_MESSAGE_FROM_SYSTEM,
		nullptr,
		errorCode,
		0,
		tempBuffer,
		ArrayCount(tempBuffer),
		nullptr
	);

	if (uResult == 0)
	{
		b32 bResult = NotifyWindowsError(state, errorCode, Severity::Warning, L"FormatMessage failed");
		if (!bResult)
			Notify(state, Severity::Warning, L"FormatMessage failed repeatedly");

		return false;
	}

	NotifyFormat(state, severity, L"%s - %s", text, tempBuffer);

	return true;
}

inline b32
NotifyWindowsError(NotificationState* state, Severity severity, c16* text)
{
	return NotifyWindowsError(state, GetLastError(), severity, text);
}

// TODO: Implement a formal circular buffer, overload operator[], and remove this function
inline u8
LogicalToActualIndex(NotificationState* state, u8 index)
{
	Assert(index <= state->queueCount);

	const u8 queueSize = ArrayCount(state->queue);
	static_assert(queueSize != 0, "Notification queue size is not a power of 2");
	static_assert((queueSize & (queueSize - 1)) == 0, "Notification queue size is not a power of 2");

	u8 result = (state->queueStart + index) & (queueSize - 1);
	return result;
}

inline b32
UpdateWindowPositionAndSize(NotificationState* state)
{
	b32 success;

	success = SetWindowPos(
		state->hwnd,
		nullptr,
		state->windowPosition.x,
		state->windowPosition.y,
		state->windowSize.cx,
		state->windowSize.cy,
		SWP_DEFERERASE | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOREPOSITION | SWP_NOZORDER
	);

	return success;
}

// TODO: If an error occurs while processing a notification more than once in a row, notifications
// are broken and this is an error condition.
b32
ProcessNotificationQueue(NotificationState* state)
{
	i32 iResult;
	u64 uResult;
	b32 success;

	if (!state->isInitialized) return true;
	if (state->queueCount == 0) return true; // TODO: Should this happen?

	Notification* notification = &state->queue[state->queueStart];

	// Resize
	RECT textSizeRect = {};
	iResult = DrawTextW(
		state->bitmapDC,
		notification->text, -1,
		&textSizeRect,
		DT_CALCRECT | DT_SINGLELINE
	);
	WARN_IF(!iResult, return false, L"DrawText failed: %i", iResult)

	i32 textWidth = textSizeRect.right - textSizeRect.left;

	u16 windowWidth = textWidth + 2*state->textPadding;
	if (windowWidth < state->windowMinWidth) windowWidth = state->windowMinWidth;
	if (windowWidth > state->windowMaxWidth) windowWidth = state->windowMaxWidth;

	if (state->windowSize.cx != windowWidth)
	{
		state->windowSize.cx = windowWidth;

		success = UpdateWindowPositionAndSize(state);
		if (!success) return false;
	}


	// Background
	for (u16 y = 0; y < state->windowSize.cy; y++)
	{
		u32* row = state->pixels + y*state->windowMaxWidth;
		for (u16 x = 0; x < state->windowSize.cx; x++)
		{
			u32* pixel = row + x;
			*pixel = state->backgroundColor;
		}
	}


	// Text
	COLORREF newColor = {};
	switch (notification->severity)
	{
		case Severity::Info:    newColor = state->textColorNormal;  break;
		case Severity::Warning: newColor = state->textColorWarning; break;
		case Severity::Error:   newColor = state->textColorError;   break;
	}

	COLORREF previousColor = {};
	previousColor = SetTextColor(state->bitmapDC, newColor);
	WINDOWS_WARN_IF(previousColor == CLR_INVALID, return false, L"SetTextColor failed");

	RECT textRect = {};
	textRect.left = state->textPadding;
	textRect.top = 0;
	textRect.right = state->windowSize.cx - state->textPadding;
	textRect.bottom = state->windowSize.cy;

	iResult = DrawTextW(
		state->bitmapDC,
		notification->text, -1,
		&textRect,
		DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS
	);
	WARN_IF(!iResult, return false, L"DrawText failed: %i", iResult)


	// TODO: Only do text rect
	// Patch text alpha
	u32 fullA = RGBA(0, 0, 0, 255);
	for (u16 y = 0; y < state->windowSize.cy; y++)
	{
		u32* row = state->pixels + y*state->windowMaxWidth;
		for (u16 x = 0; x < state->windowSize.cx; x++)
		{
			u32* pixel = row + x;
			if (*pixel != state->backgroundColor)
				*pixel |= fullA;
		}
	}

	// Animate
	LARGE_INTEGER win32_currentTicks = {};
	QueryPerformanceCounter(&win32_currentTicks);

	f64 currentTicks = (f64) win32_currentTicks.QuadPart;

	switch (state->animPhase)
	{
		case AnimPhase::Showing:
		{
			break;
		}

		case AnimPhase::Shown:
		{
			state->animStartTick = currentTicks;

			break;
		}

		case AnimPhase::Hiding:
		{
			state->animPhase = AnimPhase::Showing;

			// TODO: This will not work if the show and hide animations are different or asymmetric.
			f64 normalizedTimeInState = (currentTicks - state->animStartTick) / state->animHideTicks;
			if (normalizedTimeInState > 1) normalizedTimeInState = 1;

			state->animStartTick = currentTicks - ((1 - normalizedTimeInState) * state->animShowTicks);

			// TODO: This will overshoot by an amount based on the animation step duration
			// TODO: When replacing the current timer, WM_TIMER will not be sent until the new time
			// elapses. This means if you continually queue messages faster than the animation tick you
			// can get the notification to hang.
			uResult = SetTimer(state->hwnd, state->timerID, state->animUpdateMS, nullptr);
			WINDOWS_WARN_IF(!uResult, return false, L"SetTimer failed");

			break;
		}

		case AnimPhase::Hidden:
		{
			state->animPhase = AnimPhase::Showing;
			state->animStartTick = currentTicks;

			ShowWindow(state->hwnd, SW_SHOW);

			// TODO: This will overshoot by an amount based on the animation step duration
			uResult = SetTimer(state->hwnd, state->timerID, state->animUpdateMS, nullptr);
			WINDOWS_WARN_IF(!uResult, return false, L"SetTimer failed");

			break;
		}
	}

	// NOTE: Update the window immediately, without worrying about USER_TIMER_MINIMUM
	success = PostMessageW(state->hwnd, WM_TIMER, state->timerID, NULL);
	WINDOWS_WARN_IF(!success, return false, L"PostMessage failed");

	state->isDirty = true;

	return true;
}

// TODO: If an error occurs while processing a WM_TIMER more than once in a row, notifications are
// broken and this is an error condition.
LRESULT CALLBACK
NotificationWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	b32 success;
	u64 uResult;

	auto state = (NotificationState*) GetWindowLongPtrW(hwnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			// TODO: Getting this message twice

			auto createStruct = (CREATESTRUCT*) lParam;
			state = (NotificationState*) createStruct->lpCreateParams;

			// NOTE: Because Windows is dumb. See Return value section:
			// https://msdn.microsoft.com/en-us/library/windows/desktop/ms644898.aspx
			SetLastError(0);

			i64 iResult = SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LPARAM) state);
			WINDOWS_ERROR_IF(iResult == 0 && GetLastError() != 0, return false, L"SetWindowLongPtr failed");
			break;
		}

		case WM_CREATE:
		{
			// Bitmap
			BITMAPINFO bitmapInfo = {};
			bitmapInfo.bmiHeader.biSize          = sizeof(bitmapInfo.bmiHeader);
			bitmapInfo.bmiHeader.biWidth         = state->windowMaxWidth;
			bitmapInfo.bmiHeader.biHeight        = state->windowSize.cy;
			bitmapInfo.bmiHeader.biPlanes        = 1;
			bitmapInfo.bmiHeader.biBitCount      = 32;
			bitmapInfo.bmiHeader.biCompression   = BI_RGB;
			bitmapInfo.bmiHeader.biSizeImage     = 0;
			bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
			bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
			bitmapInfo.bmiHeader.biClrUsed       = 0;
			bitmapInfo.bmiHeader.biClrImportant  = 0;
			bitmapInfo.bmiColors[0]              = {};

			HDC screenDC = GetDC(nullptr);
			WARN_IF(!screenDC, return -1, L"GetDC failed")

			state->bitmapDC = CreateCompatibleDC(screenDC);
			WARN_IF(!state->bitmapDC, return -1, L"CreateCompatibleDC failed")

			state->bitmap = CreateDIBSection(
				state->bitmapDC,
				&bitmapInfo,
				DIB_RGB_COLORS,
				(void**) &state->pixels,
				nullptr,
				0
			);
			WARN_IF(!state->pixels, return -1, L"CreateDIBSection failed")

			i32 iResult = ReleaseDC(nullptr, screenDC);
			WARN_IF(iResult == 0, return -1, L"ReleaseDC failed")

			b32 success = GdiFlush();
			WARN_IF(!success, return -1, L"GdiFlush failed")

			state->previousBitmap = (HBITMAP) SelectObject(state->bitmapDC, state->bitmap);
			WARN_IF(!state->previousBitmap, return -1, L"SelectObject failed")


			// Font
			NONCLIENTMETRICSW nonClientMetrics = {};
			nonClientMetrics.cbSize = sizeof(nonClientMetrics);

			success = SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, nonClientMetrics.cbSize, &nonClientMetrics, 0);
			WINDOWS_WARN_IF(!success, return -1, L"SystemParametersInfo failed");

			state->font = CreateFontIndirectW(&nonClientMetrics.lfMessageFont);
			WARN_IF(!state->font, return -1, L"CreateFontIndirect failed")

			state->previousFont = (HFONT) SelectObject(state->bitmapDC, state->font);
			WARN_IF(!state->previousFont, return -1, L"SelectObject failed")

			iResult = SetBkMode(state->bitmapDC, TRANSPARENT);
			WARN_IF(iResult == 0, return -1, L"SetBkMode failed")

			state->isInitialized = true;
			success = PostMessageW(hwnd, WM_PROCESSQUEUE, 0, 0);
			WINDOWS_WARN_IF(!success, NOTHING, L"PostMessage failed");

			return 0;
		}

		case WM_DESTROY:
		{
			state->isInitialized = false;

			// NOTE: We have to replace the previous objects so they get destroyed with the DC. I don't
			// think we can destroy them manually, and we can't destroy our own objects while they are
			// selected into the DC (though destroying the DC *probably* destroys them). Also, there's
			// not really anything we can do if any of this fails.
			//
			// We can still Notify because it will short circuit on isInitialized == false and we'll be
			// able to show any queued errors during shutdown through other means (e.g. MessageBox).

			// Delete font
			HGDIOBJ previousObject;
			previousObject = SelectObject(state->bitmapDC, state->previousFont);
			state->previousFont = nullptr;
			WARN_IF(!previousObject, NOTHING, L"SelectObject failed")

			b32 success;
			success = DeleteObject(state->font);
			state->font = nullptr;
			WARN_IF(!success, NOTHING, L"DeleteObject failed")

			// Delete bitmap
			previousObject = SelectObject(state->bitmapDC, state->previousBitmap);
			state->previousBitmap = nullptr;
			WARN_IF(!previousObject, NOTHING, L"SelectObject failed")

			success = DeleteObject(state->bitmap);
			state->bitmap = nullptr;
			WARN_IF(!success, NOTHING, L"DeleteObject failed")

			// Delete DC
			success = DeleteDC(state->bitmapDC);
			state->bitmapDC = nullptr;
			WARN_IF(!success, NOTHING, L"DeleteDC failed")

			return 0;
		}

		case WM_PROCESSQUEUE:
			ProcessNotificationQueue(state);
			return 0;

		case WM_TIMER:
		{
			if (wParam == state->timerID)
			{
				// TODO: Cleanup
				b32 changed = true;
				b32 alphaChanged = false;

				while (changed)
				{
					changed = false;

					LARGE_INTEGER win32_currentTicks = {};
					QueryPerformanceCounter(&win32_currentTicks);

					f64 currentTicks = (f64) win32_currentTicks.QuadPart;

					f64 animTicks = currentTicks - state->animStartTick;

					f32 newAlpha = 1;
					switch (state->animPhase)
					{
						case AnimPhase::Showing:
						{
							alphaChanged = true;

							if (animTicks < state->animShowTicks)
							{
								// TODO: Do we want something other than linear?
								newAlpha = (f32) (animTicks / state->animShowTicks);
							}
							else
							{
								f64 overshootTicks = animTicks - state->animShowTicks;

								state->animPhase = AnimPhase::Shown;
								state->animStartTick = currentTicks - overshootTicks;

								changed = true;
								continue;
							}
							break;
						}

						case AnimPhase::Shown:
						{
							if (animTicks < state->animIdleTicks)
							{
								newAlpha = 1;

								// TODO: Round?
								u32 remainingMS = (u32) ((state->animIdleTicks - animTicks) / state->tickFrequency * 1000.);
								uResult = SetTimer(hwnd, state->timerID, remainingMS, nullptr);
								WINDOWS_WARN_IF(uResult == 0, NOTHING, L"SetTimer failed");
							}
							else
							{
								f64 overshootTicks = animTicks - state->animIdleTicks;

								state->animPhase = AnimPhase::Hiding;
								state->animStartTick = currentTicks - overshootTicks;

								// TODO: Formalize state changes
								uResult = SetTimer(hwnd, state->timerID, state->animUpdateMS, nullptr);
								WINDOWS_WARN_IF(uResult == 0, NOTHING, L"SetTimer failed");

								changed = true;
								continue;
							}

							break;
						}

						case AnimPhase::Hiding:
						{
							// Auto-show next notification
							b32 isNotificationPending = state->queueCount > 1;
							b32 allowNextNote = animTicks > .3f * state->animHideTicks;
							allowNextNote &= state->queue[state->queueStart].severity != Severity::Error;

							if (allowNextNote && isNotificationPending)
							{
								state->queueStart = LogicalToActualIndex(state, 1);
								state->queueCount--;

								ProcessNotificationQueue(state);
								return 0;
							}

							alphaChanged = true;

							if (animTicks < state->animHideTicks)
							{
								// TODO: Do we want something other than linear?
								newAlpha = (f32) (1. - animTicks / state->animHideTicks);
							}
							else
							{
								f64 overshootTicks = animTicks - state->animHideTicks;

								state->animPhase = AnimPhase::Hidden;
								state->animStartTick = currentTicks - overshootTicks;

								changed = true;
								continue;
							}

							break;
						}

						case AnimPhase::Hidden:
						{
							if (state->queue[state->queueStart].severity == Severity::Error)
							{
								PostQuitMessage(-2);
							}
							else
							{
								Assert(state->queueCount == 1);
							}

							state->queueStart = 0;
							state->queueCount = 0;

							newAlpha = 0;

							success = KillTimer(hwnd, state->timerID);
							WINDOWS_WARN_IF(!success, break, L"KillTimer failed");

							ShowWindow(hwnd, SW_HIDE);

							break;
						}
					}

					b32 isHidden = state->animPhase == AnimPhase::Hidden;

					if (state->isDirty || alphaChanged)
					{
						BLENDFUNCTION blendFunction = {};
						blendFunction.BlendOp             = AC_SRC_OVER;
						blendFunction.BlendFlags          = 0;
						blendFunction.SourceConstantAlpha = (u8) (255.f*newAlpha + .5f);
						blendFunction.AlphaFormat         = AC_SRC_ALPHA;

						POINT zeroPoint = {0, 0};

						// NOTE: I don't understand why, but psize *must* be specified or the notification
						// isn't visible even though the position isn't changing here.
						success = UpdateLayeredWindow(
							state->hwnd,
							nullptr,
							nullptr, &state->windowSize,
							state->bitmapDC,
							&zeroPoint,
							CLR_INVALID,
							&blendFunction,
							ULW_ALPHA
						);
						WINDOWS_WARN_IF(!success, return 0, L"UpdateLayeredWindow failed");

						state->isDirty = false;
					}
				}

				return 0;
			}
			break;
		}
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}
