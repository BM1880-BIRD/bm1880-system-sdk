// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef SRC_FACE_BMIVATINYSSH_HPP_
#define SRC_FACE_BMIVATINYSSH_HPP_

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <map>
#include "BmivaDevice.hpp"
#include "bmiva_util.hpp"
#include "bmiva_internal.hpp"
#if !USE_PYTHON_PROPOSAL
#include "proposal_layer_nocaffe.hpp"
#endif
using std::vector;
using std::string;

#define USE_PYTHON_PROPOSAL 0

#ifdef CONFIG_USE_PYTHON
#else
typedef void (*INIT)(void**);
typedef void (*DESTROY)(void*);
typedef void (*DO_FUNC)(void*, float*[3], float*[3], std::vector<bmiva_face_info_t> &);
#endif
class BmivaTinySSH {
 public:
  BmivaDevice *dev_;
  TimeRcorder timers_;
  BmivaTinySSH(BmivaDevice          *dev,
               const vector<string> &model_path, bmiva_datatype_t DataType = BMIVA_DTYPE_FLOAT32);
  ~BmivaTinySSH();
  void Detect(const vector<cv::Mat>             &images,
              vector<vector<bmiva_face_info_t>> &results);
 private:
  bmiva_datatype_t NpuDataType;
  
  float ResizeImage(const cv::Mat &src, cv::Mat *dst, int update);
  void ClassifyFace_MulImage(vector<bmiva_face_info_t> &regressed_rects,
                             const vector<cv::Mat>     &prepared,
                             double                    thresh);
#if !USE_PYTHON_PROPOSAL
  void handle_proposal(vector<bmiva_face_info_t> &boxes);
#endif
  vector<bmiva_face_info_t>
  NonMaximumSuppression(vector<bmiva_face_info_t> &BBoxes,
                        float                     thresh,
                        char                      method,
                        int                       imgsize);
  void Bbox2Square(vector<bmiva_face_info_t> &bboxes);
  void Padding(vector<bmiva_face_info_t> &faces,
               vector<bmiva_face_info_t> &pad_faces,
               int img_w, int img_h);
  void WrapInputLayer(vector<cv::Mat> *input_channels,
                      BmivaTensor     *input,
                      const uint32_t  off,
                      const int       height,
                      const int       width);
  void DetectPrepare(const vector<cv::Mat> &image,
                     vector<cv::Mat>       *prepared);
  void AllocTensor(int                           batch,
                   const vector<string>          &shapes,
                   vector<BmivaTensor>           *inputs,
                   vector<BmivaTensor>           *outputs,
                   vector< vector<BmivaTensor> > *posts);
  int SelectTensor(int                 N,
                   int                 C,
                   int                 H,
                   int                 W,
                   BmivaTensor         *input_tensor,
                   BmivaTensor         *output_tensor,
                   vector<BmivaTensor> *post_tensors);

  vector<cv::Mat>             crop_img_;
  int                         model_index_[2];
  int                         profiling_on_;
  double                      threshold_;
  char                        *input_buffer_;
  char                        *output_buffer_;
  float 		      *postProbsBuf;
  float 		      *postPredsBuf;
  float                       resize_ratio_[64];
  int                         cur_batch_;
  int                         last_batch_;
  int                         hs_;
  int                         ws_;
  int                         o_size_;
  vector<BmivaTensor>         ssh_input_tensor_;
  vector<BmivaTensor>         ssh_output_tensor_;
  vector<vector<BmivaTensor>> ssh_post_tensor_;
  vector<BmivaTensor>         onet_input_tensor_;
  vector<BmivaTensor>         onet_output_tensor_;
  vector<vector<BmivaTensor>> onet_post_tensor_;

  vector<bmiva_face_info_t>   condidate_rects_;
  vector<bmiva_face_info_t>   regressed_rects_;
  vector<bmiva_face_info_t>   regressed_pading_;
  vector<bmiva_face_info_t>   total_boxes_;
  pthread_mutex_t             detect_lock_;
#if USE_PYTHON_PROPOSAL
  void                        *proposal_layer_;
#else
  vector<ProposalLayer *>     proposal_layer_v_;
#endif
  void do_proposal(vector<BmivaTensor> &post_tensors,
                   vector<bmiva_face_info_t> &boxes);
#ifdef CONFIG_USE_PYTHON
#else
  void                        *proposal_handle_;
  INIT                        proposal_layer_init_;
  DESTROY                     proposal_layer_destroy_;
  DO_FUNC                     do_proposal_layer_;
#endif
};

#endif  // SRC_FACE_BMIVATINYSSH_HPP_
