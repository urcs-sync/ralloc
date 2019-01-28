#include <cstdio>
#include <iostream>

#include "RegionManager.hpp"

int main(){
	if(RegionManager::exists_test("/dev/shm/test")){
		cout<<"/dev/shm/test exist, testing remap...";
		RegionManager* mgr = new RegionManager("/dev/shm/test");
		delete mgr;
		cout<<"done!\n";
	} else {
		cout<<"create new RegionManager in /dev/shm/test...";
		RegionManager* mgr = new RegionManager("/dev/shm/test");
		delete mgr;
		cout<<"done!\n";
	}
	return 0;
}