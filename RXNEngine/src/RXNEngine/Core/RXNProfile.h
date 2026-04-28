#pragma once

#ifdef TRACY_ENABLE
	#include <tracy/Tracy.hpp>

	#define RXN_PROFILE_FRAME()           FrameMark
	#define RXN_PROFILE_SCOPE()           ZoneScoped
	#define RXN_PROFILE_SCOPE_NAMED(name) ZoneScopedN(name)
    #define RXN_PROFILE_THREAD(name)      tracy::SetThreadName(name)
#else
	#define RXN_PROFILE_FRAME()
	#define RXN_PROFILE_SCOPE()
	#define RXN_PROFILE_SCOPE_NAMED(name)
    #define RXN_PROFILE_THREAD(name)
#endif