#ifndef FACE_HPP_
#define FACE_HPP_
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#if USE_CAFFE
#else
#include "bmcnnctx.h"
#endif /* USE_CAFFE */

typedef void* FaceDetector;
typedef void* FaceExtractor;

typedef struct FaceRect {
  float x1;
  float y1;
  float x2;
  float y2;
  float score;
} FaceRect;

typedef struct FacePts {
  float x[5], y[5];
} FacePts;

typedef struct FaceInfo {
  FaceRect bbox;
  cv::Vec4f regression;
  FacePts facePts;
  double roll;
  double pitch;
  double yaw;
  double distance;
  int imgid;
} FaceInfo;

#if USE_CAFFE
void init_detector(FaceDetector* face_detector, const std::string& model_dir);
#else
void init_detector(FaceDetector* face_detector, bmcnn::bmcnn_ctx_t bmcnn_ctx);
#endif /* USE_CAFFE */
void do_face_detect(FaceDetector face_detector, const std::vector<cv::Mat>& imgs, std::vector<FaceInfo>& faceInfo, int minSize, double* threshold, double factor);
void destroy_detector(FaceDetector face_detector);

#if USE_CAFFE
void init_extractor(FaceExtractor* face_extractor, const std::string& model_file, const std::string& trained_file);
#else
void init_extractor(FaceExtractor* face_extractor, bmcnn::bmcnn_ctx_t bmcnn_ctx);
#endif /* USE_CAFFE */
void do_face_extract(FaceExtractor face_extractor, const std::vector<cv::Mat>& imgs, std::vector<std::vector<float> >& features);
void destroy_extractor(FaceExtractor face_extractor);
#endif
