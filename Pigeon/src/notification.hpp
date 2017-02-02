#define RGBA(r, g, b, a) ((b << 0) | (g << 8) | (r << 16) | (a << 24))
#define WM_PROCESSQUEUE WM_USER + 0

enum struct AnimState
{
	Showing,
	Shown,
	Hiding,
	Hidden
};

enum struct Error
{
	None,
	Warning,
	Error
};

struct Notification
{
	Error error;
	c16   text[256];
};

struct NotificationWindow
{
	Notification queue[4]          = {};
	u8           queueStart        = 0;
	u8           queueCount        = 0;

	b32          isDirty           = false;
	AnimState    animState         = AnimState::Hidden;
	f64          animStartTick     = 0;
	f64          animShowTicks     = 0;
	f64          animIdleTicks     = 0;
	f64          animHideTicks     = 0;
	u16          animUpdateMS      = 0;
	f64          tickFrequency     = 0;

	u16          windowMinWidth    = 0;
	u16          windowMaxWidth    = 0;
	SIZE         windowSize        = {};
	POINT        windowPosition    = {};

	COLORREF     backgroundColor   = {};
	COLORREF     textColorNormal   = {};
	COLORREF     textColorError    = {};
	COLORREF     textColorWarning  = {};
	u8           textPadding       = 0;

	b32          isInitialized     = false;
	HWND         hwnd              = nullptr;
	HDC          screenDC          = nullptr;
	HDC          bitmapDC          = nullptr;
	HFONT        font              = nullptr;
	HBITMAP      bitmap            = nullptr;
	u32*         pixels            = nullptr;
	u64          timerID           = 0;

	HFONT        previousFont      = nullptr;
	HBITMAP      previousBitmap    = nullptr;
};

void ProcessNotificationQueue(NotificationWindow* state);
u8 LogicalToActualIndex(NotificationWindow* state, u8 index); // TODO: Remove?

void
Notify(NotificationWindow* state, c16* text, Error error = Error::None)
{
	// TODO: Maybe have a loop iteration counter in the main pump and
	// use it to prevent infinite notifications from failures?

	// TODO: Sort out naming convention. i.e. Notification vs Message

	// TODO: Error pigeon sound
	// TODO: Line on notification indicating queue count (colored if warning/error exists?)

	// Supercede existing non-error notifications
	for (u8 i = 0; i < state->queueCount; i++)
	{
		u8 actualIndex = LogicalToActualIndex(state, i);
		if (state->queue[actualIndex].error == Error::None)
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
		error = Error::Warning;
		text  = L"Queue is overflowing";
	}
	else if (state->queueCount > maxQueueCount)
	{
		Assert(state->queueCount == ArrayCount(state->queue));
		return;
	}

	// Queue
	u8 nextIndex = LogicalToActualIndex(state, state->queueCount);
	state->queue[nextIndex].error = error;
	StrCpyW(state->queue[nextIndex].text, text);
	state->queueCount++;


	if (state->queueCount == 1)
	{
		ProcessNotificationQueue(state);
	}
	else
	{
		b32 isCurrentUnimportant = state->queue[state->queueStart].error == Error::None;
		b32 isCurrentHiding = state->animState == AnimState::Hiding || state->animState == AnimState::Hidden;

		if (isCurrentUnimportant || isCurrentHiding)
			ProcessNotificationQueue(state);
	}
}

inline void
NotifyFormat(NotificationWindow* notification, c16* format, Error error, va_list args)
{
	c16 buffer[ArrayCount(Notification::text)] = {};
	_vsnwprintf(buffer, ArrayCount(buffer), format, args);

	Notify(notification, buffer, error);
}

inline void
NotifyFormat(NotificationWindow* notification, c16* format, Error error, ...)
{
	va_list args;
	va_start(args, error);

	NotifyFormat(notification, format, error, args);

	va_end(args);
}

inline void
NotifyFormat(NotificationWindow* notification, c16* format, ...)
{
	va_list args;
	va_start(args, format);

	NotifyFormat(notification, format, Error::None, args);

	va_end(args);
}

void
NotifyWindowsError(NotificationWindow* notification, c16* text, Error error = Error::Error, u32 errorCode = GetLastError())
{
	u32 uResult = 0;

	// TODO: Use vs_list?
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
	// TODO: Error
	Assert(uResult > 0);

	NotifyFormat(notification, L"%s - %s", error, text, tempBuffer);
}

inline u8
LogicalToActualIndex(NotificationWindow* state, u8 index)
{
	Assert(index <= state->queueCount);
	//TODO: Assert ArrayCount(state->queue) is a power of 2

	u8 result = (state->queueStart + index) & (ArrayCount(state->queue) - 1);
	return result;
}

inline b32
UpdateWindowPositionAndSize(NotificationWindow* state)
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

