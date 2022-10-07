#include "../../include/rely_injection.h"
#include "../../include/global_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <string.h>
#include <elf.h>

//get new file size after add a rely
size_t get_new_file_size(
    FILE *fd, 
    size_t file_size, 
    const char *lib_path, 
    u_long align_size, 
    u_long *p_new_load_program_off, 
    u_long *p_new_load_program_size,
    u_long *p_new_dynamic_off,
    u_long *p_new_dynamic_size,
    u_long *p_new_program_header_table_off,
    u_long *p_new_program_header_table_size,
    u_long *p_new_dynstr_off,
    u_long *p_new_dynstr_size)
{   
    if(
        NULL == fd || 
        NULL == lib_path || 
        align_size < 0x1000 || 
        NULL == p_new_load_program_off || 
        NULL == p_new_load_program_size ||
        NULL == p_new_dynamic_off || 
        NULL == p_new_dynamic_size || 
        NULL == p_new_program_header_table_off ||
        NULL == p_new_program_header_table_size ||
        NULL == p_new_dynstr_off || 
        NULL == p_new_dynstr_size)    
        return -1;

    size_t ret = -1;
    size_t align_file_size = 0;
    align_size = (align_size % 0x1000) ? (align_size / 0x1000 * 0x1000 + 0x1000) : (align_size / 0x1000 * 0x1000);
    align_file_size = file_size + align_size - (file_size % align_size);
    //read file to buffer
    uint8_t *file_buffer = (uint8_t*)malloc(file_size);
    if(file_buffer){
        memset(file_buffer, 0, file_size);
        fseek(fd, 0, SEEK_SET);
        if(1 == fread(file_buffer, file_size, 1, fd)){
            Elf32_Ehdr* elf_header = (Elf32_Ehdr *)file_buffer;
            size_t new_program_header_table_size = elf_header->e_phentsize * elf_header->e_phnum + sizeof(Elf32_Phdr);
            size_t new_dynamic_size = 0;
            size_t new_dynstr_size = 0;
            Elf32_Phdr * program_header = NULL; 
            for(int i = 0; i < elf_header->e_phnum; i++){
                program_header = file_buffer + elf_header->e_phoff + i * sizeof(Elf32_Phdr);
                if(PT_DYNAMIC == program_header->p_type){
                    new_dynamic_size = program_header->p_filesz + sizeof(Elf32_Dyn);
                    Elf32_Dyn* dynamic_them = NULL;
                    for(int i = 0; i * sizeof(Elf32_Dyn) < program_header->p_filesz; i++){
                        dynamic_them = file_buffer + program_header->p_offset + i * sizeof(Elf32_Dyn);
                        if(DT_STRTAB == dynamic_them->d_tag){
                            uint8_t *dynstr_them = file_buffer + dynamic_them->d_un.d_ptr + 1;
                            new_dynstr_size += 1;
                            while(*dynstr_them){
                                new_dynstr_size = new_dynstr_size + strlen(dynstr_them) + 1;
                                dynstr_them = dynstr_them + strlen(dynstr_them) + 1;
                            }
                            new_dynstr_size = new_dynstr_size + strlen(lib_path) + 2;
                            break;
                        }
                    }
                    break;
                }
            }
            //get new file size
            if(new_program_header_table_size && new_dynamic_size && new_dynstr_size){
                *p_new_load_program_off = align_file_size;
                *p_new_load_program_size = new_program_header_table_size + new_dynamic_size + new_dynstr_size;
                *p_new_dynamic_off = align_file_size;
                *p_new_dynamic_size = new_dynamic_size;
                *p_new_program_header_table_off = align_file_size + new_dynamic_size;
                *p_new_program_header_table_size = new_program_header_table_size;
                *p_new_dynstr_off = align_file_size + new_dynamic_size + new_program_header_table_size;
                *p_new_dynstr_size = new_dynstr_size;
                ret = align_file_size + new_program_header_table_size + new_dynamic_size + new_dynstr_size;
            }
        }
        free(file_buffer);
    }

    return ret;
}
//move dynstr to file end
int move_dynstr(FILE *fd, uint8_t *new_file_buffer, u_long new_dynstr_off, const char *lib_path, u_long *p_lib_path_index)
{
    int ret = -1;
    if(NULL == fd || NULL == new_file_buffer || NULL == lib_path || NULL == p_lib_path_index)   return -1;

    Elf32_Ehdr* elf_header = (Elf32_Ehdr *)new_file_buffer;    
    Elf32_Phdr * program_header = NULL; 
    for(int i = 0; i < elf_header->e_phnum; i++){
        program_header = new_file_buffer+ elf_header->e_phoff + i * sizeof(Elf32_Phdr);
        if(PT_DYNAMIC == program_header->p_type){
            Elf32_Dyn* dynamic_them = 0;
            for(int i = 0; i * sizeof(Elf32_Dyn) < program_header->p_filesz; i++){
                dynamic_them = new_file_buffer+ program_header->p_offset + i * sizeof(Elf32_Dyn);
                if(DT_STRTAB == dynamic_them->d_tag){
                    uint8_t *dynstr_them = new_file_buffer + dynamic_them->d_un.d_ptr + 1; 
                    uint8_t *new_dynstr_them = new_file_buffer + new_dynstr_off + 1;    
                    while(*(dynstr_them)){
                        strcpy(new_dynstr_them, dynstr_them);
                        new_dynstr_them = new_dynstr_them + strlen(dynstr_them) + 1;
                        dynstr_them = dynstr_them + strlen(dynstr_them) + 1; 
                    }
                    strcpy(new_dynstr_them, lib_path);
                    *p_lib_path_index = new_dynstr_them - (new_file_buffer + new_dynstr_off);
                    ret = 0;
                    break;
                }
            }
            break;
        }
    }


    return ret;
}
//move dynamic to file end
int move_dynamic(FILE *fd, uint8_t *new_file_buffer, u_long new_dynamic_off, u_long lib_path_index)
{
    int ret = -1;
    if(NULL == fd || NULL == new_file_buffer)   return -1;

    Elf32_Ehdr* elf_header = (Elf32_Ehdr *)new_file_buffer;
    Elf32_Phdr * program_header = NULL; 
    for(int i = 0; i < elf_header->e_phnum; i++){
        program_header = new_file_buffer + elf_header->e_phoff + i * sizeof(Elf32_Phdr);
        if(PT_DYNAMIC == program_header->p_type){
            //build new DT_NEEDED
            Elf32_Dyn new_dynamic_them = {0};
            new_dynamic_them.d_tag = DT_NEEDED;
            new_dynamic_them.d_un.d_ptr = lib_path_index;
            *(Elf32_Dyn*)(new_file_buffer + new_dynamic_off) = new_dynamic_them;
            //copy old .dynamic program
            memcpy(new_file_buffer + new_dynamic_off + sizeof(Elf32_Dyn), new_file_buffer + program_header->p_offset, program_header->p_filesz);
            ret = 0;
            break;
        }
    }
    return ret;
}

