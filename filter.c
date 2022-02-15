/*
** ATOP - System & Process Monitor
**
** The program 'atop' offers the possibility to view the activity of
** the system on system-level as well as process-level.
**
** ==========================================================================
** Author:      Fei Li & zhenwei pi
** E-mail:      lifei.shirley@bytedance.com, pizhenwei@bytedance.com
** Date:        September 2019
** --------------------------------------------------------------------------
** Copyright (C) 2019 bytedance.com
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2, or (at your option) any
** later version.
**
** This program is distributed in the hope that it will be useful, but
** WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
** --------------------------------------------------------------------------
**
** Revision 1.1  2019/09/22 14:02:19
** Initial revision
** Add support for filtering proccess && task info.
**
**
*/
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "atop.h"
#include "photosyst.h"
#include "photoproc.h"

static int  recordcputop = -1;
static int  recordmemtop = -1;
static count_t max_cpu = 0, max_mem = 0, max_dsk = 0;

extern int get_posval(char *name, char *val);

void
do_recordcputop(char *name, char *val)
{
	recordcputop = get_posval(name, val);
}

void
do_recordmemtop(char *name, char *val)
{
	recordmemtop = get_posval(name, val);
}

static int
compint(const void *value1, const void *value2)
{
	return *(int*)value1 - *(int*)value2;
}

struct devtstat *
devstat_filter_topn(struct devtstat *devtstat)
{
	int *cpu_top_list = NULL, *mem_top_list = NULL;
	struct devtstat *tstat = devtstat;
	int i = 0;

	/* get cpu topN pid list */
	if (recordcputop > 0) {
		if (recordcputop > devtstat->nprocall)
			return devtstat;

		cpu_top_list = (int *)malloc(sizeof(int) * recordcputop);
		ptrverify(cpu_top_list, "Malloc failed for sorting cpu topN");

		qsort(devtstat->procall, devtstat->nprocall, sizeof(struct tstat *),
		      compcpu);

		for (i = 0; i < recordcputop; i++)
			cpu_top_list[i] = devtstat->procall[i]->gen.tgid;

		qsort(cpu_top_list, recordcputop, sizeof(int), compint);
	}

	/* get mem topN pid list */
	if (recordmemtop > 0) {
		if (recordmemtop > devtstat->nprocall)
			return devtstat;

		mem_top_list = (int *)malloc(sizeof(int) * recordmemtop);
		ptrverify(mem_top_list, "Malloc failed for sorting mem topN");

		qsort(devtstat->procall, devtstat->nprocall, sizeof(struct tstat *),
		      compmem);

		for (i = 0; i < recordmemtop; i++)
			mem_top_list[i] = devtstat->procall[i]->gen.tgid;

		qsort(mem_top_list, recordmemtop, sizeof(int), compint);
	}

	/* filter cpu & mem topN by pid list */
	if ((recordcputop > 0) || (recordmemtop > 0)) {
		tstat = (struct devtstat *)malloc(sizeof(struct devtstat));
		ptrverify(tstat, "Malloc failed for sorted devtstat");
		memcpy(tstat, devtstat, sizeof(struct devtstat));
		tstat->ntaskall = 0;
		tstat->taskall = (struct tstat*) malloc(devtstat->ntaskall *
		                                        sizeof(struct tstat));

		for (i = 0; i < devtstat->ntaskall; i++) {
			int tgid = devtstat->taskall[i].gen.tgid;
			if ((recordcputop > 0) && (cpu_top_list != NULL) &&
			    bsearch(&tgid, cpu_top_list, recordcputop, sizeof(int), compint)) {
				tstat->taskall[tstat->ntaskall] = devtstat->taskall[i];
				tstat->ntaskall++;
				continue;
			}

			if ((recordmemtop > 0) && (mem_top_list != NULL) &&
			    bsearch(&tgid, mem_top_list, recordmemtop, sizeof(int), compint)) {
				tstat->taskall[tstat->ntaskall] = devtstat->taskall[i];
				tstat->ntaskall++;
				continue;
			}
		}

		free(cpu_top_list);
		free(mem_top_list);
	}

	return tstat;
}

count_t
get_max_cpu(struct tstat *tstat, int num)
{
	int i = 0;
	count_t max = 0, tmp = 0;
	for (i = 0; i < num; i++, tstat++) {
		tmp = tstat->cpu.stime + tstat->cpu.utime;
		if (max < tmp)
			max = tmp;
	}
	return max;
}

count_t
get_max_mem(struct tstat *tstat, int num)
{
	int i = 0;
	count_t max = 0, tmp = 0;
	for (i = 0; i < num; i++, tstat++) {
		tmp = tstat->mem.rmem;
		if (max < tmp)
			max = tmp;
	}
	return max;
}

