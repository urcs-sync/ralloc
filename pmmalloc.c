/*
 * Copyright (C) 2007  Scott Schneider, Christos Antonopoulos
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "pmmalloc.h"
#include <stdatomic.h>
static int init_flag = 0;// a flag to indicate whether the library has been init.
/*
 * This implementation uses CAS-16B to avoid ABA-problem in Partial queue and free desc
 * linked list.
 * The heaps are per-threaded, but according to the header of each allocated block, we
 * are able to put them back to the list of their corresponding heap.
 */
#if (__x86_64__ || __ppc64__)
inline static int _WideCAS(volatile __uint128_t *obj, __uint128_t old_value, 
	__uint128_t new_value, memory_order morder) {
	int ret;
	uint64_t* old_v = (uint64_t*)&old_value;
	uint64_t* new_v = (uint64_t*)&new_value;
	__asm__ __volatile__(
	"lock cmpxchg16b %1;\n"
	"sete %0;\n"
	:"=m"(ret),"+m" (*obj)
	:"a" (old_v[0]), "d" (old_v[1]), "b" (new_v[0]), "c" (new_v[1]));
	atomic_thread_fence(morder);
	return ret;
}
#else
inline static int _WideCAS(volatile __uint128_t *obj, __uint128_t old_value, 
	__uint128_t new_value, memory_order morder) {
	errexit("WCAS not supported with -m32.");
}
#endif
inline static int WideCAS(volatile __uint128_t *obj, __uint128_t old_value, 
	__uint128_t new_value) {
	return _WideCAS(obj, old_value, new_value, memory_order_seq_cst); 
}

static void lf_fifo_init(lf_fifo_queue_t *queue);
static void lf_fifo_enqueue(lf_fifo_queue_t *queue, void *element);
static void *lf_fifo_dequeue(lf_fifo_queue_t *queue);

inline void lf_fifo_init(lf_fifo_queue_t *queue){
	queue->has_dummy = 1;
	queue->dummy.next.ptr = 0;
	queue->dummy.next.ocount = 0;
	queue->head.ptr = (uint64_t)&queue->dummy;
	queue->head.ocount = 0;
	queue->tail.ptr = queue->head.ptr;
	queue->tail.ocount = queue->head.ocount;
	FLUSH(queue);

}
inline void *lf_fifo_dequeue(lf_fifo_queue_t *queue)
{
	//TODO: make it persistent
	aba_t head;
	aba_t tail;
	aba_t next;
	void* ret = NULL;
	while(1) {//retry
		while(1) {
			head = queue->head;
			tail = queue->tail;
			next.ptr = ((struct queue_elem_t*)head.ptr)->next.ptr;
			FLUSHFENCE;
			if(*((__uint128_t*)&head) == *((volatile __uint128_t*)&queue->head)){//check if head, tail and next are still consistent
				if(head.ptr == tail.ptr){//queue is empty or tail falls behind
					if(next.ptr == 0){//only one node there
						if(head.ptr != (uint64_t)&queue->dummy){//the only node isn't dummy
							uint64_t zero=0;
							//so there's no dummy. insert one first
							if(atomic_compare_exchange_strong(&queue->has_dummy,&zero,1)){
								lf_fifo_enqueue(queue, &queue->dummy);
							}
							continue;
						}
						return NULL;
					}
					next.ocount = tail.ocount+1;
					WideCAS((volatile __uint128_t *)&queue->tail, 
							*((__uint128_t*)&tail), *((__uint128_t*)&next));
				} else{//no need to consider tail
					next.ocount = head.ocount+1;
					if(WideCAS((volatile __uint128_t *)&queue->head, 
						*((__uint128_t*)&head), *((__uint128_t*)&next))){
							ret = (void*)head.ptr;
							break;
					}
				}
			}
		}
		((struct queue_elem_t*)head.ptr)->next.ptr = 0;//unpoison the detached node
		if(head.ptr == (uint64_t)&queue->dummy){//the removed one is dummy
			queue->has_dummy = 0;
			FLUSHFENCE;
			uint64_t zero=0;
			if(atomic_compare_exchange_strong(&queue->has_dummy,&zero,1)){
				lf_fifo_enqueue(queue, &queue->dummy);
			}
			continue;//aka goto retry
		}
		return ret;
	}
}

