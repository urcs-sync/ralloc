/*
 * Copyright (C) 2019 University of Rochester. All rights reserved.
 * Licenced under the MIT licence. See LICENSE file in the project root for
 * details. 
 */

/*
 * (c) Copyright 2016 Hewlett Packard Enterprise Development LP
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version. This program is
 * distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details. You should have received a copy of the GNU Lesser
 * General Public License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "RegionManager.hpp"

#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/select.h>

#include <iostream>
// //mmap anynomous
// void RegionManager::__map_transient_region(){
// 	char* ret = (char*) mmap((void*) 0, FILESIZE,
// 					PROT_READ | PROT_WRITE,
// 					MAP_PRIVATE | MAP_ANONYMOUS,
// 					-1, 0);
// 	if (ret == MAP_FAILED){
// 		printf ("Mmap failed");
// 		exit(1);
// 	}
// 	base_addr = ret;
// 	curr_addr = (char*) ((size_t) ret + 3 * sizeof(intptr_t));
// 	printf("Addr: %p\n", ret);
// 	printf("Base_addr: %p\n", base_addr);
// 	printf("Current_addr: %p\n", curr_addr);
// }

//mmap file
void RegionManager::__map_persistent_region(){
    DBG_PRINT("Creating a new persistent region...\n");
    int fd;
    fd  = open(HEAPFILE.c_str(), O_RDWR | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR);

    FD = fd;
    off_t offt = lseek(fd, FILESIZE-1, SEEK_SET);
    assert(offt != -1);

    int result = write(fd, "", 1);
    assert(result != -1);

    void * addr =
        mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MMAP_FLAG, fd, 0);
    assert(addr != MAP_FAILED);

    base_addr = (char*) addr;
    // | curr_addr  |
    // | heap_start |
    // |     size   |
    new (((atomic_pptr<char>*) base_addr)) atomic_pptr<char>((char*) ((size_t)addr + PAGESIZE));
    curr_addr_ptr = (atomic_pptr<char>*)base_addr;
    *(uint64_t*)((size_t)base_addr + 2*sizeof(atomic_pptr<char>)) = FILESIZE;

    FLUSH(curr_addr_ptr);
    FLUSH((uint64_t*)((size_t)base_addr + 2*sizeof(atomic_pptr<char>)));
    FLUSHFENCE;
    DBG_PRINT("Addr: %p\n", addr);
    DBG_PRINT("Base_addr: %p\n", base_addr);
    DBG_PRINT("Current_addr: %p\n", curr_addr_ptr->load());
}
void RegionManager::__remap_persistent_region(){
    DBG_PRINT("Remapping the persistent region...\n");
    int fd;
    fd = open(HEAPFILE.c_str(), O_RDWR,
                S_IRUSR | S_IWUSR);

    FD = fd;
    off_t offt = lseek(fd, FILESIZE-1, SEEK_SET);
    assert(offt != -1);

    int result = write(fd, "", 1);
    assert(result != -1);

    offt = lseek(fd, 0, SEEK_SET);
    assert (offt == 0);

    void * addr =
        mmap(0, FILESIZE, PROT_READ | PROT_WRITE, MMAP_FLAG, fd, 0);
    assert(addr != MAP_FAILED);

    base_addr = (char*) addr;
    curr_addr_ptr = (atomic_pptr<char>*)base_addr;
    assert(*(uint64_t*)((size_t)base_addr + 2*sizeof(atomic_pptr<char>)) == FILESIZE);
    DBG_PRINT("Addr: %p\n", addr);
    DBG_PRINT("Base_addr: %p\n", base_addr);
    DBG_PRINT("Curr_addr: %p\n", curr_addr_ptr->load());
}

void RegionManager::__map_transient_region(){
    DBG_PRINT("Creating a new transient region...\n");
    int fd;
    fd  = open(HEAPFILE.c_str(), O_RDWR | O_CREAT | O_TRUNC,
                S_IRUSR | S_IWUSR);

    FD = fd;
    off_t offt = lseek(fd, FILESIZE-1, SEEK_SET);
    assert(offt != -1);

    int result = write(fd, "", 1);
    assert(result != -1);

    void * addr =
        mmap(0, FILESIZE, PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_NORESERVE, fd, 0);
    assert(addr != MAP_FAILED);

    base_addr = (char*) addr;
    // | curr_addr  |
    // | heap_start |
    // |     size   |
    new (((atomic_pptr<char>*) base_addr)) atomic_pptr<char>((char*) ((size_t)addr + PAGESIZE));
    curr_addr_ptr = (atomic_pptr<char>*)base_addr;
    *(uint64_t*)((size_t)base_addr + 2*sizeof(atomic_pptr<char>)) = FILESIZE;

    FLUSH(curr_addr_ptr);
    FLUSH((uint64_t*)((size_t)base_addr + 2*sizeof(atomic_pptr<char>)));
    FLUSHFENCE;
    DBG_PRINT("Addr: %p\n", addr);
    DBG_PRINT("Base_addr: %p\n", base_addr);
    DBG_PRINT("Current_addr: %p\n", curr_addr_ptr->load());
}
void RegionManager::__remap_transient_region(){
    DBG_PRINT("Remapping the transient region...\n");
    int fd;
    fd = open(HEAPFILE.c_str(), O_RDWR,
                S_IRUSR | S_IWUSR);

    FD = fd;
    off_t offt = lseek(fd, FILESIZE-1, SEEK_SET);
    assert(offt != -1);

    int result = write(fd, "", 1);
    assert(result != -1);

    offt = lseek(fd, 0, SEEK_SET);
    assert (offt == 0);

    void * addr =
        mmap(0, FILESIZE, PROT_READ | PROT_WRITE, 
            MAP_SHARED | MAP_NORESERVE, fd, 0);
    assert(addr != MAP_FAILED);

    base_addr = (char*) addr;
    curr_addr_ptr = (atomic_pptr<char>*)base_addr;
    assert(*(uint64_t*)((size_t)base_addr + 2*sizeof(atomic_pptr<char>)) == FILESIZE);
    DBG_PRINT("Addr: %p\n", addr);
    DBG_PRINT("Base_addr: %p\n", base_addr);
    DBG_PRINT("Curr_addr: %p\n", curr_addr_ptr->load());
}

//persist the curr and base address
void RegionManager::__close_persistent_region(){
    FLUSHFENCE;
    FLUSH(curr_addr_ptr); 
    FLUSHFENCE;
    DBG_PRINT("At the end current addr: %p\n", curr_addr_ptr->load());

    unsigned long space_used = ((unsigned long) curr_addr_ptr->load() 
         - (unsigned long) base_addr);
    unsigned long remaining_space = 
         ((unsigned long) FILESIZE - space_used) / (1024 * 1024);
    DBG_PRINT("Space Used(rounded down to MiB): %ld, Remaining(MiB): %ld\n", 
            space_used / (1024 * 1024), remaining_space);
    munmap((void*)base_addr, FILESIZE);
    close(FD);
}

//flush transient region back
void RegionManager::__close_transient_region(){
    FLUSHFENCE;
    char* curr_addr = curr_addr_ptr->load();
    FLUSH(curr_addr_ptr);
    FLUSHFENCE;

    DBG_PRINT("At the end current addr: %p\n", curr_addr);

    unsigned long space_used = ((unsigned long) curr_addr 
         - (unsigned long) base_addr);
    unsigned long remaining_space = 
         ((unsigned long) FILESIZE - space_used) / (1024 * 1024);
    DBG_PRINT("Space Used(rounded down to MiB): %ld, Remaining(MiB): %ld\n", 
            space_used / (1024 * 1024), remaining_space);
    munmap((void*)base_addr, FILESIZE);
    close(FD);
}

//store heap root by offset from base
void RegionManager::__store_heap_start(void* root){
    *(((intptr_t*) base_addr) + 1) = (intptr_t) root - (intptr_t) base_addr;
    FLUSH( (((intptr_t*) base_addr) + 1)); 
    FLUSHFENCE;
}

//retrieve heap root
void* RegionManager::__fetch_heap_start(){
    return (void*) (*(((intptr_t*) base_addr) + 1) + (intptr_t) base_addr);
}

int RegionManager::__nvm_region_allocator(void** memptr, size_t alignment, size_t size){
    char* next;
    char* res;
    if (size <= 0) return -1;

    if (((alignment & (~alignment + 1)) != alignment) ||	//should be multiple of 2
        (alignment < sizeof(void*))) return -1; //should be at least the size of void*
    char * old_curr_addr = curr_addr_ptr->load();
    
    char * new_curr_addr = old_curr_addr;
    size_t aln_adj = (size_t) new_curr_addr & (alignment - 1);

    if (aln_adj != 0)
        new_curr_addr += (alignment - aln_adj);

    res = new_curr_addr;
    next = new_curr_addr + size;
    if (next > base_addr + FILESIZE){
        printf("\n----Region Manager: out of space in mmaped file-----\nCurr:%p\nBase:%p\n",res,base_addr);
        return -1;
    }
    new_curr_addr = next;
    FLUSH(curr_addr_ptr);
    FLUSHFENCE;
    if(curr_addr_ptr->compare_exchange_strong(old_curr_addr, new_curr_addr)){
        FLUSH(curr_addr_ptr);
        FLUSHFENCE;
        *memptr = res;
        return 1;
    }
    return 0;
    // *(((intptr_t*) base_addr) + 1) = (intptr_t) curr_addr;
    // FLUSH( (((intptr_t*) base_addr) + 1)); 
}

int RegionManager::__try_nvm_region_allocator(void** memptr, size_t alignment, size_t size){
    char* next;
    char* res;
    if (size <= 0) return -1;

    if (((alignment & (~alignment + 1)) != alignment) || //should be multiple of 2
        (alignment < sizeof(void*))) return -1; //should be at least the size of void*
    char * old_curr_addr = curr_addr_ptr->load();
    char * new_curr_addr = old_curr_addr;
    size_t aln_adj = (size_t) new_curr_addr & (alignment - 1);

    if (aln_adj != 0)
        new_curr_addr += (alignment - aln_adj);

    res = new_curr_addr;
    next = new_curr_addr + size;
    if (next > base_addr + FILESIZE){
        printf("\n----Region Manager: out of space in mmaped file-----\n");
        return -1;
    }
    new_curr_addr = next;
    FLUSH(curr_addr_ptr);
    FLUSHFENCE;
    if(curr_addr_ptr->compare_exchange_strong(old_curr_addr, new_curr_addr)) {
        FLUSH(curr_addr_ptr);
        FLUSHFENCE;
        *memptr = res;
        return 0;
    } else {
        return 1;
    }
}

bool RegionManager::__within_range(void* ptr){
    intptr_t curr_addr = (intptr_t)curr_addr_ptr->load();
    return ((intptr_t)base_addr<(intptr_t)ptr) && ((intptr_t)ptr<curr_addr);
}

void RegionManager::__destroy(){
    if(!exists_test(HEAPFILE)){
        std::cout<<"File "<<HEAPFILE<<" doesn't exist!\n";
        return;
    }
    remove(HEAPFILE.c_str());
    return;
}
