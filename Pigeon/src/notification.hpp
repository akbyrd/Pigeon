#define RGBA(r, g, b, a) ((b << 0) | (g << 8) | (r << 16) | (a << 24))

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
	Error     error             = Error::None;
	c16*      text              = nullptr;
	b32       isDirty           = false;

	AnimState animState         = AnimState::Hidden;
	f64       animStartTick     = 0;
	f64       animShowTicks     = 0;
	f64       animIdleTicks     = 0;
	f64       animHideTicks     = 0;
	f64       tickFrequency     = 0;

	u16       windowMinWidth    = 0;
	u16       windowMaxWidth    = 0;
	SIZE      windowSize        = {};
	POINT     windowPosition    = {};

	COLORREF  backgroundColor   = {};
	COLORREF  textColorNormal   = {};
	COLORREF  textColorError    = {};
	COLORREF  textColorWarning  = {};
	u8        textPadding       = 0;

	HWND      hwnd              = nullptr;
	HDC       screenDC          = nullptr;
	HDC       bitmapDC          = nullptr;
	HFONT     font              = nullptr;
	HBITMAP   bitmap            = nullptr;
	u32*      pixels            = nullptr;
	u64       timerID           = 0;

	HFONT     previousFont      = nullptr;
	HBITMAP   previousBitmap    = nullptr;

	f64 appStartTimeMS = 0;
};

void
Notify(Notification* state, c16* text, Error error = Error::None)
{
	i32 iResult;
	u64 uResult;
	b32 success;

	// TODO: Queue notifications?
	// TODO: Handle this better so we can Notify errors from within this function
	b32 isNewHigherPriority = state->error == Error::None || error > state->error;
	b32 hasOldBeenSeen = state->animState == AnimState::Hiding || state->animState == AnimState::Hidden;

	if (isNewHigherPriority || hasOldBeenSeen)
	{
		state->isDirty = true;
		state->error = error;
		state->text = text;


		// Resize
		RECT textSizeRect = {};
		iResult = DrawTextW(
			state->bitmapDC,
			state->text, -1,
			&textSizeRect,
			DT_CALCRECT | DT_SINGLELINE
		);
		//if (result == 0) break;
		//TODO: Error

		i32 textWidth = textSizeRect.right - textSizeRect.left;

		u16 windowWidth = textWidth + 2*state->textPadding;
		if (windowWidth < state->windowMinWidth) windowWidth = state->windowMinWidth;
		if (windowWidth > state->windowMaxWidth) windowWidth = state->windowMaxWidth;

		if (state->windowSize.cx != windowWidth)
		{
			state->windowSize.cx = windowWidth;

			success = SetWindowPos(
				state->hwnd,
				nullptr,
				state->windowPosition.x,
				state->windowPosition.y,
				state->windowSize.cx,
				state->windowSize.cy,
				SWP_DEFERERASE | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOREPOSITION | SWP_NOZORDER
			);
			//if (!success) break;
			//TODO: Error
		}


		// Background
		for (u16 y = 0; y < state->windowSize.cy; y++)
		{
			u32* row = state->pixels + y*state->windowSize.cx;
			for (u16 x = 0; x < state->windowSize.cx; x++)
			{
				u32* pixel = row + x;
				*pixel = state->backgroundColor;
			}
		}


		// Text
		COLORREF newColor = {};
		switch (state->error)
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
			//TODO: Warning
			//GetLastError
			//L"Failed to set text color."
		}

		RECT textRect = {};
		textRect.left   = state->textPadding;
		textRect.top    = 0;
		textRect.right  = state->windowSize.cx - state->textPadding;
		textRect.bottom = state->windowSize.cy;

		iResult = DrawTextW(
			state->bitmapDC,
			state->text, -1,
			&textRect,
			DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS
		);
		//if (result == 0) break;
		//TODO: Error


		//TODO: Only do text rect
		// Patch text alpha
		u32 fullA = RGBA(0, 0, 0, 255);
		for (u16 y = 0; y < state->windowSize.cy; y++)
		{
			u32* row = state->pixels + y*state->windowSize.cx;
			for (u16 x = 0; x < state->windowSize.cx; x++)
			{
				u32* pixel = row + x;
				if (*pixel != state->backgroundColor)
					*pixel |= fullA;
			}
		}

		// Animate
		LARGE_INTEGER currentTicksRaw = {};
		success = QueryPerformanceCounter(&currentTicksRaw);
		// TODO: Warning
		// GetLastError()

		f64 currentTicks = (f64) currentTicksRaw.QuadPart;

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
				state->animStartTick = currentTicks - ((1 - normalizedTimeInState) * state->animShowTicks);

				// TODO: This will overshoot by an amount based on the animation step duration
				uResult = SetTimer(state->hwnd, state->timerID, 33, nullptr);
				//if (result == 0)
				// TODO: Error
				// GetLastError

				break;
			}

			case AnimState::Hidden:
			{
				state->animState = AnimState::Showing;
				state->animStartTick = currentTicks;

				success = ShowWindow(state->hwnd, SW_SHOW);
				//if (!success) return;
				//TODO: Error

				// TODO: This will overshoot by an amount based on the animation step duration
				uResult = SetTimer(state->hwnd, state->timerID, 33, nullptr);
				//if (result == 0)
				// TODO: Error
				// GetLastError

				break;
			}

			default: Assert(L"Missing AnimState case");
		}

		// NOTE: Update the window immediately, without worrying about USER_TIMER_MINIMUM
		success = PostMessageW(state->hwnd, WM_TIMER, state->timerID, NULL);
		//if (result == 0) return;
		//TODO: Error
	}
}

