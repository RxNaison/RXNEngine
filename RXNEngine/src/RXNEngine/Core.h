#pragma once

#ifdef RXN_PLATFORM_WINDOWS
		#ifdef RXN_BUILD_DLL
			#define RXN_API __declspec(dllexport)
		#else 
			#define RXN_API __declspec(dllimport)
		#endif // RXN_BUILD_DLL

#endif // RXN_PLATFORM_WINDOWS

#define BIT(x) (1 << x)