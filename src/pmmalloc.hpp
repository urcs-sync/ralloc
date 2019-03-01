#ifndef _PMMALLOC_HPP_
#define _PMMALLOC_HPP_

#include <string>
#include <atomic>
#include <vector>

#include "pm_config.hpp"

#include "RegionManager.hpp"
#include "BaseMeta.hpp"

using namespace std;

class pmmalloc{
public:
	pmmalloc(string id, uint64_t thd_num = MAX_THREADS); // start/restart the heap by the application id.
	~pmmalloc(); // destructor to close the heap
	bool collect();
	void* p_malloc(size_t sz, vector<void*>(*f) = nullptr);
	void p_free(void* ptr);
	void* set_root(void* ptr, uint64_t i);//return the old i-th root
	void* get_root(uint64_t i);


private:
	string filepath;
	uint64_t thread_num;
	/* manager to map, remap, and unmap the heap */
	RegionManager* mgr;//initialized when pmmalloc constructs
	/* persistent metadata and their layout */
	BaseMeta* base_md;

	//GC
};

#endif /* _PMMALLOC_HPP_ */