inline void lf_fifo_enqueue(lf_fifo_queue_t *queue, void *element)
{
	//TODO: make it persistent
	aba_t tail;
	aba_t last_node;
	aba_t new_node;
	((struct queue_elem_t *)element)->next.ptr = 0;
	((struct queue_elem_t *)element)->next.ocount = 0;
	new_node.ptr = (uint64_t)element;

	while(1) {
		tail = queue->tail;
		last_node = ((struct queue_elem_t*)tail.ptr)->next;
		if(*((__uint128_t*)&tail) == *((volatile __uint128_t*)&queue->tail)) {//check if tail and last_node are still consistent
			if(last_node.ptr == 0){//check if tail points to the real last node
				new_node.ocount = last_node.ocount + 1;
				if(WideCAS((volatile __uint128_t *)&((struct queue_elem_t*)tail.ptr)->next,
					*((__uint128_t*)&last_node), *((__uint128_t*)&new_node))){
					break;
				}
			} else{//tail doesn't point to the real last node, try to fix
				aba_t new_tail;
				new_tail.ptr = last_node.ptr;
				new_tail.ocount = tail.ocount+1;
				WideCAS((volatile __uint128_t *)&queue->tail, 
					*((__uint128_t*)&tail), *((__uint128_t*)&new_tail));
			}
		}
	}
	new_node.ocount = tail.ocount+1;
	WideCAS((volatile __uint128_t *)&queue->tail, *((__uint128_t*)&tail), *((__uint128_t*)&new_node));
	return;
}


/* This is large and annoying, and we still need to call initialization routine*/
sizeclass sizeclasses[2048 / GRANULARITY] =
				{
				{LF_FIFO_QUEUE_STATIC_INIT, 8, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 16, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 24, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 32, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 40, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 48, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 56, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 64, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 72, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 80, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 88, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 96, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 104, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 112, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 120, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 128, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 136, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 144, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 152, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 160, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 168, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 176, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 184, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 192, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 200, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 208, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 216, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 224, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 232, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 240, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 248, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 256, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 264, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 272, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 280, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 288, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 296, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 304, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 312, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 320, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 328, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 336, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 344, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 352, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 360, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 368, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 376, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 384, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 392, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 400, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 408, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 416, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 424, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 432, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 440, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 448, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 456, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 464, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 472, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 480, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 488, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 496, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 504, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 512, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 520, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 528, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 536, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 544, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 552, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 560, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 568, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 576, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 584, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 592, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 600, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 608, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 616, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 624, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 632, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 640, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 648, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 656, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 664, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 672, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 680, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 688, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 696, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 704, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 712, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 720, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 728, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 736, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 744, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 752, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 760, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 768, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 776, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 784, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 792, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 800, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 808, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 816, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 824, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 832, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 840, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 848, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 856, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 864, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 872, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 880, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 888, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 896, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 904, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 912, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 920, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 928, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 936, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 944, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 952, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 960, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 968, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 976, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 984, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 992, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1000, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1008, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1016, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1024, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1032, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1040, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1048, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1056, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1064, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1072, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1080, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1088, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1096, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1104, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1112, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1120, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1128, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1136, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1144, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1152, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1160, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1168, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1176, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1184, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1192, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1200, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1208, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1216, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1224, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1232, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1240, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1248, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1256, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1264, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1272, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1280, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1288, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1296, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1304, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1312, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1320, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1328, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1336, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1344, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1352, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1360, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1368, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1376, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1384, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1392, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1400, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1408, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1416, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1424, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1432, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1440, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1448, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1456, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1464, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1472, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1480, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1488, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1496, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1504, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1512, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1520, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1528, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1536, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1544, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1552, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1560, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1568, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1576, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1584, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1592, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1600, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1608, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1616, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1624, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1632, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1640, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1648, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1656, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1664, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1672, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1680, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1688, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1696, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1704, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1712, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1720, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1728, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1736, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1744, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1752, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1760, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1768, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1776, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1784, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1792, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1800, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1808, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1816, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1824, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1832, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1840, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1848, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1856, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1864, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1872, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1880, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1888, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1896, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1904, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1912, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1920, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1928, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1936, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1944, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1952, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1960, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1968, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1976, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 1984, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 1992, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 2000, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 2008, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 2016, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 2024, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 2032, SBSIZE},
				{LF_FIFO_QUEUE_STATIC_INIT, 2040, SBSIZE}, {LF_FIFO_QUEUE_STATIC_INIT, 2048, SBSIZE},
				};

