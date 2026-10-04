#include "viewer.h"

#define LOGGING
#include "platform/logging.h"

bool viewer_init() {
	LOG_INFO("Hello world from the viewer\n")
	return true;
}

bool viewer_run() {
	return true;
}

bool viewer_deinit() {
	return true;
}
