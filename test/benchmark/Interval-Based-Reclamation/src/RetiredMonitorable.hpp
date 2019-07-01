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


#ifndef RETIREDMONITORABLE_HPP
#define RETIREDMONITORABLE_HPP

#include <queue>
#include <list>
#include <vector>
#include <atomic>
#include "ConcurrentPrimitives.hpp"
#include "RAllocator.hpp"

class RetiredMonitorable{
public:
	padded<uint64_t>* retired_cnt;
	RetiredMonitorable(GlobalTestConfig* gtc){
		retired_cnt = new padded<uint64_t>[gtc->task_num];
	}
	void collect_retired_size(uint64_t size, int tid){
		retired_cnt[tid].ui += size;
	}
	uint64_t report_retired(int tid){
		return retired_cnt[tid].ui;
	}
};

#endif