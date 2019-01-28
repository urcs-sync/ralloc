#include <cstdio>
#include <iostream>

#include "RegionManager.hpp"
#include "BaseMeta.hpp"


int main(){
	if(RegionManager::exists_test("/dev/shm/test")){
		cout<<"/dev/shm/test exist, testing remap...";
		RegionManager* mgr = new RegionManager("/dev/shm/test");
		void* hstart = mgr->__fetch_heap_start();
		BaseMeta* base_md = (BaseMeta*) hstart;
		base_md->set_mgr(mgr);
		assert(base_md->get_root(0)==(void*)0xff00);
		assert(base_md->get_root(1)==(void*)0xfff1);
		delete mgr;
		cout<<"done!\n";
	} else {
		cout<<"create new RegionManager in /dev/shm/test...";
		RegionManager* mgr = new RegionManager("/dev/shm/test");
		BaseMeta* base_md;
		int res = mgr->__nvm_region_allocator((void**)&base_md,sizeof(void*),sizeof(BaseMeta));
		if(res!=0) assert(0&&"mgr allocation fails!");
		mgr->__store_heap_start(base_md);
		new (base_md) BaseMeta(mgr, 1);
		base_md->set_root((void*)0xff00,0);
		base_md->set_root((void*)0xfff1,1);
		delete mgr;
		cout<<"done!\n";
	}
	return 0;
}