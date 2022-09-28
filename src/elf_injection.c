#include "../include/elf_injection.h"
#include "../include/ptrace_injection.h"
#include "../include/global_data.h"

#include "stdio.h"
#include "stdlib.h"


int main(int argc, char *argv[], char *envp[])
{
    //-p pid libpath or -ps pid libpath
    if(1 == argc || (strcmp(argv[1], "-p") && strcmp(argv[1], "-ps"))){
        printf("please input -p or -ps");
        return 0;
    }
    //switch param
    if(!strcmp(argv[1], "-p")){
        pid_t pid = strtoul(argv[2], NULL, 10);
        char lib_path[PATH_MAX] = {0};
        strcpy(lib_path, argv[3]);
        void *remote_module_base = NULL;
        if(inject_remote_process(pid, lib_path, &remote_module_base)){
            printf("inject remote process is error!");
            return 0;
        }
        if(5 == argc){
            char remote_func_name[MAX_PROCNAME] = {0};
            strcpy(remote_func_name, argv[4]);
            u_long remote_module_func_ret = 0;
            if(call_remote_module_func(pid, remote_module_base, remote_func_name, NULL, 0, &remote_module_func_ret)){
                printf("remote func call is error!");
                return 0;
            }
        }
    }
    else if(!strcmp(argv[1], "-ps")){

    }

    return 0;
}