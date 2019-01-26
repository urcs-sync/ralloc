#include <sys/time.h>
#include <execinfo.h> //backtrace()
#include <unistd.h> // alarm
#include <csignal> //signal
#include <cassert>
#include <iostream>
#include <thread>
#include <random>
#include <set>
#include <queue>
#include <vector>

#include "MichaelScottQueue.hpp"

using namespace std;

const int THREAD_NUM = 4;
const int TEST_SEC = 5;

MichaelScottQueue<unsigned int> msq(THREAD_NUM);
multiset<unsigned int> in[4];
queue<unsigned int> out[4];
struct timeval time_up;

int test(int tid){
	std::mt19937 gen_p(tid);
	std::mt19937 gen_k(tid+THREAD_NUM);
	struct timeval now;
	gettimeofday(&now,NULL);
	int ops = 0;
	// unsigned int cnt = 0;
	cout << "thread " << tid << " starts\n";
	while(now.tv_sec < time_up.tv_sec 
			|| (now.tv_sec==time_up.tv_sec && now.tv_usec<time_up.tv_usec) ){
		if(gen_p()%2==0) { // enqueue
			unsigned int k = gen_k()%1000000000;
			in[tid].insert(k);
			k = (k<<2) | (tid&0b11);
			msq.enqueue(k, tid);
		} else { // dequeue
			optional<unsigned int> res = msq.dequeue(tid);
			if(res) out[tid].push(res.value());
		}
		ops++;
		gettimeofday(&now,NULL);
	}
	cout << "thread " << tid << " finishes\n";
	return ops;
}

int check(){
	cout << "start checking\n";
	cout << "removing dequeued elements from in vector...";
	for(int i=0;i<THREAD_NUM;i++){
		while(!out[i].empty()){
			auto x = out[i].front();
			out[i].pop();
			unsigned int tid = x & 0b11;
			x = x >> 2;
			auto iter = in[tid].find(x);
			assert(iter!=in[tid].end() && "Fail to find the dequeued element!");
			in[tid].erase(iter);
		}
	}
	cout<<"passed!\n";
	cout<<"dequeuing from msq...";
	while(auto _x = msq.dequeue(0)){
		unsigned int x = _x.value();
		unsigned int tid = x & 0b11;
		x = x >> 2;
		auto iter = in[tid].find(x);
		assert(iter!=in[tid].end() && "Fail to find the dequeuing element!");
		in[tid].erase(iter);
	}
	cout<<"now in vector should be empty...";
	for(int i=0;i<THREAD_NUM;i++){
		assert(in[i].empty() && "set isn't empty!");
	}
	cout<<"passed!\n";
	return 0;
}
void faultHandler(int sig) {
	void *buf[30];
	size_t sz;

	// scrape backtrace
	sz = backtrace(buf, 30);

	// print error msg
	fprintf(stderr, "Error: signal %d:\n", sig);
	backtrace_symbols_fd(buf, sz, STDERR_FILENO);
	exit(1);
}

bool testComplete;
void alarmhandler(int sig){
	if(testComplete==false){
		fprintf(stderr,"Time out error.\n");
		faultHandler(sig);
	}
}
int main(){
	gettimeofday (&time_up, NULL);
	time_up.tv_sec += TEST_SEC;
	vector<thread> workers;
	signal(SIGALRM, &alarmhandler);
	alarm(TEST_SEC+10);
	testComplete = false;
	for(int i=0;i<THREAD_NUM;i++)
		workers.emplace_back(test,i);
	for(auto& th:workers)
		th.join();
	testComplete = true;
	check();
	cout<<"test finishes!\n";
	return 0;
}