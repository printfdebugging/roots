#ifndef PLATFORM_LOGGING_H
#define PLATFORM_LOGGING_H

#ifdef LOGGING
#include <stdio.h>
#endif

#ifdef LOGGING
	#define LOG_INFO(...) fprintf(stderr, __VA_ARGS__);
	#define LOG_EVENT(...) fprintf(stderr, __VA_ARGS__);
#else
	#define LOG_INFO(...)
	#define LOG_EVENT(...)
#endif

#endif