count_t
get_max_dsk(struct tstat *tstat, int num)
{
	int i = 0;
	count_t max = 0, tmp = 0;
	for (i = 0; i < num; i++, tstat++) {
		if (tstat->dsk.wsz > tstat->dsk.cwsz)
			tmp = tstat->dsk.rio + tstat->dsk.wsz - tstat->dsk.cwsz;
		else
			tmp = tstat->dsk.rio;
		if (max < tmp)
			max = tmp;
	}
	return max;
}

int
comthread(const void *a, const void *b)
{
	if (!max_cpu && !max_mem && !max_dsk)
		return 0;

	float asum = 0.0, bsum = 0.0;
	struct tstat	*ta = (*(struct tstat **)a);
	struct tstat	*tb = (*(struct tstat **)b);

	register count_t acpu = ta->cpu.stime + ta->cpu.utime;
	register count_t bcpu = tb->cpu.stime + tb->cpu.utime;

	register count_t amem = ta->mem.rmem;
	register count_t bmem = tb->mem.rmem;

	count_t	adsk, bdsk;
	if (ta->dsk.wsz > ta->dsk.cwsz)
		adsk = ta->dsk.rio + ta->dsk.wsz - ta->dsk.cwsz;
	else
		adsk = ta->dsk.rio;
	if (tb->dsk.wsz > tb->dsk.cwsz)
		bdsk = tb->dsk.rio + tb->dsk.wsz - tb->dsk.cwsz;
	else
		bdsk = tb->dsk.rio;

	if (max_mem != 0) {
		asum += (float)amem/max_mem;
		bsum += (float)bmem/max_mem;
	}
	if (max_cpu != 0) {
		asum += (float)acpu/max_cpu;
		bsum += (float)bcpu/max_cpu;
	}
	if (max_dsk != 0) {
		asum += (float)adsk/max_dsk;
		bsum += (float)bdsk/max_dsk;
	}

	if (asum < bsum)
		return 1;
	if (asum > bsum)
		return -1;

	return 0;
}

/*
** filter top threadmax threads, in case one process has too many threads
** then the raw log will be too huge and disk will be soon full
*/
struct devtstat *
devstat_filter_thread(struct devtstat *devtstat)
{
	int old = 0, new = 0;
	struct tstat **tmp;
	struct devtstat *tstat = devtstat;

	if (threadmax <= 0 || threadmax > devtstat->ntaskall)
		return tstat;

	max_cpu = get_max_cpu(devtstat->taskall, devtstat->ntaskall);
	max_mem = get_max_mem(devtstat->taskall, devtstat->ntaskall);
	max_dsk = get_max_dsk(devtstat->taskall, devtstat->ntaskall);

	/* reassign a new tstat to store the filtered devtstat */
	tstat = (struct devtstat *)malloc(sizeof(struct devtstat));
	ptrverify(tstat, "Malloc failed for sorted devtstat");
	memcpy(tstat, devtstat, sizeof(struct devtstat));
	tstat->ntaskall = 0;
	tstat->taskall = (struct tstat*) malloc(devtstat->ntaskall *
						sizeof(struct tstat));

	for (old = 0, new = 0; old < devtstat->ntaskall;) {
		if (devtstat->taskall[old].gen.nthr > threadmax) { // devtstat->taskall->gen.isproc==1

			tstat->taskall[new++] = devtstat->taskall[old];
			tstat->ntaskall++;
			old++;

			/* sort out top threadmax threads */
			int th = 0, nth = 0;
			int tgid = devtstat->taskall[old].gen.tgid;
			/* assign a temporary devtstat ** to filter the topN pids */
			tmp = (struct tstat **)malloc(devtstat->ntaskall * sizeof(struct tstat *));
			ptrverify(tmp, "Malloc failed for temporary devtstat->taskall");
			// Note that j maybe less than tstat->taskall[k-1].gen.nthr
			for (th = 0; devtstat->taskall[old].gen.tgid == tgid; th++, old++) {
				tmp[th] = &(devtstat->taskall[old]);
			}
			/* get top threadmax by (cpu+mem+dsk), and sort by pid */
			qsort(tmp, (th - 1), sizeof(struct tstat *), comthread);
			int min = th < threadmax ? th : threadmax;
			for (nth = 0; nth < min; nth++) {
				tstat->taskall[new++] = *(tmp[nth]);
				tstat->ntaskall++;
			}
			free(tmp);
		} else {
			tstat->taskall[new++] = devtstat->taskall[old++];
			tstat->ntaskall++;
		}
	}

	return tstat;
}

struct devtstat *
filter_devtstat(struct devtstat *devtstat)
{
	struct devtstat *proc_filter_devtstat, *task_filter_devtstat;

	/* sort topN process by cpu & mem */
	proc_filter_devtstat = devstat_filter_topn(devtstat);

	/* in case too many threads occupy the disk space, let's filter topN threads */
	task_filter_devtstat = devstat_filter_thread(proc_filter_devtstat);

	if (task_filter_devtstat != proc_filter_devtstat && proc_filter_devtstat != devtstat) {
		free(proc_filter_devtstat->taskall);
		free(proc_filter_devtstat);
	}

	return task_filter_devtstat;
}
