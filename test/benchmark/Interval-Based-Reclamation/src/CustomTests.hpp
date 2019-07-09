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


#ifndef CUSTOMTESTS_HPP
#define CUSTOMTESTS_HPP

#include "Harness.hpp"
#include "RUnorderedMap.hpp"
#include "ROrderedMap.hpp"
#include "RetiredMonitorable.hpp"
#include <map>
#include <random>
template <class T>
class MapChurnTest : public Test{
public:
	RUnorderedMap<T,T>* m;
	int pg,pr,pp,pi,pv;
	int prop_gets, prop_replaces, prop_puts, prop_inserts, prop_removes;
	int range;
	int prefill;

	inline T fromInt(uint64_t v);
	
	MapChurnTest(int p_gets, int p_replaces, int p_puts, int p_inserts, int p_removes, int range, int prefill);
	MapChurnTest(int p_gets, int p_replaces, int p_puts, int p_inserts, int p_removes, int range):
		MapChurnTest(p_gets, p_replaces, p_puts, p_inserts, p_removes, range,0){}
	void init(GlobalTestConfig* gtc);
	void parInit(GlobalTestConfig* gtc, LocalTestConfig* ltc){}
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};

template <class T>
MapChurnTest<T>::MapChurnTest(int p_gets, int p_replaces, int p_puts, 
 int p_inserts, int p_removes, int range, int prefill){
	pg = p_gets;
	pr = p_replaces;
	pp = p_puts;
	pi = p_inserts;
	pv = p_removes;

	int sum = p_gets;
	prop_gets = sum;
	sum+=p_replaces;
	prop_replaces = sum;
	sum+=p_puts;
	prop_puts = sum;
	sum+=p_inserts;
	prop_inserts = sum;
	sum+=p_removes;
	prop_removes = sum;
	
	this->range = range;
	this->prefill = prefill;
}


template <class T>
void MapChurnTest<T>::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->m = dynamic_cast<RUnorderedMap<T,T>*>(ptr);
	if (!m) {
		 errexit("MapChurnTest must be run on RUnorderedMap<T,T> type object.");
	}
	
	if(gtc->verbose){
		printf("Gets:%d Replaces:%d Puts:%d Inserts:%d Removes: %d\n",
		 pg,pr,pp,pi,pv);
	}
	
	// overrides for constructor arguments
	if(gtc->checkEnv("range")){
		range = atoi((gtc->getEnv("range")).c_str());
	}
	if(gtc->checkEnv("prefill")){
		prefill = atoi((gtc->getEnv("prefill")).c_str());
	}

	// prefill
	int i = 0;
	uint64_t r = 1;
	std::mt19937_64 gen(1);
	for(i = 0; i<prefill; i++){
		// r = nextRand(r);
		r = gen();
		T k = this->fromInt(r%range);
		T val = k;
		m->put(k,val,0);
}
	if(gtc->verbose){
		printf("Prefilled %d\n",i);
	}
}

template <class T>
inline T MapChurnTest<T>::fromInt(uint64_t v){
	return (T)v;
}

template<>
inline std::string MapChurnTest<std::string>::fromInt(uint64_t v){
	return std::to_string(v);
}

template <class T>
int MapChurnTest<T>::MapChurnTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	uint64_t r = ltc->seed;
	std::mt19937_64 gen_k(r);
	std::mt19937_64 gen_p(r+1);
	int tid = ltc->tid;

	//broker->threadInit(gtc,ltc);

	while(timeDiff(&now,&time_up)>0){
		// r = nextRand(r);
		r = gen_k();
		T k = this->fromInt(r%range);
		T val = k;

		int p = gen_p()%100;

		if(p<prop_gets){
			m->get(k,tid);
		}
		else if(p<prop_replaces){
			auto old = m->replace(k,val,tid);
		}
		else if(p<prop_puts){
			auto old = m->put(k,val,tid);
		}
		else if(p<prop_inserts){
			m->insert(k,val,tid);
		}
		else{ // p<=prop_removes
			m->remove(k,tid);
		}

		ops++;
		gettimeofday(&now,NULL);
	}
	return ops;
}

