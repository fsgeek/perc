/*
 * Copyright (C) 2018 Tony Mason
 */

#include <stdlib.h>

//
// Some CPU functions
//
int cpu_has_rtm(void);
int cpu_has_hle(void);
int cpu_has_clwb(void);
int cpu_has_clflushopt(void);
size_t cpu_cacheline_size(void);
unsigned cpu_frequency(void);
unsigned cpu_rdtsc(void);
void cpu_prefetch_l1(const void *addr);
void cpu_prefetch_l2(const void *addr);
void cpu_prefetch_l3(const void *addr);
void cpu_prefetch_oneuse(const void *addr);

