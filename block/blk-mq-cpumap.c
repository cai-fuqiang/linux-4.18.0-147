/*
 * CPU <-> hardware queue mapping helpers
 *
 * Copyright (C) 2013-2014 Jens Axboe
 */
#include <linux/kernel.h>
#include <linux/threads.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/cpu.h>

#include <linux/blk-mq.h>
#include "blk.h"
#include "blk-mq.h"

static int queue_index(struct blk_mq_queue_map *qmap,
		       unsigned int nr_queues, const int q)
{
	return qmap->queue_offset + (q % nr_queues);
}

static int get_first_sibling(unsigned int cpu)
{
	unsigned int ret;

	ret = cpumask_first(topology_sibling_cpumask(cpu));
	if (ret < nr_cpu_ids)
		return ret;

	return cpu;
}

int blk_mq_map_queues(struct blk_mq_queue_map *qmap)
{
	unsigned int *map = qmap->mq_map;
	unsigned int nr_queues = qmap->nr_queues;       //nr_hw_queues
	unsigned int cpu, first_sibling, q = 0;

	for_each_possible_cpu(cpu)
		map[cpu] = -1;

	/*
	 * Spread queues among present CPUs first for minimizing
	 * count of dead queues which are mapped by all un-present CPUs
	 */
	for_each_present_cpu(cpu) {     //当前的cpu个数, 优先分配present cpu
		if (q >= nr_queues)         //也就是cpu个数 > 硬件队列个数
			break;
		map[cpu] = queue_index(qmap, nr_queues, q++);
	}

	for_each_possible_cpu(cpu) {    //这个是为了处理cpu个数和硬件队列个数不匹配的问题
		if (map[cpu] != -1)
			continue;
		/*
		 * First do sequential mapping between CPUs and queues.
		 * In case we still have CPUs to map, and we have some number of
		 * threads per cores then map sibling threads to the same queue
		 * for performance optimizations.
		 */
		if (q < nr_queues) {    //这个是表示 cpu个数 < 硬件队列个数，硬件队列个数多了, 直接映射就完事了
			map[cpu] = queue_index(qmap, nr_queues, q++);   //分配到possible cpu上
		} else {                                            //这个是cpu > 硬件队列数
			first_sibling = get_first_sibling(cpu);         //寻找同一个core下的cpu, 同一个core下的cpu
			if (first_sibling == cpu)
				map[cpu] = queue_index(qmap, nr_queues, q++);
			else
				map[cpu] = map[first_sibling];              //不知道搞啥
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(blk_mq_map_queues);

/*
 * We have no quick way of doing reverse lookups. This is only used at
 * queue init time, so runtime isn't important.
 */
int blk_mq_hw_queue_to_node(struct blk_mq_queue_map *qmap, unsigned int index)
{
	int i;

	for_each_possible_cpu(i) {
		if (index == qmap->mq_map[i])
			return local_memory_node(cpu_to_node(i));
	}

	return NUMA_NO_NODE;
}
