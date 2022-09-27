#include "../../include/ptrace_injection.h"
#include "../../include/global_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <error.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/system_properties.h>

//get module base address
void* get_module_base(pid_t pid, const char *module_name)
{
    if(0 == pid || module_name == NULL) return -1;
    
    void* module_base = -1;
    char file_name[PATH_MAX] = {0};
    sprintf(file_name, "/proc/%d/maps", pid);
    FILE* fd = NULL;
    fd = fopen(file_name, "r");
    if(NULL != fd){
        char file_data[1024] = {0};
        while(fgets(file_data, 1024, fd)){
            if(strstr(file_data, module_name)){
                char *addr = strtok(file_data, '-');
                module_base = strtoul(addr, NULL, 16);
                break;
            }
        }
        fclose(fd);
    }
    return module_base;
}


int ptrace_init()
{
    char system_version[PROP_VALUE_MAX] = {0};
    //get system version
    int ret = __system_property_get("ro.build.version.release", system_version);
    if(ret > 0){
        //if version < 10
        if(atoi(system_version) < 10){
            #ifdef  __arm__
            strcpy(g_libc_path, "/system/lib/libc.so");
            strcpy(g_linker_path, "/system/bin/linker");
            #elif   __aarch64__
            strcpy(g_libc_path, "/system/lib64/libc.so");
            strcpy(g_linker_path, "/system/bin/linker64");
            #endif
        }
        else{
            #ifdef  __arm__
            strcpy(g_libc_path, "/apex/com.android.runtime/lib/bionic/libc.so");
            strcpy(g_linker_path, "/apex/com.android.runtime/lib/bionic/libdl.so");
            #elif   __aarch64__
            strcpy(g_libc_path, "/apex/com.android.runtime/lib64/bionic/libc.so");
            strcpy(g_linker_path, "/apex/com.android.runtime/lib64/bionic/libdl.so");
            #endif
        }
    }
    return -1;
}


//read remote process's data of size
int ptrace_readdata(pid_t pid, uint8_t *source, uint8_t *dest, size_t size)
{
    if(0 == pid || NULL == source || NULL == dest)  return -1;

    int ret = 0;
    int word = __WORDSIZE / 8;
    for(int i = 0; i < size / word; i++){
        if(ptrace(PTRACE_PEEKDATA, pid, dest + i * word, source + i * word))    return -1;
    }
    ret = ptrace(PTRACE_POKEDATA, pid, dest + size - word - 1, source + size - word - 1);
    return ret;
}


//write remote process's data of size
int ptrace_writedata(pid_t pid, uint8_t *source, uint8_t *dest, size_t size)
{
    if(0 == pid || NULL == source || NULL == dest)  return -1;

    int ret = 0;
    int word = __WORDSIZE / 8;
    for(int i = 0; i < size / word; i++){
        if(ptrace(PTRACE_POKEDATA, pid, dest + i * word, source + i * word))    return -1;
    }
    ret = ptrace(PTRACE_POKEDATA, pid, dest + size - word - 1, source + size - word - 1);
    return ret;
}


//remote call pid's proc
#ifdef  __arm__
int ptrace_call(pid_t pid, void *remote_proc, const u_long *parameters, int param_num, struct pt_regs* regs)
{
    if(0 == pid || NULL == remote_proc || NULL == regs) return -1;

    

}
#elif   __aarch64__
int ptrace_call(pid_t pid, void *remote_proc )
{


}
#endif
