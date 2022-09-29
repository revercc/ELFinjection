#include "../../include/rely_injection.h"
#include "../../include/global_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <string.h>
#include <elf.h>

//get new file size after a rely
size_t get_new_file_size(FILE *fd, u_long align_size)
{   
    if(NULL == fd || align_size < 0x1000)    return -1;

    align_size = (align_size % 0x1000) ? (align_size / 0x1000 * 0x1000 + 0x1000) : (align_size / 0x1000 * 0x1000);
    //get file size
    size_t file_size = 0;
    fseek(fd, NULL, SEEK_END);
    file_size = ftell(fd);
    fseek(fd, NULL, SEEK_SET);
    //read file to buffer
    uint8_t *file_buffer = (uint8_t*)malloc(file_size);
    if(file_buffer){
        if(1 == fread(file_buffer, file_size, 1, fd)){
            Elf32_Ehdr* elf_header = (Elf32_Ehdr *)file_buffer;
            size_t new_phentsize = elf_header->e_phentsize + sizeof(Elf32_Phdr);
            size_t new_dynamic_size = 0;
            size_t new_dynstr_size = 0;
            Elf32_Phdr * program_header = NULL; 
            for(int i = 0; i < elf_header->e_phnum; i++){
                program_header = file_buffer + elf_header->e_phoff + i * sizeof(Elf32_Phdr);
                if(PT_DYNAMIC == program_header->p_type){
                    new_dynamic_size = program_header->p_filesz + sizeof(Elf32_Dyn);

                    Elf32_Dyn* dynamic_them = 0;
                    for(int i = 0; i * sizeof(Elf32_Dyn) < program_header->p_filesz; i++){
                        dynamic_them = file_buffer + program_header->p_offset;
                        if(DT_STRTAB == dynamic_them->d_tag){
                            dynamic_them->d_un.d_ptr;
                        }
                    }
                    
                    break;
                }
            }

        }
        free(file_buffer);
    }

    return -1;
}
//open file and align file
FILE *file_align(const char *file_path)
{
    if(NULL == file_path)   return -1;

    FILE *fd = fopen(file_path, "r+");
    if(fd){

    }
    return -1;
}

int add_rely_lib(const char *dest_lib_path, const char *source_lib_path)
{
    if(NULL == dest_lib_path || NULL == source_lib_path)    return -1;

    FILE *fd = fopen(dest_lib_path, "r+");
    if(!fd){
        return -1;
    }
    //计算增加新的依赖项需要额外增加多少空间
    size_t new_fize_size = get_new_file_size(dest_lib_path, 0x1000);
    //文件末尾进行填充对齐（粒度为0x1000
    FILE *fd = file_align(dest_lib_path);

    //将.dynamic节区移动到文件末尾并在开头增加新的一项（DT_NEEDED

    //将program header tabel移动到文件末尾并在尾部增加新的一项（PT_LOAD类型的项

    //将.dynstr移动到文件尾部并在末尾增加一个字符串（需要注入的lib的路径

    //修正各个字段

}