inline void
ProcessNotificationQueue(NotificationWindow* state)
{
	i32 iResult;
	u64 uResult;
	b32 success;

	if (!state->isInitialized) return;
	if (state->queueCount == 0) return;

	Notification* notification = &state->queue[state->queueStart];

	// Resize
	RECT textSizeRect = {};
	iResult = DrawTextW(
		state->bitmapDC,
		notification->text, -1,
		&textSizeRect,
		DT_CALCRECT | DT_SINGLELINE
	);
	//if (result == 0) break;
	// TODO: Error

	i32 textWidth = textSizeRect.right - textSizeRect.left;

	u16 windowWidth = textWidth + 2*state->textPadding;
	if (windowWidth < state->windowMinWidth) windowWidth = state->windowMinWidth;
	if (windowWidth > state->windowMaxWidth) windowWidth = state->windowMaxWidth;

	if (state->windowSize.cx != windowWidth)
	{
		state->windowSize.cx = windowWidth;

		success = UpdateWindowPositionAndSize(state);
		//if (!success) break;
		// TODO: Error
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
	switch (notification->error)
	{
		case Error::None:    newColor = state->textColorNormal;  break;
		case Error::Warning: newColor = state->textColorWarning; break;
		case Error::Error:   newColor = state->textColorError;   break;
		default: Assert(L"Missing Error case");
	}

	COLORREF previousColor = {};
	previousColor = SetTextColor(state->bitmapDC, newColor);
	if (previousColor == CLR_INVALID)
	{
		// TODO: Warning
		//GetLastError
		//L"Failed to set text color."
	}

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
	//if (result == 0) break;
	// TODO: Error


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

	switch (state->animState)
	{
		case AnimState::Showing:
		{
			break;
		}

		case AnimState::Shown:
		{
			state->animStartTick = currentTicks;

			break;
		}

		case AnimState::Hiding:
		{
			state->animState = AnimState::Showing;

			// TODO: This will not work if the show and hide animations are different or asymmetric
			f64 normalizedTimeInState = (currentTicks - state->animStartTick) / state->animHideTicks;
			if (normalizedTimeInState > 1) normalizedTimeInState = 1;

			state->animStartTick = currentTicks - ((1 - normalizedTimeInState) * state->animShowTicks);

			// TODO: This will overshoot by an amount based on the animation step duration
			uResult = SetTimer(state->hwnd, state->timerID, state->animUpdateMS, nullptr);
			//if (result == 0)
			// TODO: Error
			//GetLastError

			break;
		}

		case AnimState::Hidden:
		{
			state->animState = AnimState::Showing;
			state->animStartTick = currentTicks;

			success = ShowWindow(state->hwnd, SW_SHOW);
			//if (!success) return;
			// TODO: Error

			// TODO: This will overshoot by an amount based on the animation step duration
			uResult = SetTimer(state->hwnd, state->timerID, state->animUpdateMS, nullptr);
			//if (result == 0)
			// TODO: Error
			//GetLastError

			break;
		}

		default: Assert(L"Missing AnimState case");
	}

	// NOTE: Update the window immediately, without worrying about USER_TIMER_MINIMUM
	success = PostMessageW(state->hwnd, WM_TIMER, state->timerID, NULL);
	//if (result == 0) return;
	// TODO: Error

	state->isDirty = true;
}

LRESULT CALLBACK
NotificationWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	b32 success;
	u64 uResult;
	i32 iResult;

	auto state = (NotificationWindow*) GetWindowLongPtrW(hwnd, GWLP_USERDATA);

	switch (uMsg)
	{
		case WM_NCCREATE:
		{
			// TODO: Getting this message twice
			// TODO: Handle errors
			auto createStruct = (CREATESTRUCT*) lParam;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LPARAM) createStruct->lpCreateParams);
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
			bitmapInfo.bmiHeader.biXPelsPerMeter = 0; // TODO: ?
			bitmapInfo.bmiHeader.biYPelsPerMeter = 0; // TODO: ?
			bitmapInfo.bmiHeader.biClrUsed       = 0;
			bitmapInfo.bmiHeader.biClrImportant  = 0;
			//bitmapInfo.bmiColors                 = {}; // TODO: ?

			state->screenDC = GetDC(nullptr);
			if (!state->screenDC) return -1;

			state->bitmapDC = CreateCompatibleDC(state->screenDC);
			if (!state->bitmapDC) return -1;

			// TODO: Have to GdiFlush before using pixels
			// https://msdn.microsoft.com/query/dev14.query?appId=Dev14IDEF1&l=EN-US&k=k(WINGDI%2FCreateDIBSection);k(CreateDIBSection);k(DevLang-C%2B%2B);k(TargetOS-Windows)&rd=true
			state->bitmap = CreateDIBSection(
				state->bitmapDC,
				&bitmapInfo,
				DIB_RGB_COLORS,
				(void**) &state->pixels,
				nullptr,
				0
			);
			if (!state->pixels) return -1;
			GdiFlush();

			state->previousBitmap = (HBITMAP) SelectObject(state->bitmapDC, state->bitmap);
			if (!state->previousBitmap) return -1;

			// Font
			NONCLIENTMETRICSW nonClientMetrics = {};
			nonClientMetrics.cbSize = sizeof(nonClientMetrics);

			// TODO: Is it worth moving this to a function to linearize the flow?
			success = SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, nonClientMetrics.cbSize, &nonClientMetrics, 0);
			if (success)
			{
				state->font = CreateFontIndirectW(&nonClientMetrics.lfMessageFont);
				if (state->font)
				{
					state->previousFont = (HFONT) SelectObject(state->bitmapDC, state->font);
					if (!state->previousFont)
					{
						// TODO: Queue warning notification
						//GetLastError
						//L"Failed to use created font."
					}
				}
				else
				{
					// TODO: Queue warning notification
					//GetLastError
					//L"Failed to create font."
				}
			}
			else
			{
				// TODO: Queue warning notification
				//GetLastError
				//L"Failed to obtain the current font."
			}

			iResult = SetBkMode(state->bitmapDC, TRANSPARENT);
			if (iResult == 0)
			{
				// TODO: Queue warning notification
				//GetLastError
				//L"Failed to set transparent text background."
			}

			state->isInitialized = true;
			success = PostMessageW(hwnd, WM_PROCESSQUEUE, 0, 0);
			Assert(success);

			return 0;
		}

		case WM_DESTROY:
		{
			state->isInitialized = false;

			// TODO: Error handling
			// NOTE: We only really need to bother with this because we want to
			// destroy the startup window cleanly
			SelectObject(state->bitmapDC, state->previousFont);
			DeleteObject(state->font);

			SelectObject(state->bitmapDC, state->previousBitmap);
			DeleteObject(state->bitmap);

			DeleteDC(state->bitmapDC);
			ReleaseDC(nullptr, state->screenDC);

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
					switch (state->animState)
					{
						case AnimState::Showing:
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

								state->animState = AnimState::Shown;
								state->animStartTick = currentTicks - overshootTicks;

								changed = true;
								continue;
							}
							break;
						}

						case AnimState::Shown:
						{
							if (animTicks < state->animIdleTicks)
							{
								newAlpha = 1;

								// TODO: Round?
								u32 remainingMS = (u32) ((state->animIdleTicks - animTicks) / state->tickFrequency * 1000.);
								uResult = SetTimer(hwnd, state->timerID, remainingMS, nullptr);
								//if (result == 0)
								// TODO: Error
								//GetLastError
							}
							else
							{
								f64 overshootTicks = animTicks - state->animIdleTicks;

								state->animState = AnimState::Hiding;
								state->animStartTick = currentTicks - overshootTicks;

								// TODO: Formalize state changes
								uResult = SetTimer(hwnd, state->timerID, state->animUpdateMS, nullptr);
								//if (result == 0)
								// TODO: Error
								//GetLastError

								changed = true;
								continue;
							}

							break;
						}

						case AnimState::Hiding:
						{
							// Auto-show next notification
							b32 isNotificationPending = state->queueCount > 1;
							b32 allowNextNote = animTicks > .3f * state->animHideTicks;
							allowNextNote &= state->queue[state->queueStart].error != Error::Error;

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

								state->animState = AnimState::Hidden;
								state->animStartTick = currentTicks - overshootTicks;

								changed = true;
								continue;
							}

							break;
						}

						case AnimState::Hidden:
						{
							if (state->queue[state->queueStart].error == Error::Error)
							{
								PostQuitMessage(2);
							}
							else
							{
								Assert(state->queueCount == 1);
							}

							state->queueStart = 0;
							state->queueCount = 0;

							newAlpha = 0;

							success = KillTimer(hwnd, state->timerID);
							// TODO: Error

							success = ShowWindow(hwnd, SW_HIDE);
							// TODO: Error

							break;
						}

						default: Assert(L"Missing AnimState case");
					}

					bool isHidden = state->animState == AnimState::Hidden;

					if (state->isDirty || alphaChanged)
					{
						BLENDFUNCTION blendFunction = {};
						blendFunction.BlendOp             = AC_SRC_OVER;
						blendFunction.BlendFlags          = 0;
						blendFunction.SourceConstantAlpha = (u8) (255.f*newAlpha + .5f);
						blendFunction.AlphaFormat         = AC_SRC_ALPHA;

						POINT zeroPoint = {0, 0};

						success = UpdateLayeredWindow(
							state->hwnd,
							state->screenDC,
							&state->windowPosition, &state->windowSize,
							state->bitmapDC,
							&zeroPoint,
							CLR_INVALID,
							&blendFunction,
							ULW_ALPHA
						);
						// TODO: Warning

						state->isDirty = !success;
					}
				}

				return 0;
			}
			break;
		}
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}