static inline void sizeclass_init(){
	int i = 0;
	for(;i<2048 / GRANULARITY;i++){
		lf_fifo_init(&sizeclasses[i].Partial);
	}
}

static inline void pm_init(){
	sizeclass_init();
}

__thread procheap* heaps[2048 / GRANULARITY] =	{ };

static volatile descriptor_queue queue_head;

static inline long min(long a, long b)
{
	return a < b ? a : b;
}

static inline long max(long a, long b)
{
	return a > b ? a : b;
}

static void* AllocNewSB(size_t size, uint64_t alignement)
{
	void* addr;

	//mmap will align to a page which is 4096B so the first desc must be aligned to 64B
	addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	if (addr == MAP_FAILED) {
		fprintf(stderr, "AllocNewSB() mmap failed, %lu: ", size);
		switch (errno) {
			case EBADF:	fprintf(stderr, "EBADF"); break;
			case EACCES:	fprintf(stderr, "EACCES"); break;
			case EINVAL:	fprintf(stderr, "EINVAL"); break;
			case ETXTBSY:	fprintf(stderr, "ETXBSY"); break;
			case EAGAIN:	fprintf(stderr, "EAGAIN"); break;
			case ENOMEM:	fprintf(stderr, "ENOMEM"); break;
			case ENODEV:	fprintf(stderr, "ENODEV"); break;
		}
		fprintf(stderr, "\n");
		fflush(stderr);
		exit(1);
	}
	else if (addr == NULL) {
		fprintf(stderr, "AllocNewSB() mmap of size %lu returned NULL\n", size);
		fflush(stderr);
		exit(1);
	}

	return addr;
}

static void organize_desc_list(descriptor* start, uint64_t count, uint64_t stride)
{
	uint64_t ptr;
	unsigned int i;
 
	start->Next = (descriptor*)(start + stride);
	FLUSH(&start->Next);
	ptr = (uint64_t)start; 
	for (i = 1; i < count - 1; i++) {
		ptr += stride;
		((descriptor*)ptr)->Next = (descriptor*)(ptr + stride);
		FLUSH(&((descriptor*)ptr)->Next);
	}
	ptr += stride;
	((descriptor*)ptr)->Next = NULL;
	FLUSH(&((descriptor*)ptr)->Next);
	//fence can be done until start is published.
}

static void organize_list(void* start, uint64_t count, uint64_t stride)
{
	uint64_t ptr;
	uint64_t i;
  
	ptr = (uint64_t)start; 
	for (i = 1; i < count - 1; i++) {
		ptr += stride;
		*((uint64_t*)ptr) = i + 1;
		FLUSH((uint64_t*)ptr);
	}
}

static descriptor* DescAlloc() {
  
	descriptor_queue old_queue, new_queue;
	descriptor* desc;

#ifdef DEBUG
	fprintf(stderr, "In DescAlloc\n");
	fflush(stderr);
#endif

	while(1) {
		//tag version of ABA-free solution
		old_queue = queue_head;
		if (old_queue.DescAvail) {
			new_queue.DescAvail = (uint64_t)((descriptor*)old_queue.DescAvail)->Next;
			new_queue.tag = old_queue.tag + 1;
			FLUSHFENCE;
			if (WideCAS((volatile __uint128_t *)&queue_head, *((__uint128_t*)&old_queue), *((__uint128_t*)&new_queue))) {
				FLUSH(&queue_head);
				FLUSHFENCE;
				desc = (descriptor*)old_queue.DescAvail;
#ifdef DEBUG
				fprintf(stderr, "Returning recycled descriptor %p (tag %hu)\n", desc, queue_head.tag);
				fflush(stderr);
#endif
				break;
			}
		}
		else {
			desc = AllocNewSB(DESCSBSIZE, sizeof(descriptor));
			organize_desc_list((void *)desc, DESCSBSIZE / sizeof(descriptor), sizeof(descriptor));

			new_queue.DescAvail = (uint64_t)desc->Next;
			new_queue.tag = old_queue.tag + 1;
			FLUSHFENCE;
			if (WideCAS((volatile __uint128_t *)&queue_head,*((__uint128_t*)&old_queue), *((__uint128_t*)&new_queue))) {
				FLUSH(&queue_head);
				FLUSHFENCE;
#ifdef DEBUG
				fprintf(stderr, "Returning descriptor %p from new descriptor block\n", desc);
				fflush(stderr);
#endif
				break;
			}
			munmap((void*)desc, DESCSBSIZE);   
		}
	}
	return desc;
}