//move program header table to file end
int move_program_header_table(FILE *fd, uint8_t *new_file_buffer, u_long new_program_header_table_off, u_long new_load_program_off, u_long new_load_program_size, u_long *p_new_load_program_addr)
{
    if(NULL == fd || NULL == new_file_buffer || NULL == p_new_load_program_addr)   return -1;

    Elf32_Ehdr *elf_header = (Elf32_Ehdr *)new_file_buffer;
    Elf32_Phdr *program_header = NULL;
    Elf32_Addr last_load_program_addr_end = 0;
    //get last load program addr end
    for(int i = 0; i < elf_header->e_phnum; i++){
        program_header = new_file_buffer + elf_header->e_phoff + i * sizeof(Elf32_Phdr);
        if(PT_LOAD == program_header->p_type){
            Elf32_Word aligned_size = 0;
            aligned_size = ((program_header->p_memsz % program_header->p_align) ? (program_header->p_memsz / program_header->p_align + 1) : (program_header->p_memsz / program_header->p_align)) * program_header->p_align;
            if(program_header->p_vaddr + aligned_size > last_load_program_addr_end){
                last_load_program_addr_end = program_header->p_vaddr + aligned_size;
            }
        }
    }
    //build new PT_LOAD 
    Elf32_Phdr new_program_header_them = {0};
    new_program_header_them.p_type = PT_LOAD;
    new_program_header_them.p_offset = new_load_program_off;
    new_program_header_them.p_vaddr = last_load_program_addr_end + PAGE_SIZE;
    new_program_header_them.p_paddr = last_load_program_addr_end + PAGE_SIZE;
    new_program_header_them.p_filesz = new_load_program_size;
    new_program_header_them.p_memsz = new_load_program_size;
    new_program_header_them.p_flags = PF_R | PF_W | PF_X;
    new_program_header_them.p_align = PAGE_SIZE;
    
    memcpy(new_file_buffer + new_program_header_table_off, new_file_buffer + elf_header->e_phoff, elf_header->e_phentsize * elf_header->e_phnum);
    *(Elf32_Phdr*)(new_file_buffer + new_program_header_table_off + (elf_header->e_phentsize * elf_header->e_phnum)) = new_program_header_them;
    *p_new_load_program_addr = ((last_load_program_addr_end % PAGE_SIZE) ? (last_load_program_addr_end / PAGE_SIZE + 1) : (last_load_program_addr_end / PAGE_SIZE)) * PAGE_SIZE;
    return 0;
}

