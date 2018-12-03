#ifndef BMIVA_FACE_HPP_
#define BMIVA_FACE_HPP_
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "bmiva_type.hpp"

#define MIN_FACE_WIDTH  (16)
#define MIN_FACE_HEIGHT (16)

using std::vector;
using std::string;

// api for face extractor
extern bmiva_err_t
bmiva_face_extractor_create(bmiva_face_handle_t       *handle,
                            bmiva_dev_t          dev,
                            const vector<string> &model_path,
			    bmiva_face_algorithm_t algo = BMIVA_FR_BMFACE);

extern bmiva_err_t
bmiva_face_extractor_predict(bmiva_face_handle_t        handle,
                             const vector<cv::Mat> &images,
                             vector<vector<float>> &results);

extern bmiva_err_t
bmiva_face_extractor_set_attrib(bmiva_face_handle_t handle,
                                int            aid,
                                void           *attr);

extern bmiva_err_t
bmiva_face_extractor_get_attrib(bmiva_face_handle_t handle,
                                int            aid,
                                void           *attr);

extern bmiva_err_t
bmiva_face_extractor_destroy(bmiva_face_handle_t handle); 

// api for face detector
extern bmiva_err_t
bmiva_face_detector_create(bmiva_face_handle_t       *handle,
                           bmiva_dev_t          dev,
                           const vector<string> &model_path,
			   bmiva_face_algorithm_t algo = BMIVA_FD_MTCNN);

extern bmiva_err_t
bmiva_face_detector_detect(bmiva_face_handle_t                    handle,
                           const vector<cv::Mat>             &images,
                           vector<vector<bmiva_face_info_t>> &results);

extern bmiva_err_t
bmiva_face_detector_prepare(bmiva_face_handle_t                    handle,
                            const vector<cv::Mat>             &images,
                            vector<cv::Mat>                   *prepared);

extern bmiva_err_t
bmiva_face_detector_predict(bmiva_face_handle_t                    handle,
                            const vector<cv::Mat>             &images,
                            vector<vector<bmiva_face_info_t>> &results);

extern bmiva_err_t
bmiva_face_detector_set_attrib(bmiva_face_handle_t handle,
                               int            aid,
                               void           *attr);

extern bmiva_err_t
bmiva_face_detector_get_attrib(bmiva_face_handle_t handle,
                               int            aid,
                               void           *attr);

extern bmiva_err_t
bmiva_face_detector_destroy(bmiva_face_handle_t handle);

extern bmiva_err_t face_align(const cv::Mat           &image,
                              cv::Mat                 &aligned,
                              const bmiva_face_info_t &faceInfo,
                              int                     width,
                              int                     height);
extern bmiva_err_t face_align_opt(const cv::Mat           &image,
                                  cv::Mat                 &aligned,
                                  const bmiva_face_info_t &face_info,
                                  int                     width,
                                  int                     height,
                                  const uchar             *p_src);
#if defined(__aarch64__)
extern void* face_align_opt_mem_alloc(const cv::Mat &image, bmiva_dev_t dev);
extern void* face_align_opt_mem_alloc(size_t size , bmiva_dev_t dev);
extern void face_align_opt_dma_trans(const cv::Mat &image, bmiva_dev_t dev, void *hd_ion);
extern bmiva_err_t face_align_opt_mem_free(bmiva_dev_t dev , void *data);
#endif

#endif
