// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef INCLUDE_BMIVA_UTIL_HPP_
#define INCLUDE_BMIVA_UTIL_HPP_

#include <sys/time.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <string>
#include <utility>
#include <map>
#include <unordered_map>
#include "bmiva_type.hpp"

using std::string;
using std::vector;
using std::unordered_map;
using std::map;
using std::pair;

#define MAX_WIDTH_PIC 1920
#define MAX_HIGHT_PIC 1080
#define TRANS_POINTS_NUM 5

//extern uchar g_src_ch0[MAX_HIGHT_PIC][MAX_WIDTH_PIC];
//extern uchar g_src_ch1[MAX_HIGHT_PIC][MAX_WIDTH_PIC];
//extern uchar g_src_ch2[MAX_HIGHT_PIC][MAX_WIDTH_PIC];
//extern uchar *g_p_src;

// file system helpers
extern string GetBasename(char c, const string &src);
extern string GetFilePostfix(char *path);
extern string ReplaceExtname(const string &ext, const string &src);
extern string ReplaceFilename(const string &file, const string &src);
extern void DumpDataToFile(const string &name, char *data, int size);
extern void StringSplit(const string   &src,
                        vector<string> *results,
                        const string   &c);
extern char *LoadDataFromFile(const string &name, int *len);
extern bool FileExist(const string &file_path);
extern bool DirExist(const string &dir_path);
extern int FindPic(const string &target_dir, vector<string> *result);

// face feature helpers
typedef unordered_map<string, vector<vector<float>>> BmivaFeature;
extern void NameLoad(BmivaFeature *name_features,
                     const string &name_file);
extern void CalcNameSim(BmivaFeature                *name_features,
                        const vector<float>         &feature,
                        vector<pair<string, float>> *result);
extern float VectorDot(const vector<float> &vec1,
                       const vector<float> &vec2);
extern float VectorNorm(const vector<float> &vec);
extern float VectorCosine(const vector<float> &vec1,
                          const vector<float> &svec);

#if defined(__aarch64__)
/* Note:
   There will be some loss of precison when using NEON to operate on floating-point.
   Please make sure your algorithm can accept this.
*/
extern "C" float VectorDotNEON(const vector<float> &vec1,
                               const vector<float> &vec2);
extern "C" float VectorNormNEON(const vector<float> &vec);
extern float VectorCosineNEON(const vector<float> &vec1,
                              const vector<float> &svec);
#endif

#ifdef USE_SSE
extern float VectorDotSSE(const float *vec1,
                          const float *vec2,
                          unsigned int size);
extern float VectorDotSSE(const vector<float> &src1,
                          const vector<float> &src2);
extern void NameLoadSSE(BmivaFeature *name_features,
                        const string &name_file);
extern void CalcNameSimSSE(const BmivaFeature          &name_features,
                           const vector<float>         &feature,
                           vector<pair<string, float>> *result);
#endif

extern pair<string, float>
FindName(vector<pair<string, float>> &result, float threshold);

extern pair<string, float>
FindNameDemo(vector<pair<string, float>> *result, float threshold);

extern void set_thread_name(const string &name);
extern int CRC16(unsigned char *Buf, int len);

// timer
class TimeRcorder {
 public:
  TimeRcorder(): elapses_(), tags_() { tags_.reserve(2000);}
  inline uint32_t get_cur_us() {
    struct timeval t0;
    gettimeofday(&t0, NULL);
    return t0.tv_sec * 1000000 + t0.tv_usec;
  }

  void store_timestamp(const string &tag) {
    map<string, uint32_t>::iterator it = elapses_.find(tag);
    if (it == elapses_.end()) {
      elapses_[tag] = get_cur_us();
      tags_.push_back(tag);
    } else {
      elapses_[tag] = get_cur_us() - elapses_[tag];
    }
  }

  void store_timestamp(const string &tag, int index) {
    std::stringstream ss;
    ss << tag << "#" << index;
    store_timestamp(ss.str());
  }

  void store_timestamp(const string &tag, char name, int index) {
    std::stringstream ss;
    ss << tag << "$" << name << "#" << index;
    store_timestamp(ss.str());
  }

  void show() {
    for (size_t i = 0; i < tags_.size(); i++) {
      std::cout << "time<" << i << "> " << tags_[i]
                << ": " << elapses_[tags_[i]] << " us"
                << std::endl;
    }
  }

  void clear() {
    elapses_.clear();
    tags_.clear();
  }

  map<string, uint32_t> elapses_;
  vector<string> tags_;
};

#endif  // INCLUDE_BMIVA_UTIL_HPP_
