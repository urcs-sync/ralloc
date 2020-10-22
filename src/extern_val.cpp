// This file defines extern variables declared in AllocatorMacro.hpp
// for other allocators such as PMDK and JEMalloc
#ifdef RALLOC
    // No extern var
#elif defined(MAKALU) 
    char *base_addr = nullptr;
    char *curr_addr = nullptr;
#elif defined(PMDK)
    #include <libpmemobj.h>
    PMEMobjpool* pop = nullptr;
    PMEMoid root;
#else
    void* roots[1024];
#endif