template <class T>
class ObjRetireTest : public Test{
public:
	RUnorderedMap<T,T>* m;
	int pg,pr,pp,pi,pv;
	int prop_gets, prop_replaces, prop_puts, prop_inserts, prop_removes;
	int range;
	int prefill;

	inline T fromInt(uint64_t v);
	
	ObjRetireTest(int p_gets, int p_replaces, int p_puts, int p_inserts, int p_removes, int range, int prefill);
	ObjRetireTest(int p_gets, int p_replaces, int p_puts, int p_inserts, int p_removes, int range):
		ObjRetireTest(p_gets, p_replaces, p_puts, p_inserts, p_removes, range,0){}
	void init(GlobalTestConfig* gtc);
	void parInit(GlobalTestConfig* gtc, LocalTestConfig* ltc){}
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
};

template <class T>
ObjRetireTest<T>::ObjRetireTest(int p_gets, int p_replaces, int p_puts, 
 int p_inserts, int p_removes, int range, int prefill){
	pg = p_gets;
	pr = p_replaces;
	pp = p_puts;
	pi = p_inserts;
	pv = p_removes;

	int sum = p_gets;
	prop_gets = sum;
	sum+=p_replaces;
	prop_replaces = sum;
	sum+=p_puts;
	prop_puts = sum;
	sum+=p_inserts;
	prop_inserts = sum;
	sum+=p_removes;
	prop_removes = sum;
	
	this->range = range;
	this->prefill = prefill;
}


template <class T>
void ObjRetireTest<T>::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	if (!dynamic_cast<RetiredMonitorable*>(ptr)){
		errexit("ObjRetireTest must be run on RetiredMonitorable type object.");
	}
	this->m = dynamic_cast<RUnorderedMap<T,T>*>(ptr);
	if (!m) {
		 errexit("ObjRetireTest must be run on RUnorderedMap<T,T> type object.");
	}
	
	if(gtc->verbose){
		printf("Gets:%d Replaces:%d Puts:%d Inserts:%d Removes: %d\n",
		 pg,pr,pp,pi,pv);
	}
	
	// overrides for constructor arguments
	if(gtc->checkEnv("range")){
		range = atoi((gtc->getEnv("range")).c_str());
	}
	if(gtc->checkEnv("prefill")){
		prefill = atoi((gtc->getEnv("prefill")).c_str());
	}

	// add a field in records:
	gtc->recorder->addThreadField("obj_retired", &Recorder::sumInt64s);

	// prefill
	int i = 0;
	// std::mt19937_64 gen(1);
	// for testing GC we disable RNG
	for(i = 0; i<prefill; i++){
		T k = this->fromInt(i%range);
		T val = k;
		m->put(k,val,0);
}
	if(gtc->verbose){
		printf("Prefilled %d\n",i);
	}
}

template <class T>
inline T ObjRetireTest<T>::fromInt(uint64_t v){
	return (T)v;
}

template<>
inline std::string ObjRetireTest<std::string>::fromInt(uint64_t v){
	return std::to_string(v);
}

template <class T>
int ObjRetireTest<T>::ObjRetireTest::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	uint64_t r = ltc->seed;
	std::mt19937_64 gen_k(r);
	std::mt19937_64 gen_p(r+1);
	int tid = ltc->tid;

	//broker->threadInit(gtc,ltc);

	while(timeDiff(&now,&time_up)>0){
		// r = nextRand(r);
		r = gen_k();
		T k = this->fromInt(r%range);
		T val = k;

		int p = gen_p()%100;

		if(p<prop_gets){
			// printf("g: %lu\n", k);
			m->get(k,tid);
		}
		else if(p<prop_replaces){
			// printf("r: %lu\n", k);
			auto old = m->replace(k,val,tid);
		}
		else if(p<prop_puts){
			// printf("p: %lu\n", k);
			auto old = m->put(k,val,tid);
		}
		else if(p<prop_inserts){
			// std::cout<<"i: "<<k<<std::endl;
			// printf("i: %lu\n", k);
			m->insert(k,val,tid);
		}
		else{ // p<=prop_removes
			// std::cout<<"r: "<<k<<std::endl;
			// printf("r: %lu\n", k);
			m->remove(k,tid);
		}

		ops++;
		gettimeofday(&now,NULL);
	}

	RetiredMonitorable* rm_ptr = dynamic_cast<RetiredMonitorable*>(m);
	gtc->recorder->reportThreadInfo("obj_retired", rm_ptr->report_retired(ltc->tid), ltc->tid);
	return ops;
}


