/*

Copyright 2019 University of Rochester

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

/*

MIT License

Copyright (c) 2018 Ricardo Leite

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "TCache.hpp"

using namespace ralloc;
thread_local TCaches ralloc::t_caches;

void TCacheBin::push_block(char* block)
{
	// block has at least sizeof(char*)
	*(pptr<char>*)block = _block;
	_block = block;
	_block_num++;
}

void TCacheBin::push_list(char* block, uint32_t length)
{
	// caller must ensure there's no available block
	// this op is only used to fill empty cache
	assert(_block_num == 0);

	_block = block;
	_block_num = length;
}

char* TCacheBin::pop_block()
{
	// caller must ensure there's an available block
	assert(_block_num > 0);

	char* ret = _block;
	char* next = (char*)(*(pptr<char>*)ret);
	_block = next;
	_block_num--;
	return ret;
}

void TCacheBin::pop_list(char* block, uint32_t length)
{
	assert(_block_num >= length);

	_block = block;
	_block_num -= length;
}
