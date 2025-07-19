#pragma once

#include <memory>

#ifdef RXN_PLATFORM_WINDOWS
#if RXN_DYNAMIC_LINK
		#ifdef RXN_BUILD_DLL
			#define RXN_API __declspec(dllexport)
		#else 
			#define RXN_API __declspec(dllimport)
		#endif // RXN_BUILD_DLL
#else
		#define RXN_API
#endif
#endif // RXN_PLATFORM_WINDOWS

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