#include "include/Platform.h"

INTERNALSCOPE double s_SystemClockFrequency;
INTERNALSCOPE LARGE_INTEGER s_SystemClockStartTime;

static auto AutoInitializePlatformFunction = []() -> bool {
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	s_SystemClockFrequency = 1.0 / (double)frequency.QuadPart;
	QueryPerformanceCounter(&s_SystemClockStartTime);

	return true;
};

INTERNALSCOPE bool Initialize = AutoInitializePlatformFunction();

double Platform::GetAbsoluteTime()
{
	LARGE_INTEGER currentTime;
	QueryPerformanceCounter(&currentTime);
	return static_cast<double>(currentTime.QuadPart) * s_SystemClockFrequency;
}
