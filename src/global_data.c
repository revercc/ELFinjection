#include "../include/global_data.h"

char g_libc_path[PATH_MAX] = {0};
char g_linker_path[PATH_MAX] = {0};

void *remote_dlopen = 0;
void *remote_dlsym  = 0;
void *remote_dlclose = 0;
void *remote_mmap   = 0;
void *remote_munmap  = 0;
void *g_current_linker_base = 0;
void *g_current_libc_base   = 0;
void *g_remote_linker_base  = 0;
void *g_remote_libc_base    = 0;
