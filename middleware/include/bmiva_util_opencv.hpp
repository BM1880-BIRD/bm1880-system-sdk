#ifndef BMIVA_UTIL_OPENCV_HPP_
#define BMIVA_UTIL_OPENCV_HPP_

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

// opencv related calls
extern void remove_light(cv::Mat *image);
extern void DrawName(const bmiva_face_info_t       &faceInfo,
                     cv::Mat                       &image,
                     std::string                   name_string);
#endif
