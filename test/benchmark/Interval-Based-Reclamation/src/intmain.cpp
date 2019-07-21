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



#include <stdio.h>
#include <stdlib.h>
#include <iostream>

#include "Harness.hpp"
#include "CustomTests.hpp"
#include "rideables/SGLUnorderedMap.hpp"
#include "rideables/SortedUnorderedMap.hpp"
#include "rideables/NatarajanTree.hpp"
#include "rideables/LinkList.hpp"
#include "trackers/AllocatorMacro.hpp"

using namespace std;

GlobalTestConfig* gtc;

// the main function
// sets up output and tests
int main(int argc, char *argv[])
{
	int restart = PM_start("ibrint");
	gtc = new GlobalTestConfig((bool)restart);

	// additional rideables go here
	gtc->addRideableOption(new SGLUnorderedMapFactory<int,int>(), "SGLUnorderedMap (default)");
	gtc->addRideableOption(new SortedUnorderedMapFactory<int,int>(), "SortedUnorderedMap");
	gtc->addRideableOption(new LinkListFactory<int,int>(), "LinkList");
	gtc->addRideableOption(new NatarajanTreeFactory<int,int>(), "NatarajanTree");

	gtc->addTestOption(new ObjRetireTest<int>(50,0,50,0,0,10000000,10000000), "300 MB prefill:range=10000000");
	gtc->addTestOption(new ObjRetireTest<int>(50,0,50,0,0,20000000,20000000), "600 MB prefill:range=20000000");
	gtc->addTestOption(new ObjRetireTest<int>(50,0,50,0,0,30000000,30000000), "900 MB prefill:range=30000000");
	gtc->addTestOption(new ObjRetireTest<int>(50,0,50,0,0,40000000,40000000), "1200 MB prefill:range=40000000");
	gtc->addTestOption(new ObjRetireTest<int>(50,0,50,0,0,80000000,80000000), "2400 MB prefill:range=80000000");
	gtc->addTestOption(new ObjRetireTest<int>(50,0,50,0,0,100000000,100000000), "3000 MB prefill:range=100000000");

	// gtc->addTestOption(new MapOrderedGet<int>(65536, 5000), "MapOrderedGetPut:range=65536:prefill=5000");
	// gtc->addTestOption(new MapChurnTest<int>(50,0,0,50,0,8000,1024), "MapChurn:g50i50:range=8K:prefill=1024");
	// gtc->addTestOption(new MapChurnTest<int>(50,0,0,50,0,128000,1024), "MapChurn:g50i50:range=128K:prefill=1024");
	// gtc->addTestOption(new MapChurnTest<int>(50,0,0,50,0,2000000,1024), "MapChurn:g50i50:range=2M:prefill=1024");
	// gtc->addTestOption(new QueryVerifyTest<int>, "QueryVerifyTest");
	// gtc->addTestOption(new DebugTest, "DebugTest");
	// gtc->addTestOption(new TopologyReport(), "TopologyReport");

	// parse command line
	gtc->parseCommandLine(argc,argv);

	if(gtc->verbose){
		fprintf(stdout, "Testing:  %d threads for %lu seconds with %s\nTest size = %s ",
		  gtc->task_num,gtc->interval,gtc->getRideableName().c_str(),gtc->getTestName().c_str());
	}

	// register fancy seg fault handler to get some
	// info in case of crash
	signal(SIGSEGV, faultHandler);

	// do the work....
	gtc->runTest();


	// print out results
	long int ops = gtc->interval==0 ? 0 : gtc->total_operations/gtc->interval;
	if(gtc->verbose){
		printf("Operations/sec: %ld\n",ops);
	}
	else{
		printf("%ld \t",ops);
	}
	// PM_close();
	return 0;
}

