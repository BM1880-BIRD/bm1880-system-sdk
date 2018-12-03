// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef SRC_BMIVAJSON_HPP_
#define SRC_BMIVAJSON_HPP_

#include <vector>
#include <string>
#include "config.h"
#ifndef CONFIG_BMTAP_V2
#include "bm_runtime.h"
#include "bmdnn_net_api.h"
#include "host_util.h"
#else
#include "bmtap2.h"
#endif
#include "json11.hpp"
#include "bmiva_type.hpp"

using std::string;
using std::vector;

struct bmnet_model {
  string                    path;
#ifdef CONFIG_BMTAP_V2
  int                       weight_size;
  int                       neuron_size;
  bmmem_device_t            weight_mem;
  bmmem_device_t            neuron_mem;
#endif
  vector<struct bmnet_func> functions;
};

struct bmnet_func {
  string   name;
  string   cmdbuf;
  int      device_type;
  int      nodechip_num;
  int      in, ic, ih, iw;
  uint64_t input_size;
  uint64_t output_size;
  uint64_t weight_size;
  uint64_t neuron_size;
  uint64_t output_offset;
#ifndef CONFIG_BMTAP_V2
  bmdnn_net_handle_t net_handle;
#else
  bmnet_t  net_handle;
  bmmem_device_t input_mem;
  bmmem_device_t output_mem;
#endif
};

class BmivaJson {
 public:
  explicit BmivaJson(const vector<string> &model_path);
  BmivaJson();
  ~BmivaJson();
  int RegisterModel(const string &model);
  bmiva_err_t Predict(int         model_index,
                      BmivaTensor *input,
                      BmivaTensor *output);
#ifdef CONFIG_BMTAP_V2
  bmiva_err_t U8ToF32(uint64_t src, int N, int C, int H, int W,
                               int S_N, int S_C, int S_H,
                               float alpha, float beta, uint64_t dst);
  bmiva_err_t U8ToU8(uint64_t src, int N, int C, int H, int W,
                     int S_N, int S_C, int S_H, uint64_t dst);
  uint64_t GetPaddr(int model_index, int N, int C, int H, int W);
  bmiva_err_t PredictOpt(int         model_index,
                         BmivaTensor *input,
                         BmivaTensor *output);
  bmiva_err_t PredictNoOutput(int         model_index,
                              BmivaTensor *input,
                              BmivaTensor *output);
#endif
  int                        nodechip_num_;

 private:
  int                        device_id_;
  uint32_t                   count_;
#ifndef CONFIG_BMTAP_V2
  bmdnn_handle_t             handle_;
#else
  bmctx_t                    handle_;
#endif
  bmiva_chip_t               chip_type_;
  bmiva_datatype_t           data_type_;
  vector<struct bmnet_model> models_;
  void LoadModelFromFile(struct bmnet_model *model, const string &model_path);
  void RegisterBmnet(struct bmnet_model *new_model, const string &weight_path);
};

#endif  // SRC_BMIVAJSON_HPP_
