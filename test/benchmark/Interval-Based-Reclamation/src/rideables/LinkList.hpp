/*

Copyright 2017 University of Rochester

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. 

*/


#ifndef LINK_LIST
#define LINK_LIST

#include "SortedUnorderedMap.hpp"
#include "trackers/AllocatorMacro.hpp"

#ifdef NGC
#define COLLECT false
#else
#define COLLECT true
#endif

template <class K, class V>
class LinkListFactory : public RideableFactory{
	SortedUnorderedMap<K,V,1>* build(GlobalTestConfig* gtc){
		if(gtc->restart) {
			auto ret = reinterpret_cast<SortedUnorderedMap<K,V,1>*>(get_root(2));
			ret->restart(gtc);
			return ret;
		} else {
			auto ret = new SortedUnorderedMap<K,V,1>(gtc);
			set_root(ret, 2);
			return ret;
		}
	}
};

#endif
