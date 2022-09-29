#include "../../include/ptrace_injection.h"
#include "../../include/global_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <error.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
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
                u_long index = strchr(file_data, '-');
                file_data[index - (u_long)file_data] = 0;
                module_base = strtoul(file_data, NULL, 16);
                break;
            }
        }
        fclose(fd);
    }
    return module_base;
}

//init
int ptrace_init(pid_t remote_pid)
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

        //get mmap，dlopen，dlsym，dlclose proc's address
        ret = -1;
        g_current_libc_base = get_module_base(getpid(), g_libc_path);
        g_current_linker_base = get_module_base(getpid(), g_linker_path);
        g_remote_libc_base  = get_module_base(remote_pid, g_libc_path);
        g_remote_linker_base  = get_module_base(remote_pid, g_linker_path);
        if(g_current_libc_base && g_current_linker_base && g_remote_libc_base && g_remote_linker_base){
            remote_dlopen = (u_long)g_remote_linker_base + ((u_long)dlopen - (u_long)g_current_linker_base);
            remote_dlsym = (u_long)g_remote_linker_base + ((u_long)dlsym - (u_long)g_current_linker_base);
            remote_dlclose = (u_long)g_remote_linker_base + ((u_long)dlclose - (u_long)g_current_linker_base);
            remote_mmap = (u_long)g_remote_libc_base + ((u_long)mmap - (u_long)g_current_libc_base);
            remote_munmap = (u_long)g_remote_libc_base + ((u_long)munmap - (u_long)g_current_libc_base);
            ret = 0;
        }
    }
    
    return ret;
}


//read remote process's data of size
int ptrace_readdata(pid_t pid, uint8_t *dest, uint8_t *source, size_t size)
{
    if(0 == pid || NULL == source || NULL == dest)  return -1;

    int ret = 0;
    int word = __WORDSIZE / 8;
    for(int i = 0; i < size / word; i++){
        if(ptrace(PTRACE_PEEKDATA, pid, source + i * word, dest + i * word))    return -1;
    }
    ret = ptrace(PTRACE_POKEDATA, pid, source + size - word, dest + size - word);
    return ret;
}


//write remote process's data of size
int ptrace_writedata(pid_t pid, uint8_t *dest, uint8_t *source, size_t size)
{
    if(0 == pid || NULL == source || NULL == dest)  return -1;

    int ret = 0;
    int word = __WORDSIZE / 8;
    for(int i = 0; i < size / word; i++){
        if(ptrace(PTRACE_POKEDATA, pid, dest + i * word, *(u_long*)(source + i * word)))    return -1;
    }
    ret = ptrace(PTRACE_POKEDATA, pid, dest + size - word, *(u_long*)(source + size - word));
    return ret;
}


//remote call pid's proc
#ifdef  __arm__
int ptrace_call(pid_t pid, void *remote_proc, const u_long *parameters, int param_num, struct pt_regs* regs, u_long* remote_proc_ret)
{
    if(0 == pid || NULL == remote_proc  || NULL == regs || NULL == remote_proc_ret) return -1;

    int ret = 0;
    if(param_num <= 4){
        for(int i = 0; i < param_num; i++){
            regs->uregs[i] = parameters[i];
        }
    }
    else if(param_num > 4){
        for(int i = 0; i < 4; i++){
            regs->uregs[i] = parameters[i];
        }
        for(int i = 1; i <= param_num -4; i++){
            ret = ptrace(PTRACE_POKEDATA, pid, regs->uregs[13] - 4 * i, NULL);
            if(ret)    break;
            regs->uregs[13] = regs->uregs[13] - 4;      //sp
        }
    }

    if(!ret){
        regs->uregs[15] = remote_proc;  //pc
        regs->uregs[14] = 0;            //lr
        if(regs->uregs[15] & 1){
            //thumb
            regs->uregs[15] &= (~1u);
            regs->uregs[16] |= CPSR_T_MASK;
        }
        else{
            //arm
            regs->uregs[16] &= (~CPSR_T_MASK);
        }
        //set regs
        ret = ptrace(PTRACE_SETREGS, pid, NULL, regs);
        if(!ret){
            ret = ptrace(PTRACE_CONT, pid, NULL, NULL);
            if(!ret){
                int status = 0;
                waitpid(pid, &status, WUNTRACED);
                if(0xb7f == status){
                    //get remote proc return
                    ret = ptrace(PTRACE_GETREGS, pid, NULL, regs);
                    *remote_proc_ret = regs->uregs[0];
                }
            }
        }

    }
    return ret;

}
#elif   __aarch64__
int ptrace_call(pid_t pid, void *remote_proc )
{


}
#endif

