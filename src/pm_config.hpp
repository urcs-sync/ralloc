#ifndef _PM_CONFIG_HPP_
#define _PM_CONFIG_HPP_


#include <assert.h>

#include "pfence_util.h"

/* user customized macros */
#define HEAPFILE_PREFIX "/dev/shm/"
#define ENABLE_FILTER_FUNC 1
#define DEBUG 1
const uint64_t MAX_FILESIZE = 5*1024*1024*1024ULL + 24;//TODO:is 24 a good number?
const uint64_t MAX_THREADS = 512;
const int MAX_ROOTS = 1024;
const int MAX_SECTION = 10;
const int DESC_SPACE_CAP = 1024;//number of desc sbs per desc space
const uint64_t SB_SPACE_SIZE = 4*1024*1024*1024ULL;
//Note: by this config, approximately 1 sb space needs 1 desc space

/* constant variables */
const int TYPE_SIZE = 4;
const int PTR_SIZE = sizeof(void*);
const int HEADER_SIZE = (TYPE_SIZE + PTR_SIZE);
const int CACHE_LINE_SIZE = 64;

const int LARGE = 249; // tag indicating the block is large
const int SMALL = 250; // tag indicating the block is small

const int PAGESIZE = 4096;
const int SBSIZE = (16 * PAGESIZE); // size of a superblock
const int DESCSBSIZE = (1024 * 64);//assume sizeof(Descriptor) is 64

const int ACTIVE = 0; // 4 status of a superblock
const int FULL = 1;
const int PARTIAL = 2;
const int EMPTY = 3;

const int MAXCREDITS = 64; // 2^(bits for credits in active)
const int GRANULARITY = 8; // granularity of sizeclass size
const int MAX_SMALLSIZE = 2048; // largest size of a small object
const int PROCHEAP_NUM = MAX_THREADS; // number of processor heap

const int DESC_SPACE_SIZE = DESC_SPACE_CAP * DESCSBSIZE;

#endif