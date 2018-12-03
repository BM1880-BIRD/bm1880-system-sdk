// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef SRC_BMIVADEVICE_HPP_
#define SRC_BMIVADEVICE_HPP_

#include <vector>
#include <string>
#include <pthread.h>
#include "Config.hpp"
#include "FileMonitor.hpp"
#include "config.h"
#include "BmivaJson.hpp"
#ifdef CONFIG_BMODEL
#include "BmivaBmodel.hpp"
#endif
#include "bmiva_type.hpp"
#include "bmiva_ion.hpp"
#ifdef CONFIG_TVM_RUNTIME
#include "BmivaTVM.hpp"
#endif

using std::string;
using std::vector;

class BmivaDevice {
 public:
  int debug_on_;
  int profiling_on_;
  int runtime_stat_on_;
  explicit BmivaDevice(const vector<string> &model_path);
  BmivaDevice();
  ~BmivaDevice();
  int RegisterModel(const string &model);
  int NodechipNum();
  bmiva_err_t Predict(int         model_index,
                      BmivaTensor *input,
                      BmivaTensor *output);
  bmiva_err_t GetOutPutInfo(int   model_index,
                            bmnet_output_info_t *outputInfo);
#if defined(__aarch64__)
  void *ionMemAlloc(size_t size);
  void *ionCachedMemAlloc(size_t size);
  uint64_t getIonPhyAddr(void *data);
  uint64_t getIonVAddr(void *data);
  void ionMemSync(void *data);
  void ionMemSync(void *addr , unsigned long size);
  void ionMemFree(void *data);
#endif
#ifdef CONFIG_BMTAP_V2
  bmiva_err_t U8ToF32(uint64_t src, int N, int C, int H, int W,
                               int S_N, int S_C, int S_H,
                               float alpha, float beta, uint64_t dst);
#if defined(__aarch64__)
  bmiva_err_t U8ToU8(uint64_t src, int N, int C, int H, int W,
                     int S_N, int S_C, int S_H, uint64_t dst);
#endif
  uint64_t GetPaddr(int model_index, int N, int C, int H, int W);
  bmiva_err_t PredictOpt(int         model_index,
                         BmivaTensor *input,
                         BmivaTensor *output);
  bmiva_err_t PredictNoOutput(int         model_index,
                              BmivaTensor *input,
                              BmivaTensor *output);
#endif
  void RuntimeStatShow();
  void RuntimeStatTotalStart();
  float RuntimeStatTotalGet();
  void RuntimeStatTotalGet(vector<long> &stat);

 private:
  pthread_mutex_t dev_lock_;
  void DeviceInit();
  Config *config_;
  FileMonitor *monitor_;
#ifdef CONFIG_TVM_RUNTIME
  BmivaTVM *device_;
#elif defined CONFIG_BMODEL
  BmivaBmodel *device_;
#else
  BmivaJson *device_;
#endif
#if defined(__aarch64__)
  ion::IonMemAllocator ion_;
#endif
};

extern BmivaDevice *g_dev_handle;

#endif  // SRC_BMIVADEVICE_HPP_
