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
        int ret = inject_remote_process(pid, lib_path);
        if(ret){
            printf("inject remote process is error!");
        }
    }
    else if(!strcmp(argv[1], "-ps")){

    }

    return 0;
}