// by Hs: test framework used for debugging, modifiy it as needed.
class DebugTest : public Test{
public:
	RUnorderedMap<std::string, std::string>* m;
	void init(GlobalTestConfig* gtc);
	void parInit(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc){}
	void put_get(std::string key, std::string value, int tid);
	void remove_get(std::string key, int tid);
	void get(std::string key, int tid);
};


template <class T>
class MapVerifyTest : public Test{
public:
	RUnorderedMap<T,T>* q;
	std::atomic<bool> passed;
	UIDGenerator* ug;
	
	T fromInt(uint64_t v);
	int toInt(T v);

	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};

template <class T>
void MapVerifyTest<T>::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<RUnorderedMap<T,T>*>(ptr);
	if (!q) {
		 errexit("MapVerificationTest must be run on RMap type object.");
	}
	ug = new UIDGenerator(gtc->task_num);
	passed.store(1);
}

template <class T>
inline T MapVerifyTest<T>::fromInt(uint64_t v){
	return (T)v;
}
template<>
inline std::string MapVerifyTest<std::string>::fromInt(uint64_t v){
	return std::to_string(v);
}
template <class T>
inline int MapVerifyTest<T>::toInt(T v){
	return (int)v;
}
template<>
inline int MapVerifyTest<std::string>::toInt(std::string v){
	return std::atoi(v.c_str());
}

template <class T>
int MapVerifyTest<T>::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	uint64_t r = ltc->seed;
	std::mt19937_64 gen_k(r);
	std::mt19937_64 gen_p(r+1);
	int tid = ltc->tid;

	std::vector<uint32_t> found;
	found.resize(gtc->task_num);
	for(int i = 0; i<gtc->task_num; i++){
		found[i]=0;
	}

	uint32_t insKey = ug->initial(tid);
	uint32_t remKey = ug->initial((tid+1)%(gtc->task_num));

	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		// r = nextRand(r);
		r = gen_k();
		if(gen_p()%2==0){
			q->put(this->fromInt(insKey),this->fromInt(insKey),tid);
			insKey = ug->next(insKey,tid);
		}
		else{
			auto removed = q->remove(this->fromInt(remKey),tid);
			if(!removed){continue;}
			uint32_t r = this->toInt(removed.value());
			assert(r==remKey);
			uint32_t id = ug->id(r);
			uint32_t cnt = ug->count(r);

			if(cnt<=found[id]){
				std::cout<<"Verification failed! Reordering violation."<<std::endl;
				std::cout<<"I'm thread "<<tid<<std::endl;
				std::cout<<"Found "<<found[id]<<" for thread "<<id<<std::endl;
				std::cout<<"Putting "<<cnt<<std::endl;
				passed.store(0);
				assert(0);
			}
			found[id]=cnt;
			remKey = ug->next(remKey,tid);
		}
		ops++;

		gettimeofday(&now,NULL);
	}
	return ops;
}

