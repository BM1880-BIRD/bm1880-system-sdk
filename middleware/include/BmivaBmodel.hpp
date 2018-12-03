// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef SRC_BMIVALOADABLE_HPP_
#define SRC_BMIVALOADABLE_HPP_

#include <vector>
#include <string>
#include "config.h"
#include "bmtap2.h"
#include "bmiva_type.hpp"
#include "json11.hpp"

using std::string;
using std::vector;

struct bmodel_def {
  string   path;
  int      device_type;
  int      in, ic, ih, iw;
  uint64_t input_size;
  uint64_t output_size;
  uint64_t neuron_size;
  uint64_t output_offset;
  bmnet_t  net_handle;
//  bmmem_device_t input_mem;
//  bmmem_device_t output_mem;
};

class BmivaBmodel {
 public:
  explicit BmivaBmodel(const vector<string> &model_path);
  BmivaBmodel();
  ~BmivaBmodel();
  int RegisterModel(const string &model);

  bmiva_err_t GetOutPutInfo(int         model_index,
                               bmnet_output_info_t *outputInfo);

  bmiva_err_t Predict(int         model_index,
                      BmivaTensor *input,
                      BmivaTensor *output);
  bmiva_err_t PredictOpt(int         model_index,
                         BmivaTensor *input,
                         BmivaTensor *output);
  bmiva_err_t PredictNoOutput(int         model_index,
                              BmivaTensor *input,
                              BmivaTensor *output);
  int                        nodechip_num_;
  bmiva_err_t U8ToF32(uint64_t src, int N, int C, int H, int W,
                               int S_N, int S_C, int S_H,
                               float alpha, float beta, uint64_t dst);
  bmiva_err_t U8ToU8(uint64_t src, int N, int C, int H, int W,
                     int S_N, int S_C, int S_H, uint64_t dst);
  uint64_t GetPaddr(int model_index, int N, int C, int H, int W);

 private:
  int                        device_id_;
  uint32_t                   count_;
  bmctx_t                    handle_;
  bmiva_chip_t               chip_type_;
  bmiva_datatype_t           data_type_;
  vector<struct bmodel_def> models_;
  void LoadModelFromFile(struct bmodel_def *model, const string &model_path);
  void RegisterBmnet(struct bmodel_def *new_model);
};
#endif  // SRC_BMIVALOADABLE_HPP_
