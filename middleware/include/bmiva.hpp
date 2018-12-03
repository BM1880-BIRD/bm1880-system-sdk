// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef INCLUDE_BMIVA_HPP_
#define INCLUDE_BMIVA_HPP_

#include <vector>
#include <string>

#include "bmiva_engine.hpp"
#include "bmiva_type.hpp"

using std::string;
using std::vector;

// device api
#if defined(__aarch64__)
extern void* bmiva_mem_alloc(bmiva_dev_t handle, size_t size);
extern void bmiva_mem_sync(bmiva_dev_t handle, void *data);
extern void bmiva_mem_free(bmiva_dev_t handle, void *data);
extern void* bmiva_cached_mem_alloc(bmiva_dev_t handle, size_t size);
extern uint64_t bmiva_phy_addr(bmiva_dev_t handle , void *data);
extern uint64_t bmiva_get_vaddr(bmiva_dev_t handle , void *data);
extern uint64_t bmiva_get_paddr(bmiva_dev_t handle , void *data);
extern bmiva_err_t bmiva_mem_dmacpy(bmiva_dev_t    handle,
                                    uint64_t src, int N, int C, int H,
                                    int W, int S_N, int S_C, int S_H,
                                    uint64_t dst);
extern bmiva_err_t bmiva_mem_dmacpy(bmiva_dev_t    handle,
                                    uint64_t src, int N, int C, int H,
                                    int W, int S_N, int S_C, int S_H,
                                    void *hd_ion);
#endif

extern bmiva_err_t
bmiva_show_runtime_stat_start(bmiva_dev_t dev);

extern float
bmiva_show_runtime_stat_get(bmiva_dev_t dev);

extern void
bmiva_show_runtime_stat_get(bmiva_dev_t dev, vector<long> &stat);

extern bmiva_err_t
bmiva_show_runtime_stat_show(bmiva_dev_t dev);

#endif  // INCLUDE_BMIVA_HPP_
