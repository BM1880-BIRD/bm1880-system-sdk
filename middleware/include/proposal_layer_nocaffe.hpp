#ifndef INCLUDE_PROPOSAL_LAYER_NOCAFFE_HPP_
#define INCLUDE_PROPOSAL_LAYER_NOCAFFE_HPP_
#include <string>
#include <vector>
#include <map>

#define NUM_TOP_NUMBER 100

struct param_pro_t {
  int feat_stride;
  int base_size;
  int scales[2];
  float ratios;
  int typeinfo;
};

class ProposalLayer {
 public:
  //std::multimap<float,std::vector<float>,std::greater<float>> _result;
  std::vector<std::pair<float,int>> _order;
  float *_datain;
  //std::vector<std::pair<float,std::vector<float>>> _result;
 private:
  int _feat_stride;
  float _anchor_ratios;
  float _anchor_scales[2];
  size_t _base_size;
  int _im_info[2];
  int _num_anchors;
  bool _trans_anchors_enable;
  float *_mod_anchors_ptr;
  std::string _cfg_key;
  int _pre_nms_topN;
 public:
  ProposalLayer();
  ~ProposalLayer();
  void forward(float *input_score, float *input_bbox, const int *shape);
  void setup(const param_pro_t &param_str);
 private:
  void bbox_transform_inv(float *anchors , float *boxes , int length);
  void clip_boxes(float *datain , int length);
  void filter_boxes(int min_size);
  void Swap(std::vector<std::pair<float,int>> &A, int i, int j);
  void Heapify(std::vector<std::pair<float,int>> &A, int i, int size);
  int BuildHeap(std::vector<std::pair<float,int>> &A, int n);
  void HeapSort(std::vector<std::pair<float,int>> &A, int n);
  void softmax(const char * src, float* dst, int length, float scale);
};

#endif