void DescRetire(descriptor* desc)
{
	descriptor_queue old_queue, new_queue;

#ifdef DEBUG
	fprintf(stderr, "Recycling descriptor %p (sb %p)\n", desc, desc->sb);
	fflush(stderr);
#endif  
	do {
		old_queue = queue_head;
		desc->Next = (descriptor*)old_queue.DescAvail;
		FLUSH(&desc->Next);
		new_queue.DescAvail = (uint64_t)desc;
		new_queue.tag = old_queue.tag + 1;
		FLUSHFENCE;
	} while (!WideCAS((volatile __uint128_t *)&queue_head, *((__uint128_t*)&old_queue), *((__uint128_t*)&new_queue)));
	FLUSH(&queue_head);
	FLUSHFENCE;
}

static void ListRemoveEmptyDesc(sizeclass* sc)
{//TODO: double check the correctness
	descriptor *desc;
	int num_non_empty = 0;

	while ((desc = (descriptor *)lf_fifo_dequeue(&sc->Partial))) {
		if (desc->sb == NULL) {
			DescRetire(desc);
		}
		else {
			lf_fifo_enqueue(&sc->Partial, (void *)desc);
			if (++num_non_empty >= 2) break;
		}
	}
}

static descriptor* ListGetPartial(sizeclass* sc)
{
	return (descriptor*)lf_fifo_dequeue(&sc->Partial);
}

static void ListPutPartial(descriptor* desc)
{
	lf_fifo_enqueue(&desc->heap->sc->Partial, (void*)desc);  
}

static void RemoveEmptyDesc(procheap* heap, descriptor* desc)
{
	FLUSHFENCE;
	if (atomic_compare_exchange_strong(&heap->Partial, &desc, NULL)) {
		FLUSH(&heap->Partial);
		FLUSHFENCE;
		DescRetire(desc);
	}
	else {
		FLUSH(&heap->Partial);//ensure the value we read already persist
		FLUSHFENCE;
		ListRemoveEmptyDesc(heap->sc);
	}
}

static descriptor* HeapGetPartial(procheap* heap)
{ 
	descriptor* desc;
	desc = *((descriptor**)&heap->Partial); // casts away the volatile
	do {
		if (desc == NULL) {
			return ListGetPartial(heap->sc);
		}
		FLUSHFENCE;
	} while (!atomic_compare_exchange_weak(&heap->Partial, &desc, NULL));
	FLUSH(&heap->Partial);
	FLUSHFENCE;
	return desc;
}

static void HeapPutPartial(descriptor* desc)
{
	descriptor* prev;
	prev = (descriptor*)desc->heap->Partial; // casts away volatile
	do{
		FLUSHFENCE;
	} while (!atomic_compare_exchange_weak(&desc->heap->Partial, &prev, desc));
	FLUSH(&desc->heap->Partial);
	FLUSHFENCE;
	if (prev) {
		ListPutPartial(prev); 
	}
}

static void UpdateActive(procheap* heap, descriptor* desc, uint64_t morecredits)
{ 
	active oldactive, newactive;
	anchor oldanchor, newanchor;

#ifdef DEBUG
	fprintf(stderr, "UpdateActive() heap->Active %p, credits %lu\n", *((void**)&heap->Active), morecredits);
	fflush(stderr);
#endif

	*((uint64_t*)&oldactive) = 0;
	newactive.ptr = (uint64_t)desc>>6;
	newactive.credits = morecredits - 1;
	FLUSHFENCE;
	if (atomic_compare_exchange_strong((volatile uint64_t *)&heap->Active, ((uint64_t*)&oldactive), *((uint64_t*)&newactive))) {
		FLUSH(&heap->Active);
		FLUSHFENCE;
		return;
	}
	FLUSH(&heap->Active);//ensure the value we read already persist
	FLUSHFENCE;
	// Someone installed another active sb
	// Return credits to sb and make it partial
	oldanchor = desc->Anchor;
	do { 
		newanchor = oldanchor;//atomic_compare_exchange_weak updates old_queue on failure of CAS
		newanchor.count += morecredits;
		newanchor.state = PARTIAL;
		FLUSHFENCE;
	} while (!atomic_compare_exchange_weak((volatile uint64_t *)&desc->Anchor, ((uint64_t*)&oldanchor), *((uint64_t*)&newanchor)));
	FLUSH(&desc->Anchor);
	FLUSHFENCE;

	HeapPutPartial(desc);
}

