// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef INCLUDE_BMIVA_BASE_HPP_
#define INCLUDE_BMIVA_BASE_HPP_

#include <vector>
#include <string>

#include "bmiva_type.hpp"

using std::string;
using std::vector;

// device api
extern bmiva_err_t bmiva_init(bmiva_dev_t *handle);
extern bmiva_err_t bmiva_deinit(bmiva_dev_t handle);
extern int bmiva_register(bmiva_dev_t handle, const string &model);
extern bmiva_err_t bmiva_predict(bmiva_dev_t    handle,
                                 int            model_index,
                                 BmivaTensor    *input,
                                 BmivaTensor    *output);

extern bmiva_err_t bmiva_predict_no_output(bmiva_dev_t    handle,
                                           int            model_index,
                                           BmivaTensor    *input,
                                           BmivaTensor    *output);
#endif
