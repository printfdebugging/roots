#include "engine/platform/memory.h"

#if defined(__USE_POSIX)
#include <sys/mman.h>
#include <unistd.h>
#endif

#if defined(_WIN32)
#endif

#if defined(__USE_POSIX)
void *platform_reserve_memory(uint64_t size) {
	void *chunk = mmap(0, size, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (chunk == MAP_FAILED)
		return NULL;
	return chunk;
}

bool platform_commit_memory(void *chunk, uint64_t size) {
	return !mprotect(chunk, size, PROT_READ | PROT_WRITE);
}

void platform_free_memory(void *chunk, uint64_t size) {
	munmap(chunk, size);
}

int64_t platform_get_page_size() {
	return sysconf(_SC_PAGE_SIZE);
}

#endif

#if defined(_WIN32)
void *platform_reserve_memory(uint64_t size) {
}

bool platform_commit_memory(void *chunk, uint64_t size) {
}

void platform_free_memory(void *chunk, uint64_t size) {
}

int64_t platform_get_page_size() {
}
#endif