static descriptor* mask_credits(active oldactive)
{
	uint64_t ret = oldactive.ptr;
	return (descriptor*)(ret<<6);
}

static void* MallocFromActive(procheap *heap) 
{
	active newactive, oldactive;
	descriptor* desc;
	anchor oldanchor, newanchor;
	void* addr;
	uint64_t morecredits = 0;
	unsigned int next = 0;

	// First step: reserve block
	oldactive = heap->Active;
	do { 
		newactive = oldactive;//atomic_compare_exchange updates old_queue on failure of CAS
		if (!(*((uint64_t*)(&oldactive)))) {
			return NULL;
		}
		if (oldactive.credits == 0) {
			*((uint64_t*)(&newactive)) = 0;
#ifdef DEBUG
			fprintf(stderr, "MallocFromActive() setting active to NULL, %lu, %d\n", newactive.ptr, newactive.credits);
			fflush(stderr);
#endif
		}
		else {
			--newactive.credits;
		}
		FLUSHFENCE;
	} while (!atomic_compare_exchange_weak((volatile uint64_t*)&heap->Active, ((uint64_t*)&oldactive), *((uint64_t*)&newactive)));
	FLUSH(&heap->Active);
	FLUSHFENCE;

#ifdef DEBUG
	fprintf(stderr, "MallocFromActive() heap->Active %p, credits %hu\n", *((void**)&heap->Active), oldactive.credits);
	fflush(stderr);
#endif

	// Second step: pop block
	desc = mask_credits(oldactive);
	oldanchor = desc->Anchor;
	do {
		// state may be ACTIVE, PARTIAL or FULL
		newanchor = oldanchor;//atomic_compare_exchange updates old_queue on failure of CAS
		addr = (void *)((uint64_t)desc->sb + oldanchor.avail * desc->sz);
		next = *(uint64_t *)addr;
		newanchor.avail = next;
		++newanchor.tag;

		if (oldactive.credits == 0) {

			// state must be ACTIVE
			if (oldanchor.count == 0) {
#ifdef DEBUG
				fprintf(stderr, "MallocFromActive() setting superblock %p to FULL\n", desc->sb);
				fflush(stderr);
#endif
				newanchor.state = FULL;
			}
			else { 
				morecredits = min(oldanchor.count, MAXCREDITS);
				newanchor.count -= morecredits;
			}
		} 
		FLUSHFENCE;
	} while (!atomic_compare_exchange_weak((volatile uint64_t*)&desc->Anchor, ((uint64_t*)&oldanchor), *((uint64_t*)&newanchor)));
	FLUSH(&desc->Anchor);
	FLUSHFENCE;
#ifdef DEBUG
	fprintf(stderr, "MallocFromActive() sb %p, Active %p, avail %d, oldanchor.count %hu, newanchor.count %hu, morecredits %lu, MAX %d\n", 
			desc->sb, *((void**)&heap->Active), desc->Anchor.avail, oldanchor.count, newanchor.count, morecredits, MAXCREDITS);
	fflush(stderr);
#endif

	if (oldactive.credits == 0 && oldanchor.count > 0) {
		UpdateActive(heap, desc, morecredits);
	}

	*((char*)addr) = (char)SMALL; 
	addr += TYPE_SIZE;
	*((descriptor**)addr) = desc; 
	FLUSH(addr-TYPE_SIZE);
	FLUSH(addr);
	FLUSHFENCE;//so that type and desc of each block won't lose
	return ((void*)((uint64_t)addr + PTR_SIZE));
}