//revise the elf file 
int revise_new_elf_file(
    FILE *fd,
    uint8_t *new_file_buffer,
    u_long new_load_program_off,
    u_long new_load_program_size,
    u_long new_load_program_addr,
    u_long new_program_header_table_off, 
    u_long new_program_header_table_size, 
    u_long new_program_header_table_addr, 
    u_long new_dynamic_off,
    u_long new_dynamic_size,
    u_long new_dynamic_addr,
    u_long new_dynstr_off,
    u_long new_dynstr_size,
    u_long new_dynstr_addr)
{
    if(NULL == fd || NULL == new_file_buffer)   return -1;

    //revise elf header
    Elf32_Ehdr *elf_header = (Elf32_Ehdr *)new_file_buffer;
    elf_header->e_phnum ++;
    elf_header->e_phoff = new_program_header_table_off;
    Elf32_Phdr *program_header = NULL;
    //revise program header table
    for(int i = 0; i < elf_header->e_phnum; i++){
        program_header = new_file_buffer + elf_header->e_phoff + i * sizeof(Elf32_Phdr);
        if(PT_PHDR == program_header->p_type){
            program_header->p_offset = new_program_header_table_off;
            program_header->p_vaddr = new_program_header_table_addr;
            program_header->p_paddr = new_program_header_table_addr;
            program_header->p_filesz = new_program_header_table_size;
            program_header->p_memsz = new_program_header_table_size;
        }
        else if(PT_DYNAMIC == program_header->p_type){
            program_header->p_offset = new_dynamic_off;
            program_header->p_vaddr = new_dynamic_addr;
            program_header->p_paddr = new_dynamic_addr;
            program_header->p_filesz = new_dynamic_size;
            program_header->p_memsz = new_dynamic_size;
            //revise dynamic's DT_STRTAB and DT_STRSZ
            Elf32_Dyn* dynamic_them = NULL;
            for(int i = 0; i * sizeof(Elf32_Dyn) < program_header->p_filesz; i++){
                dynamic_them = new_file_buffer+ program_header->p_offset + i * sizeof(Elf32_Dyn);
                if(DT_STRTAB == dynamic_them->d_tag){
                    dynamic_them->d_un.d_ptr = new_dynstr_addr;
                }
                else if(DT_STRSZ == dynamic_them->d_tag){
                    dynamic_them->d_un.d_val = new_dynstr_size;
                }
            }
        }
        else if(PT_LOAD == program_header->p_type){
            if((elf_header->e_phnum - 1) == i){
                program_header->p_offset = new_load_program_off;
                program_header->p_vaddr = new_load_program_addr;
                program_header->p_paddr = new_load_program_addr;
                program_header->p_filesz = new_load_program_size;
                program_header->p_memsz = new_load_program_size;
            }
        }
    }
    //revise section header table
    Elf32_Shdr *section_header = NULL;
    for(int i = 0; i < elf_header->e_shnum; i++){
        section_header = new_file_buffer + elf_header->e_shoff + i * sizeof(Elf32_Shdr);
        if(SHT_DYNAMIC == section_header->sh_type){
            section_header->sh_addr = new_dynamic_addr;
            section_header->sh_offset = new_dynamic_off;
            section_header->sh_size = new_dynamic_size;
        }
        //.dynstr and .shstrtab
        else if(SHT_STRTAB == section_header->sh_type && section_header->sh_flags){
            section_header->sh_addr = new_dynstr_addr;
            section_header->sh_offset = new_dynstr_off;
            section_header->sh_size = new_dynstr_size;
        }
    }
    
    return 0;
}

