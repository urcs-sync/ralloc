#include <cstdio>
#include <iostream>

#include "RegionManager.hpp"
#include "BaseMeta.hpp"


int main(){
	RegionManager* mgr;
	BaseMeta* base_md;
	if(RegionManager::exists_test("/dev/shm/test")){
		cout<<"/dev/shm/test exist, testing remap...";
		mgr = new RegionManager("/dev/shm/test");
		void* hstart = mgr->__fetch_heap_start();
		base_md = (BaseMeta*) hstart;
		base_md->set_mgr(mgr);
		assert(base_md->get_root(0)==(void*)0xff00);
		assert(base_md->get_root(1)==(void*)0xfff1);
	} else {
		cout<<"create new RegionManager in /dev/shm/test...";
		mgr = new RegionManager("/dev/shm/test");
		int res = mgr->__nvm_region_allocator((void**)&base_md,sizeof(void*),sizeof(BaseMeta));
		if(res!=0) assert(0&&"mgr allocation fails!");
		mgr->__store_heap_start(base_md);
		new (base_md) BaseMeta(mgr, 1);
		base_md->set_root((void*)0xff00,0);
		base_md->set_root((void*)0xfff1,1);
		base_md->new_space(0);//desc
		base_md->new_space(1);//small sb
		base_md->new_space(2);//large sb
	}
	delete mgr;
	cout<<"done!\n";
	return 0;
}