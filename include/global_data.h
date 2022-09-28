#pragma once

#include <unistd.h>
#define MAX_PARAMENUM   10
#define MAX_PROCNAME    100
extern char g_libc_path[PATH_MAX];
extern char g_linker_path[PATH_MAX];

extern void *remote_dlopen;
extern void *remote_dlsym;
extern void *remote_dlclose;
extern void *remote_mmap;
extern void *remote_munmap;
extern void *g_current_linker_base;
extern void *g_current_libc_base;
extern void *g_remote_linker_base;
extern void *g_remote_libc_base;