//injection remote process
int inject_remote_process(pid_t pid, const char *lib_path, void * remote_module_base)
{
    if(0 == pid || NULL == lib_path || NULL == remote_module_base)    return -1;

    //init
    if(ptrace_init(pid)){
        return -1;
    }
    //attach to remote process
    if(ptrace(PTRACE_ATTACH, pid, NULL, NULL)){
        return -1;
    }
    waitpid(pid, NULL, WUNTRACED);
    //get registers
    struct pt_regs old_regs = {0};
    if(ptrace(PTRACE_GETREGS, pid, NULL, &old_regs)){
        return -1;
    }
    //remote call mmap
    u_long remote_map_addr = 0;
    struct pt_regs new_regs = old_regs;
    u_long  parameters[6] = {0};
    parameters[0] = NULL;           //addr
    parameters[1] = 0x1000;         //size
    parameters[2] = PROT_READ | PROT_WRITE | PROT_EXEC; //port: rwx
    parameters[3] = MAP_PRIVATE | MAP_ANONYMOUS;        //flags: copy-write
    parameters[4] = NULL;           //fd
    parameters[5] = NULL;           //offset
    int ret = ptrace_call(pid, remote_mmap, parameters, 6, &new_regs, &remote_map_addr);
    if(ret || !remote_map_addr){
        return -1;
    }
    //write to remote process of injection lib path
    ret = ptrace_writedata(pid, remote_map_addr, lib_path, strlen(lib_path));
    if(ret){
        return -1;
    }
    //remote call dlopen
    void *inject_module_base = NULL;
    parameters[0] = remote_map_addr;            //file_name
    parameters[1] = RTLD_NOW | RTLD_GLOBAL;     //flags
    ret = ptrace_call(pid, remote_dlopen, parameters, 2, &new_regs, &inject_module_base);
    if(ret || !inject_module_base){
        return -1;
    }
    //remote call munmap
    int munmap_ret = 0;
    parameters[0] = remote_map_addr;            //addr
    parameters[1] = 0x1000;                     //size
    ret = ptrace_call(pid, remote_munmap, parameters, 2, &new_regs, &munmap_ret);
    if(ret || munmap_ret){
        return -1;
    }
    //set regs
    if(ptrace(PTRACE_SETREGS, pid, NULL, &old_regs)){
        return -1;
    }
    //detach remote process
    if(ptrace(PTRACE_DETACH, pid, NULL, NULL)){
        return -1;
    }

    *(u_long*)remote_module_base = inject_module_base;
    return 0;
}


//call remote module's func
int call_remote_module_func(
    pid_t pid, 
    void *remote_module_base, 
    const char *remote_module_func, 
    const char *remote_module_func_parameters, 
    u_long param_num, 
    u_long *remote_module_func_ret)
{
    if(0 == pid || NULL == remote_module_base || NULL == remote_module_func || param_num > MAX_PARAMENUM)    return -1;

    //init
    if(ptrace_init(pid)){
        return -1;
    }
    //attach to remote process
    if(ptrace(PTRACE_ATTACH, pid, NULL, NULL)){
        return -1;
    }
    waitpid(pid, NULL, WUNTRACED);
    //get registers
    struct pt_regs old_regs = {0};
    if(ptrace(PTRACE_GETREGS, pid, NULL, &old_regs)){
        return -1;
    }
    //remote call mmap
    u_long remote_map_addr = 0;
    struct pt_regs new_regs = old_regs;
    u_long  parameters[MAX_PARAMENUM] = {0};
    parameters[0] = NULL;           //addr
    parameters[1] = 0x1000;         //size
    parameters[2] = PROT_READ | PROT_WRITE | PROT_EXEC; //port: rwx
    parameters[3] = MAP_PRIVATE | MAP_ANONYMOUS;        //flags: copy-write
    parameters[4] = NULL;           //fd
    parameters[5] = NULL;           //offset
    int ret = ptrace_call(pid, remote_mmap, parameters, 6, &new_regs, &remote_map_addr);
    if(ret || !remote_map_addr){
        return -1;
    }
    //write to remote process of injection lib path
    ret = ptrace_writedata(pid, remote_map_addr, remote_module_func, strlen(remote_module_func));
    if(ret){
        return -1;
    }
    //remote call dlsym
    void *remote_module_func_addr = NULL;
    parameters[0] = remote_module_base;     //handle
    parameters[1] = remote_map_addr;        //symbol
    ret = ptrace_call(pid, remote_dlsym, parameters, 2, &new_regs, &remote_module_func_addr);
    if(ret || !remote_module_func_addr){
        return -1;
    }
    //remote call munmap
    int munmap_ret = 0;
    parameters[0] = remote_map_addr;            //addr
    parameters[1] = 0x1000;                     //size
    ret = ptrace_call(pid, remote_munmap, parameters, 2, &new_regs, &munmap_ret);
    if(ret || munmap_ret){
        return -1;
    }
    //remote call remote_module_func
    for(int i = 0; i < param_num; i++){
        parameters[i] = remote_module_func_parameters[i];
    }
    ret = ptrace_call(pid, remote_module_func_addr, parameters, param_num, &new_regs, remote_module_func_ret);
    if(ret){
        return -1;
    }
    //set regs
    if(ptrace(PTRACE_SETREGS, pid, NULL, &old_regs)){
        return -1;
    }
    //detach remote process
    if(ptrace(PTRACE_DETACH, pid, NULL, NULL)){
        return -1;
    }
    return 0;
}