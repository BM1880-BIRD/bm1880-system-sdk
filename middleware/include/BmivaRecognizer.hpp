// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef SRC_FACE_BMIVARECOGNIZER_HPP_
#define SRC_FACE_BMIVARECOGNIZER_HPP_

#include <opencv2/opencv.hpp>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "bmiva_util.hpp"
#include "BmivaDevice.hpp"

using std::vector;
using std::string;

class BmivaRecognizer {
 public:
  BmivaDevice    *dev_;
  TimeRcorder timers_;
  BmivaRecognizer(BmivaDevice          *dev,
                  const vector<string> &model_path,
		  bmiva_datatype_t DataType = BMIVA_DTYPE_FLOAT32);
  ~BmivaRecognizer();

  void Predict(const vector<cv::Mat> &images,
               vector<vector<float>> &features);

  void Prepare(const vector<cv::Mat> &images);

 private:
  int         m_ic;
  int         m_iw;
  int         m_ih;
  int         m_oc;
  int         m_index;
  int         m_lastbatch;
  int         profiling_on_;
  char        *m_in_buffer;
  char        *m_out_buffer;
  float       *m_out_float_buffer;
  BmivaTensor m_input_tensor;
  BmivaTensor m_output_tensor;
  bmiva_datatype_t    NpuDataType;
  pthread_mutex_t predict_lock_;
};

#endif  // SRC_FACE_BMIVARECOGNIZER_HPP_
