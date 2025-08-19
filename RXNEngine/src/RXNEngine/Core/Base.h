#pragma once

#include "RXNEngine/Core/PlatformDetection.h"

#include <memory>

#ifdef RXN_DEBUG
	#if defined(RXN_PLATFORM_WINDOWS)
		#define RXN_DEBUGBREAK() __debugbreak()
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


#define RXN_EXPAND_MACRO(x) x
#define RXN_STRINGIFY_MACRO(x) #x

#define BIT(x) (1 << x)


namespace RXNEngine {

	template<typename T>
	using Ref = std::shared_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Ref<T> CreateRef(Args&& ... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}


	template<typename T>
	using Scope = std::unique_ptr<T>;
	template<typename T, typename ... Args>
	constexpr Scope<T> CreateScope(Args&& ... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}

}