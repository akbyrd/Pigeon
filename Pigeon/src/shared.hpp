#include <cstdint>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef char    c8;
typedef wchar_t c16;

typedef int b32;

typedef size_t index;

#define Assert(x) {if (!(x)) *((u8*) 0) = 0;}
#define ArrayCount(x) sizeof(x) / sizeof(x[0])

enum struct Event
{
	MessagePump,
	Notification,
	UpdateStart,
	UpdateEnd,
	Showing,
	Shown,
	Hiding,
};

struct LogEntry
{
	f64   ticks;
	Event event;
};

LogEntry eventLog[256];
u16 eventLogCount;

inline void
LogEvent(f64 ticks, Event event)
{
	Assert(eventLogCount < ArrayCount(eventLog));
	eventLog[eventLogCount++] = {ticks, event};
}

inline void
LogEvent(Event event)
{
	LARGE_INTEGER currentTicks = {};
	QueryPerformanceCounter(&currentTicks);

	LogEvent((f64) currentTicks.QuadPart, event);
}

void PrintLog()
{
	LARGE_INTEGER tickFrequencyRaw = {};
	QueryPerformanceFrequency(&tickFrequencyRaw);
	f64 tickFrequency = (f64) tickFrequencyRaw.QuadPart;

	f64 lastTimeMS = 0;
	for (u16 i = 0; i < eventLogCount; i++)
	{
		f64 timeMS = eventLog[i].ticks / tickFrequency * 1000.;

		f64 deltaT = timeMS - lastTimeMS;
		lastTimeMS = timeMS;
	}

	eventLogCount = 0;
}