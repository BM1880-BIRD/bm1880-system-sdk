// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef SRC_BMIVATRACKER_HPP_
#define SRC_BMIVATRACKER_HPP_

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <dlib/image_processing.h>
#include <dlib/image_io.h>
#include <dlib/dir_nav.h>
#include <dlib/opencv.h>
#include "bmiva_type.hpp"

using std::string;
using std::vector;

class BmivaTracker {
 public:
  BmivaTracker(const cv::Mat &image,
               const vector<bmiva_detect_rect_t> &bboxes);
  BmivaTracker();
  ~BmivaTracker();
  void Update(const cv::Mat &image);
  void Start(const cv::Mat &image,
             const vector<bmiva_detect_rect_t> &bboxes);
  void GetRect(vector<bmiva_detect_rect_t> *bboxes);

 private:
  vector<int> track_id_;
  vector<dlib::correlation_tracker> trackers_;
  void Mat2Array(const cv::Mat &src,
                 dlib::array2d<unsigned char> *dst);
};

// tracker api
#ifdef CONFIG_DLIB
extern bmiva_err_t
bmiva_dlib_tracker_create(bmiva_handle_t *handle);

extern bmiva_err_t
bmiva_dlib_tracker_start(bmiva_handle_t                     handle,
                         const cv::Mat                      &image,
                         const vector<bmiva_detect_rect_t>  &bboxes);

extern bmiva_err_t
bmiva_dlib_tracker_update(bmiva_handle_t                    handle,
                          const cv::Mat                     &image);

extern bmiva_err_t
bmiva_dlib_tracker_get_rect(bmiva_handle_t              handle,
                            vector<bmiva_detect_rect_t> *results);

extern bmiva_err_t
bmiva_dlib_tracker_destroy(bmiva_handle_t handle);
#endif

#endif  // SRC_BMIVATRACKER_HPP_