static void* MallocFromPartial(procheap* heap)
{
	descriptor* desc;
	anchor oldanchor, newanchor;
	uint64_t morecredits;
	void* addr;
  
retry:
	desc = HeapGetPartial(heap);
	if (!desc) {
		return NULL;
	}

	desc->heap = heap;
	FLUSH(&desc->heap);
	oldanchor = desc->Anchor;
	do {
		// reserve blocks
		newanchor = oldanchor;
		if (oldanchor.state == EMPTY) {
			DescRetire(desc); 
			goto retry;
		}

		// oldanchor state must be PARTIAL
		// oldanchor count must be > 0
		morecredits = min(oldanchor.count - 1, MAXCREDITS);
		newanchor.count -= morecredits + 1;
		newanchor.state = (morecredits > 0) ? ACTIVE : FULL;
		FLUSHFENCE;
	} while (!atomic_compare_exchange_weak((volatile uint64_t*)&desc->Anchor, ((uint64_t*)&oldanchor), *((uint64_t*)&newanchor)));
	FLUSH(&desc->Anchor);
	FLUSHFENCE;

	oldanchor = desc->Anchor;
	do { 
		// pop reserved block
		newanchor = oldanchor;
		addr = (void*)((uint64_t)desc->sb + oldanchor.avail * desc->sz);

		newanchor.avail = *(uint64_t*)addr;
		++newanchor.tag;
		FLUSHFENCE;
	} while (!atomic_compare_exchange_weak((volatile uint64_t*)&desc->Anchor, ((uint64_t*)&oldanchor), *((uint64_t*)&newanchor)));
	FLUSH(&desc->Anchor);
	FLUSHFENCE;

	if (morecredits > 0) {
		UpdateActive(heap, desc, morecredits);
	}

	*((char*)addr) = (char)SMALL; 
	addr += TYPE_SIZE;
	*((descriptor**)addr) = desc; 
	FLUSH(addr-TYPE_SIZE);
	FLUSH(addr);
	FLUSHFENCE;//so that type and desc at the head of each block won't lose
	return ((void *)((uint64_t)addr + PTR_SIZE));
}

static void* MallocFromNewSB(procheap* heap)
{
	descriptor* desc;
	void* addr;
	active newactive, oldactive;

	*((uint64_t*)&oldactive) = 0;
	desc = DescAlloc();
	desc->sb = AllocNewSB(heap->sc->sbsize, SBSIZE);

	desc->heap = heap;
	desc->Anchor.avail = 1;
	desc->sz = heap->sc->sz;
	desc->maxcount = heap->sc->sbsize / desc->sz;
	FLUSH(&desc->sb);
	FLUSH(&desc->heap);
	FLUSH(&desc->sz);
	FLUSH(&desc->maxcount);

	// Organize blocks in a linked list starting with index 0.
	organize_list(desc->sb, desc->maxcount, desc->sz);

#ifdef DEBUG
	fprintf(stderr, "New SB %p associated with desc %p (sz %u, sbsize %d, heap %p, Anchor.avail %hu, Anchor.count %hu)\n", 
			desc->sb, desc, desc->sz, heap->sc->sbsize, heap, desc->Anchor.avail, desc->Anchor.count);
	fflush(stderr);
#endif

	*((uint64_t*)&newactive) = 0;
	newactive.ptr = (uint64_t)desc>>6;
	newactive.credits = min(desc->maxcount - 1, MAXCREDITS) - 1;

	desc->Anchor.count = max(((signed long)desc->maxcount - 1 ) - ((signed long)newactive.credits + 1), 0); // max added by Scott
	desc->Anchor.state = ACTIVE;
	FLUSH(&desc->Anchor);

#ifdef DEBUG
	fprintf(stderr, "MallocFromNewSB() sz %u, maxcount %u, Anchor.count %hu, newactive.credits %hu, max %ld\n", 
			desc->sz, desc->maxcount, desc->Anchor.count, newactive.credits, 
			((signed long)desc->maxcount - 1 ) - ((signed long)newactive.credits + 1));
	fflush(stderr);
#endif

	// memory fence.
	FLUSHFENCE;
	if (atomic_compare_exchange_strong((volatile uint64_t*)&heap->Active, ((uint64_t*)&oldactive), *((uint64_t*)&newactive))) { 
		FLUSH(&heap->Active);
		addr = desc->sb;
		*((char*)addr) = (char)SMALL; 
		addr += TYPE_SIZE;
		*((descriptor **)addr) = desc; 
		FLUSH(addr-TYPE_SIZE);
		FLUSH(addr);//so that desc at the head of each block won't lose
		FLUSHFENCE;
		return (void *)((uint64_t)addr + PTR_SIZE);
	} 
	else {
		//Free the superblock desc->sb.
		FLUSH(&heap->Active);//ensure the value we read already persist
		FLUSHFENCE;
		munmap(desc->sb, desc->heap->sc->sbsize);
		DescRetire(desc); 
		return NULL;
	}
}

