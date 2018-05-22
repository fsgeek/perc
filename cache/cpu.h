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