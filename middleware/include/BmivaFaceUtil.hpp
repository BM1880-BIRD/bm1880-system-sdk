// Copyright 2018 Bitmain Inc.
// License
// Author xfkuang

#ifndef __BMIVA_FACE_UTIL_HPP_

#define __BMIVA_FACE_UTIL_HPP_

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

using namespace std;

void Dequantize(const char * src, float* dst, int length, float scale);


void SoftMax(const char * src, float * dst, int num, int channel, int size, float scale);

void AverageAndSplitToF32(cv::Mat &prepared, vector<cv::Mat> &channels, float MeanR, float MeanG, float MeanB, int width, int height);

void NormalizeAndSplitToU8(cv::Mat &prepared, vector<cv::Mat> &channels, float MeanR, float alphaR, float MeanG,  float alphaG, 
			float MeanB, float alphaB, int width, int height);

void NormalizeAndSplitToF32(cv::Mat &prepared, vector<cv::Mat> &channels, float MeanR, float alphaR, float MeanG,  float alphaG, 
			float MeanB, float alphaB, int width, int height);
#endif
