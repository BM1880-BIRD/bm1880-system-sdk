// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef SRC_FACE_BMIVAMTCNNTRANS_HPP_
#define SRC_FACE_BMIVAMTCNNTRANS_HPP_

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include "BmivaDevice.hpp"
#include "bmiva_util.hpp"

using std::vector;
using std::string;

class BmivaMTCNN_Trans {
 public:
  BmivaDevice    *dev_;
  TimeRcorder timers_;
  BmivaMTCNN_Trans(BmivaDevice          *dev,
             const vector<string> &model_path,
	     bmiva_datatype_t DataType = BMIVA_DTYPE_INT8);
  ~BmivaMTCNN_Trans();
  void DetectPrepare(const vector<cv::Mat> &image,
                     vector<cv::Mat>       *prepared);
  bmiva_err_t Detect(const vector<cv::Mat>             &images,
              vector<vector<bmiva_face_info_t>> &results);

 private:
  void Preprocess(const cv::Mat        &img,
                  vector<cv::Mat>      *input_channels);

  void WrapInputLayer(vector<cv::Mat>  *input_channels,
                      BmivaTensor      *input,
                      const uint32_t   off,
                      const int        height,
                      const int        width);

  void GenerateBoundingBox(BmivaTensor *confidence,
                           BmivaTensor *reg,
                           float       scale,
                           float       thresh,
                           int         image_width,
                           int         image_height,
                           int         image_id);

  void ClassifyFace_MulImage(vector<bmiva_face_info_t> &regressed_rects,
                             const vector<cv::Mat>     &prepared,
                             double                    thresh,
                             int                       stage);

  vector<bmiva_face_info_t>
  NonMaximumSuppression(vector<bmiva_face_info_t> &bboxes,
                        float                     thresh,
                        char                      method,
                        int                       imgsize);

  void Bbox2Square(vector<bmiva_face_info_t> &bboxes);
  void Padding(int img_w, int img_h);
  void RegressPoint(const vector<bmiva_face_info_t> &faceInfo);

  int SelectTensor(int                  N,
                   int                  C,
                   int                  H,
                   int                  W,
                   BmivaTensor          *input_tensor,
                   BmivaTensor          *output_tensor,
                   vector<BmivaTensor>  *post_tensors);

  void AllocTensor(int                            batch,
                   const vector<string>           &shapes,
                   vector<BmivaTensor>            *inputs,
                   vector<BmivaTensor>            *outputs,
                   vector< vector<BmivaTensor> >  *posts);

  void ReorgBuffer(BmivaTensor                    *output_tensor,
                   vector<BmivaTensor>            *post_tensors);

  vector<bmiva_face_info_t>
  BoxRegress(vector<bmiva_face_info_t> &faceInfo_, int stage);
  float ResizeImage(const cv::Mat &src, cv::Mat *dst, int update);

  // x1,y1,x2,t2 and score
  vector<bmiva_face_info_t>         condidate_rects_;
  vector<bmiva_face_info_t>         regressed_rects_;
  vector<bmiva_face_info_t>         regressed_pading_;
  vector<bmiva_face_info_t>         total_boxes_;

  vector<cv::Mat>             crop_img_;
  bmiva_datatype_t            NpuDataType;
  int                         model_index_[3];
  int                         profiling_on_;
  int                         curr_feature_map_w_;
  int                         curr_feature_map_h_;
  int                         num_channels_;
  double                      threshold_[3];
  double                      factor_;
  int                         min_size_;
  char                        *input_buffer_;
  char                        *output_buffer_;
  float                       *postConfidence;
  float                       resize_ratio_[64];
  int                         cur_input_w_;
  int                         cur_input_h_;
  int                         cur_batch_;
  int                         last_input_w_;
  int                         last_input_h_;
  int                         last_batch_;
  vector<BmivaTensor>         det1_input_tensor_;
  vector<BmivaTensor>         det2_input_tensor_;
  vector<BmivaTensor>         det3_input_tensor_;
  vector<BmivaTensor>         det1_output_tensor_;
  vector<BmivaTensor>         det2_output_tensor_;
  vector<BmivaTensor>         det3_output_tensor_;
  vector<vector<BmivaTensor>> det1_post_tensor_;
  vector<vector<BmivaTensor>> det2_post_tensor_;
  vector<vector<BmivaTensor>> det3_post_tensor_;
  pthread_mutex_t             detect_lock_;
};

#endif  // SRC_FACE_BMIVAMTCNNTRANS_HPP_
