#ifndef _PMMALLOC_HPP_
#define _PMMALLOC_HPP_

#include <string>
#include <atomic>
#include <vector>

#include "pm_config.hpp"

#include "RegionManager.hpp"
#include "BaseMeta.hpp"

using namespace std;
/*
 * TODO: arrange the heap layout (start addr of each structs)
 * Maybe a table would be helpful. Learn how Makalu does this
 */
class pmmalloc{
public:
	pmmalloc(string id, uint64_t thd_num = MAX_THREADS); // start/restart the heap by the application id.
	~pmmalloc(); // destructor to close the heap
	bool collect();
	void* malloc(size_t sz, vector<void*>(*f) = nullptr);
	void free(void* ptr);
	void* set_root(void* ptr, uint64_t i);//return the old i-th root
	void* get_root(uint64_t i);


private:
	void restart();

	string filepath;
	/* manager to map, remap, and unmap the heap */
	RegionManager* mgr;//initialized when pmmalloc constructs
	/* persistent metadata and their layout */
	BaseMeta* base_md;

	/* transient metadata and tools */
	MichaelScottQueue<Descriptor*> free_desc;
	//GC

};

#endif /* _PMMALLOC_HPP_ */