template <class T>
void MapVerifyTest<T>::cleanup(GlobalTestConfig* gtc){
	if(passed){
		std::cout<<"Verification passed!"<<std::endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
}


template <class T>
class QueryVerifyTest : public Test{
public:
	ROrderedMap<T,T>* q;
	std::atomic<bool> passed;
	UIDGenerator* ug;
	
	T fromInt(uint32_t v);
	uint32_t toInt(T v);

	void init(GlobalTestConfig* gtc);
	int execute(GlobalTestConfig* gtc, LocalTestConfig* ltc);
	void cleanup(GlobalTestConfig* gtc);
};

template <class T>
void QueryVerifyTest<T>::init(GlobalTestConfig* gtc){
	Rideable* ptr = gtc->allocRideable();
	this->q = dynamic_cast<ROrderedMap<T,T>*>(ptr);
	if (!q) {
		 errexit("RangeVerificationTest must be run on ROrderedMap type object.");
	}
	ug = new UIDGenerator(gtc->task_num);
	passed.store(1);
}

template <class T>
inline T QueryVerifyTest<T>::fromInt(uint32_t v){
	return (T)v;
}
template<>
inline std::string QueryVerifyTest<std::string>::fromInt(uint32_t v){
	return std::to_string(v);
}
template <class T>
inline uint32_t QueryVerifyTest<T>::toInt(T v){
	return (uint32_t)v;
}
template<>
inline uint32_t QueryVerifyTest<std::string>::toInt(std::string v){
	return std::atoi(v.c_str());
}

template <class T>
int QueryVerifyTest<T>::execute(GlobalTestConfig* gtc, LocalTestConfig* ltc){
	struct timeval time_up = gtc->finish;
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	uint64_t r = ltc->seed;
	std::mt19937_64 gen(r);
	int tid = ltc->tid;

	std::vector<uint32_t> found;
	found.resize(gtc->task_num);
	for(int i = 0; i<gtc->task_num; i++){
		found[i]=0;
	}
	int len=0;
	uint32_t insKey = ug->initial(tid);
	T leftKey = this->fromInt(0);
	T rightKey = this->fromInt(0);
	for(uint32_t i=1;i<uint32_t(gtc->task_num);i++){
		T curKey=this->fromInt(i);
		if(curKey<leftKey) leftKey=curKey;
		if(curKey>rightKey) rightKey=curKey;
	}
	while(now.tv_sec < time_up.tv_sec 
		|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		// r = nextRand(r);
		r = gen();
		if(r%2==0){
			q->put(this->fromInt(tid),this->fromInt(insKey),tid);
			insKey = ug->next(insKey,tid);
		}
		else{
			size_t nodeCnt=0;
			for(uint32_t i=0;i<uint32_t(gtc->task_num);i++){
				auto rt=q->get(this->fromInt(i),tid);
				if(rt){
					uint32_t cnt = ug->count(this->toInt(rt.value()));
					found[i]=cnt;
					nodeCnt++;
				}
			}
			if(nodeCnt==0) {continue;}
			auto range = q->rangeQuery(leftKey,rightKey,len,tid);
			if(nodeCnt>range.size()){
				std::cout<<"Verification failed! Returned result has unexpectedly less elements. "<<std::endl;
				std::cout<<"range.size():"<<range.size()<<" nodeCnt:"<<nodeCnt<<std::endl;
				std::cout<<"I'm thread "<<tid<<std::endl;
				passed.store(0);
				assert(0);
			}
			for(auto& e:range){
				uint32_t r = this->toInt(e.second);
				uint32_t id = this->toInt(e.first);
				uint32_t cnt = ug->count(r);
				if(cnt<found[id]){
					std::cout<<"Verification failed! Reordering violation."<<std::endl;
					std::cout<<"I'm thread "<<tid<<std::endl;
					std::cout<<"Found "<<found[id]<<" for thread "<<id<<std::endl;
					std::cout<<"Putting "<<cnt<<std::endl;
					passed.store(0);
					assert(0);
				}
				found[id]=cnt;
			}
		}
		ops++;
		gettimeofday(&now,NULL);
	}
	return ops;
}

template <class T>
void QueryVerifyTest<T>::cleanup(GlobalTestConfig* gtc){
	if(passed){
		std::cout<<"Verification passed!"<<std::endl;
		gtc->recorder->reportGlobalInfo("notes","verify pass");
	}
	else{
		gtc->recorder->reportGlobalInfo("notes","verify fail");
	}
}

#endif
