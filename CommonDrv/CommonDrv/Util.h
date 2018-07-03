#pragma once
#include <fltKernel.h>
/// Sets a break point that works only when a debugger is present
#if !defined(HYPERPLATFORM_COMMON_DBG_BREAK)
#define COMMON_DBG_BREAK() \
  if (KD_DEBUGGER_NOT_PRESENT) {         \
  } else {                               \
    __debugbreak();                      \
  }                                      \
  reinterpret_cast<void*>(0)
#endif

#define COMMON_DEBUG_INFO(format, ...) DbgPrintEx(0,0,format,__VA_ARGS__)

extern "C" {
	NTSTATUS UtilForEachProcessor(
		_In_ NTSTATUS(*callback_routine)(void *),
		void *context
	);

	PVOID UtilPcToFileHeader(
		_In_ PVOID pc_value
	);
}