LRESULT CALLBACK
NotificationWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	b32 success;
	u64 uResult;

	auto notification = (Notification*) GetWindowLongPtrW(hwnd, GWLP_USERDATA);

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

		case WM_TIMER:
		{
			if (wParam == notification->timerID)
			{
				// TODO: Cleanup
				b32 changed = true;
				b32 alphaChanged = false;

				while (changed)
				{
					changed = false;

					LARGE_INTEGER currentTicksRaw = {};
					success = QueryPerformanceCounter(&currentTicksRaw);
					// TODO: Warning
					// GetLastError()

					f64 currentTicks = (f64) currentTicksRaw.QuadPart;
					f64 animTicks = currentTicks - notification->animStartTick;

					f32 newAlpha = 1;
					switch (notification->animState)
					{
						case AnimState::Showing:
						{
							alphaChanged = true;

							if (animTicks < notification->animShowTicks)
							{
								// TODO: Do we want something other than linear?
								newAlpha = (f32) (animTicks / notification->animShowTicks);
							}
							else
							{
								f64 overshootTicks = animTicks - notification->animShowTicks;

								notification->animState = AnimState::Shown;
								notification->animStartTick = currentTicks - overshootTicks;

								changed = true;
								continue;
							}
							break;
						}

						case AnimState::Shown:
						{
							if (animTicks < notification->animIdleTicks)
							{
								// TODO: This is happening more than it should
								newAlpha = 1;

								u32 remainingMS = (u32) ((notification->animIdleTicks - animTicks) / notification->tickFrequency) * 1000;
								uResult = SetTimer(hwnd, notification->timerID, remainingMS, nullptr);
								//if (result == 0)
								// TODO: Error
								// GetLastError
							}
							else
							{
								f64 overshootTicks = animTicks - notification->animIdleTicks;

								notification->animState = AnimState::Hiding;
								notification->animStartTick = currentTicks - overshootTicks;

								changed = true;
								continue;
							}

							break;
						}

						case AnimState::Hiding:
						{
							alphaChanged = true;

							if (animTicks < notification->animHideTicks)
							{
								// TODO: Do we want something other than linear?
								newAlpha = (f32) (1. - animTicks / notification->animHideTicks);
							}
							else
							{
								f64 overshootTicks = animTicks - notification->animHideTicks;

								notification->animState = AnimState::Hidden;
								notification->animStartTick = currentTicks - overshootTicks;

								changed = true;
								continue;
							}

							break;
						}

						case AnimState::Hidden:
						{
							notification->error = Error::None;

							newAlpha = 0;

							success = KillTimer(hwnd, notification->timerID);
							// TODO: Error

							success = ShowWindow(hwnd, SW_HIDE);
							// TODO: Error

							break;
						}

						default: Assert(L"Missing AnimState case");
					}

					bool isHidden = notification->animState == AnimState::Hidden;

					if (notification->isDirty || alphaChanged)
					{
						BLENDFUNCTION blendFunction = {};
						blendFunction.BlendOp             = AC_SRC_OVER;
						blendFunction.BlendFlags          = 0;
						blendFunction.SourceConstantAlpha = (u8) (255.f * newAlpha);
						blendFunction.AlphaFormat         = AC_SRC_ALPHA;

						POINT zeroPoint = {0, 0};

						success = UpdateLayeredWindow(
							notification->hwnd,
							notification->screenDC,
							&notification->windowPosition, &notification->windowSize,
							notification->bitmapDC,
							&zeroPoint,
							CLR_INVALID,
							&blendFunction,
							ULW_ALPHA
						);
						//TODO: Warning

						notification->isDirty = !success;
					}
				}
				return 0;
			}
			break;
		}
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}