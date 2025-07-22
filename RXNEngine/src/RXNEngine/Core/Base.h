#pragma once

#include <memory>

#ifdef RXN_DEBUG
	#if defined(RXN_PLATFORM_WINDOWS)
		#define HZ_DEBUGBREAK() __debugbreak()
	#elif defined(RXN_PLATFORM_LINUX)
		#include <signal.h>
	#define RXN_DEBUGBREAK() raise(SIGTRAP)
	#else
		#error "Platform doesn't support debugbreak yet!"
	#endif
	#define RXN_ENABLE_ASSERTS
#else
	#define RXN_DEBUGBREAK()
#endif

#ifdef RXN_ENABLE_ASSERTS
#define RXN_ASSERT(x, ...) { if(!(x)) { RXN_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#define RXN_CORE_ASSERT(x, ...) { if(!(x)) { RXN_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
//#define RXN_CORE_ASSERT(x) { if(!(x)) { RXN_CORE_ERROR("Assertion Failed: {0}"); __debugbreak(); } }
#else
#define RXN_ASSERT(x, ...)
#define RXN_CORE_ASSERT(x, ...)
//#define RXN_CORE_ASSERT(x)
#endif

#define BIT(x) (1 << x)


namespace RXNEngine {

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T>
	using Scope = std::unique_ptr<T>;
}