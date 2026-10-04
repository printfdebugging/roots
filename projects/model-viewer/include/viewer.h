#ifndef VIEWER_H
#define VIEWER_H

struct viewer {
	bool initialized;
};

bool viewer_init();
bool viewer_run();
bool viewer_deinit();

#endif