static procheap* find_heap(size_t sz)
{
	procheap* heap;
  
	// We need to fit both the object and the descriptor in a single block
	sz += HEADER_SIZE;
	if (sz > 2048) {
		return NULL;
	}
  
	heap = heaps[sz / GRANULARITY];
	if (heap == NULL) {
		heap = mmap(NULL, sizeof(procheap), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		*((uint64_t*)&(heap->Active)) = 0;
		heap->Partial = NULL;
		heap->sc = &sizeclasses[sz / GRANULARITY];
		FLUSH(&heap->Partial);
		FLUSH(&heap->sc);
		heaps[sz / GRANULARITY] = heap;
		FLUSH(&heaps[sz / GRANULARITY]);
		FLUSHFENCE;//ensure all writes persist before getting used
	}
	
	return heap;
}

static void* alloc_large_block(size_t sz)
{
	void* addr;
	addr = mmap(NULL, sz + HEADER_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	// If the highest bit of the descriptor is 1, then the object is large (allocated / freed directly from / to the OS)
	*((char*)addr) = (char)LARGE;
	addr += TYPE_SIZE;
	*((uint64_t *)addr) = sz + HEADER_SIZE;
	FLUSH(addr-TYPE_SIZE);
	FLUSH(addr);
	FLUSHFENCE;//so that type and desc at the head of each block won't lose
	return (void*)(addr + PTR_SIZE); 
}

void* PM_malloc(size_t sz)
{ 
	procheap *heap;
	void* addr;
	while(init_flag == 0){
		//the library isn't init
		int zero = 0;
		FLUSHFENCE;
		if(atomic_compare_exchange_weak(&init_flag,&zero,1)){
			//only one will succeed CASing it to 1
			pm_init();
			FLUSH(&init_flag);
			FLUSHFENCE;
#ifdef DEBUG
			fprintf(stderr, "pm library is initialized!\n");
			fflush(stderr);
#endif
			break;
		}
	}
#ifdef DEBUG
	fprintf(stderr, "malloc() sz %lu\n", sz);
	fflush(stderr);
#endif
	// Use sz and thread id to find heap.
	heap = find_heap(sz);

	if (!heap) {
		// Large block
		addr = alloc_large_block(sz);
#ifdef DEBUG
		fprintf(stderr, "Large block allocation: %p\n", addr);
		fflush(stderr);
#endif
		return addr;
	}

	while(1) { 
		addr = MallocFromActive(heap);
		if (addr) {
#ifdef DEBUG
			fprintf(stderr, "malloc() return MallocFromActive %p\n", addr);
			fflush(stderr);
#endif
			return addr;
		}
		addr = MallocFromPartial(heap);
		if (addr) {
#ifdef DEBUG
			fprintf(stderr, "malloc() return MallocFromPartial %p\n", addr);
			fflush(stderr);
#endif
			return addr;
		}
		addr = MallocFromNewSB(heap);
		if (addr) {
#ifdef DEBUG
			fprintf(stderr, "malloc() return MallocFromNewSB %p\n", addr);
			fflush(stderr);
#endif
			return addr;
		}
	} 
}

void PM_free(void* ptr) 
{
	descriptor* desc;
	void* sb;
	anchor oldanchor, newanchor;
	procheap* heap = NULL;
	while(init_flag == 0){
		//the library isn't init
		int zero = 0;
		FLUSHFENCE;
		if(atomic_compare_exchange_weak(&init_flag,&zero,1)){
			//only one will succeed CASing it to 1
			pm_init();
			FLUSH(&init_flag);
			FLUSHFENCE;
#ifdef DEBUG
			fprintf(stderr, "pm library is initialized!\n");
			fflush(stderr);
#endif
			break;
		}
	}
#ifdef DEBUG
	fprintf(stderr, "Calling my free %p\n", ptr);
	fflush(stderr);
#endif

	if (!ptr) {
		return;
	}
	
	if(*((char*)((uint64_t)ptr - HEADER_SIZE)) != (char)LARGE && *((char*)((uint64_t)ptr - HEADER_SIZE)) != (char)SMALL) {//this block wasn't allocated by pmmalloc, call regular free by default.
		free(ptr);
		return;
	}
	// get prefix
	ptr = (void*)((uint64_t)ptr - HEADER_SIZE);  
	if (*((char*)ptr) == (char)LARGE) {
#ifdef DEBUG
		fprintf(stderr, "Freeing large block\n");
		fflush(stderr);
#endif
		munmap(ptr, *((uint64_t *)(ptr + TYPE_SIZE)));
		return;
	}
	desc = *((descriptor**)((uint64_t)ptr + TYPE_SIZE));
	
	sb = desc->sb;
	oldanchor = desc->Anchor;
	do { 
		newanchor = oldanchor;

		*((uint64_t*)ptr) = oldanchor.avail;
		newanchor.avail = ((uint64_t)ptr - (uint64_t)sb) / desc->sz;

		if (oldanchor.state == FULL) {
#ifdef DEBUG
			fprintf(stderr, "Marking superblock %p as PARTIAL\n", sb);
			fflush(stderr);
#endif
			newanchor.state = PARTIAL;
		}

		if (oldanchor.count == desc->maxcount - 1) {
			heap = desc->heap;
			INSTRFENCE;// instruction fence.
#ifdef DEBUG
			fprintf(stderr, "Marking superblock %p as EMPTY; count %d\n", sb, oldanchor.count);
			fflush(stderr);
#endif
			newanchor.state = EMPTY;
		} 
		else {
			++newanchor.count;
		}
		// memory fence.
		FLUSHFENCE;
	} while (!atomic_compare_exchange_weak((volatile uint64_t*)&desc->Anchor, ((uint64_t*)&oldanchor), *((uint64_t*)&newanchor)));
	FLUSH(&desc->Anchor);
	FLUSHFENCE;
	if (newanchor.state == EMPTY) {
#ifdef DEBUG
		fprintf(stderr, "Freeing superblock %p with desc %p (count %hu)\n", sb, desc, desc->Anchor.count);
		fflush(stderr);
#endif

		munmap(sb, heap->sc->sbsize);
		RemoveEmptyDesc(heap, desc);
	} 
	else if (oldanchor.state == FULL) {
#ifdef DEBUG
		fprintf(stderr, "Puting superblock %p to PARTIAL heap\n", sb);
		fflush(stderr);
#endif
		HeapPutPartial(desc);
	}
}

void *PM_calloc(size_t nmemb, size_t size)
{//not persistent!
	void *ptr;
	
	ptr = PM_malloc(nmemb*size);
	if (!ptr) {
		return NULL;
	}

	return memset(ptr, 0, nmemb*size);
}

void *PM_valloc(size_t size)
{
	fprintf(stderr, "valloc() called in libmaged. Not implemented. Exiting.\n");
	fflush(stderr);
	exit(1);
}

void *PM_memalign(size_t boundary, size_t size)
{
	void *p;

	p = PM_malloc((size + boundary - 1) & ~(boundary - 1));
	if (!p) {
		return NULL;
	}

	return(void*)(((uint64_t)p + boundary - 1) & ~(boundary - 1)); 
}

int PM_posix_memalign(void **memptr, size_t alignment, size_t size)
{
	*memptr = PM_memalign(alignment, size);
	if (*memptr) {
		return 0;
	}
	else {
		/* We have to "personalize" the return value according to the error */
		return -1;
	}
}

void *PM_realloc(void *object, size_t size)
{//not persistent!
	descriptor* desc;
	void* header;
	void* ret;

	if (object == NULL) {
		return PM_malloc(size);
	}
	else if (size == 0) {
		PM_free(object);
		return NULL;
	}

	header = (void*)((uint64_t)object - HEADER_SIZE);  

	if (*((char*)header) == (char)LARGE) {
		ret = PM_malloc(size);
		memcpy(ret, object, *((uint64_t *)(header + TYPE_SIZE)));
		munmap(object, *((uint64_t *)(header + TYPE_SIZE)));
	}
	else {
		desc = *((descriptor**)((uint64_t)header + TYPE_SIZE));
		if (size <= desc->sz - HEADER_SIZE) {
			ret = object;
		}
		else {
			ret = PM_malloc(size);
			memcpy(ret, object, desc->sz - HEADER_SIZE);
			PM_free(object);
		}
	}

	return ret;
}

