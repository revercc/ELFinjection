#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define CPSR_T_MASK (1u << 5)

//get module base address
void* get_module_base(pid_t pid, const char *module_name);
//init
int ptrace_init(pid_t pid);
//read remote process's data of size
int ptrace_readdata(pid_t pid, uint8_t *source, uint8_t *dest, size_t size);
//write remote process's data of size
int ptrace_writedata(pid_t pid, uint8_t *source, uint8_t *dest, size_t size);


//injection remote process
int inject_remote_process(pid_t pid, const char *lib_path);
//call remote module's func
int call_remote_module_func(
    pid_t pid, 
    void *remote_module_base, 
    const char *remote_module_func, 
    const char *remote_module_func_parameters, 
    u_long param_num, 
    u_long *remote_module_func_ret);