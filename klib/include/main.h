#pragma once

#include <fltKernel.h>
#include <wdm.h>

#define KERNEL_LIB_TAG 'vrdl' // ldrv

#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#define __FILENAMEW__ (wcsrchr(_CRT_WIDE(__FILE__), L'\\') ? wcsrchr(_CRT_WIDE(__FILE__), L'\\') + 1 : _CRT_WIDE(__FILE__))

#define LOG(Level, Format, ...) {DbgPrint("["##Level"] FILE:%s, LINE:%d, "##Format".\r\n", __FILENAME__, __LINE__, __VA_ARGS__);}
#define LOG_ERROR(Format, ...) LOG("ERROR", Format, __VA_ARGS__)
#define LOG_INFO(Format, ...) LOG("INFO", Format, __VA_ARGS__)

#define ALIGN __declspec(align(MEMORY_ALLOCATION_ALIGNMENT))