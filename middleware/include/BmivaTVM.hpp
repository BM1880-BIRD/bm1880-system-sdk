// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef SRC_BMIVATVM_HPP_
#define SRC_BMIVATVM_HPP_

#include <vector>
#include <string>
#include <dlpack/dlpack.h>
#include <tvm/runtime/packed_func.h>
#include "bmiva_type.hpp"

using std::string;
using std::vector;

typedef struct {
  DLTensor                 *weight_;
  DLTensor                 *input_;
  DLTensor                 *output_;
  BmivaTensor              input_tensor;
  BmivaTensor              output_tensor;
  string                   model_path;
  string                   weight_path;
  tvm::runtime::Module     mod;
  tvm::runtime::PackedFunc f;
} bmiva_tvm_t;

class BmivaTVM {
 public:
  explicit BmivaTVM(const vector<string> &model_path);
  BmivaTVM();
  ~BmivaTVM();
  int RegisterModel(const string &model);
  bmiva_err_t Predict(int         model_index,
                      BmivaTensor *input,
                      BmivaTensor *output);
  int                 nodechip_num_;

 private:
  int                 dtype_code_;
  int                 dtype_bits_;
  int                 dtype_lanes_;
  int                 device_id_;
  uint32_t            count_;
  bmiva_chip_t        chip_type_;
  bmiva_datatype_t    data_type_;
  vector<bmiva_tvm_t> models_;

  void TvmInit();
  bmiva_err_t TvmPrepare(int         model_index,
                         BmivaTensor *input,
                         BmivaTensor *output);
};

extern BmivaTVM *g_tvm_handle;

#endif  // SRC_BMIVATVM_HPP_