int add_rely_lib(const char *dest_lib_path, const char *source_lib_path)
{
    if(NULL == dest_lib_path || NULL == source_lib_path)    return -1;

    int ret = -1;
    FILE *fd = fopen(dest_lib_path, "r+");
    if(!fd){
        return -1;
    }
    size_t file_size = 0;
    fseek(fd, 0, SEEK_END);
    file_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    //计算增加新的依赖项需要额外增加多少空间
    #ifdef __arm__
    Elf32_Off new_load_program_off = 0;
    Elf32_Word new_load_program_size = 0;
    Elf32_Addr new_load_program_addr = 0;
    Elf32_Off new_dynamic_off = 0;
    Elf32_Word new_dynamic_size = 0;
    Elf32_Addr new_dynamic_addr = 0;
    Elf32_Off new_program_header_table_off = 0;
    Elf32_Word new_program_header_table_size = 0;
    Elf32_Addr new_program_header_table_addr = 0;
    Elf32_Off new_dynstr_off = 0;
    Elf32_Word new_dynstr_size = 0;
    Elf32_Addr new_dynstr_addr = 0;
    Elf32_Addr lib_path_index = 0;
    #elif __aarch64__
    Elf64_Off new_load_program_off = 0;
    Elf64_Word new_load_program_size = 0;
    Elf64_Addr new_load_program_addr = 0;
    Elf64_Off new_dynamic_off = 0;
    Elf64_Word new_dynamic_size = 0;
    Elf64_Addr new_dynamic_addr = 0;
    Elf64_Off new_program_header_table_off = 0;
    Elf64_Word new_program_header_table_size = 0;
    Elf64_Addr new_program_header_table_addr = 0;
    Elf64_Off new_dynstr_off = 0;
    Elf64_Word new_dynstr_size = 0;
    Elf64_Addr new_dynstr_addr = 0;
    Elf64_Addr lib_path_index = 0;
    #endif
    

    size_t new_file_size = get_new_file_size(
        fd, 
        file_size, 
        source_lib_path, 
        PAGE_SIZE, 
        &new_load_program_off, 
        &new_load_program_size,
        &new_dynamic_off,
        &new_dynamic_size,
        &new_program_header_table_off,
        &new_program_header_table_size,
        &new_dynstr_off,
        &new_dynstr_size
        );
    if(-1 != new_file_size){
        uint8_t *new_file_buffer = (uint8_t *)malloc(new_file_size);
        if(NULL != new_file_buffer){
            //read dest file
            memset(new_file_buffer, 0, new_file_size);
            fseek(fd, 0, SEEK_SET);
            if(1 == fread(new_file_buffer, file_size, 1, fd)){
                //将.dynstr移动到文件尾部并在末尾增加一个字符串（需要注入的lib的路径
                if(0 == move_dynstr(fd, new_file_buffer, new_dynstr_off, source_lib_path, &lib_path_index) &&
                    //将.dynamic节区移动到文件末尾并在开头增加新的一项（DT_NEEDED 
                   0 == move_dynamic(fd, new_file_buffer, new_dynamic_off, lib_path_index) &&
                    //将program header tabel移动到文件末尾并在尾部增加新的一项（PT_LOAD类型的项
                   0 == move_program_header_table(fd, new_file_buffer, new_program_header_table_off, new_load_program_off, new_load_program_size, &new_load_program_addr)){
                    //修正各个字段
                    new_dynamic_addr = new_load_program_addr;
                    new_program_header_table_addr = new_dynamic_addr + new_dynamic_size;
                    new_dynstr_addr = new_program_header_table_addr + new_program_header_table_size;
                    if(0 == revise_new_elf_file(
                        fd, 
                        new_file_buffer, 
                        new_load_program_off,
                        new_load_program_size,
                        new_load_program_addr,
                        new_program_header_table_off, 
                        new_program_header_table_size, 
                        new_program_header_table_addr, 
                        new_dynamic_off,
                        new_dynamic_size,
                        new_dynamic_addr,
                        new_dynstr_off,
                        new_dynstr_size,
                        new_dynstr_addr)){
                            fseek(fd, 0, SEEK_SET);
                            if(1 == fwrite(new_file_buffer, new_file_size, 1, fd)){
                                ret = 0;
                            }
                        }
    
                }
            }
            free(new_file_buffer);
        }
    }
    fclose(fd);
    return ret;
}