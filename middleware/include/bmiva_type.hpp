// Copyright 2018 Bitmain Inc.
// License
// Author
#ifndef INCLUDE_BMIVA_TYPE_HPP_
#define INCLUDE_BMIVA_TYPE_HPP_

#include <string.h>
#include <array>
#include <string>

#define MAX_FACE_PTS 5
#define MAX_NAME_LEN 256

typedef enum {
  BMIVA_CHIP_BM1680 = 0,
  BMIVA_CHIP_BM1682,
  BMIVA_CHIP_INVALID
} bmiva_chip_t;

typedef enum {
  BMIVA_DTYPE_INT8 = 0,
  BMIVA_DTYPE_FLOAT32,
  BMIVA_DTYPE_INVALID
} bmiva_datatype_t;


typedef enum {
  BMIVA_FD_TINYSSH = 0,
  BMIVA_FD_MTCNN   = 1,
  BMIVA_FD_MTCNN_TRANS = 2,
  BMIVA_FR_BMFACE  = 3,
  BMIVA_FACE_ALGO_INVALID
} bmiva_face_algorithm_t;


typedef enum {
  BMIVA_RT_SUCCESS = 0,
  BMIVA_RT_AGAIN,         /* Not ready yet */
  BMIVA_RT_FAILURE,       /* General failure */
  BMIVA_RT_TIMEOUT,       /* Timeout */
  BMIVA_RT_INVALID_PARAM, /* Parameters invalid */
  BMIVA_RT_ALLOC_FAIL,    /* Not enough memory */
  BMIVA_RT_DEV_MEM_FAIL,  /* Data error */
  BMIVA_RT_BUSY,          /* Busy */
  BMIVA_RT_NOFEATURE,     /* Not supported yet */
  BMIVA_RT_UNSUPPORT_DTYPE,
  BMIVA_RT_UNSUPPORT_SHAPE,
  BMIVA_RT_INVALID
} bmiva_err_t;

typedef struct {
  float x1;
  float y1;
  float x2;
  float y2;
  float score;
  int id;
  int track_id;
  char name[MAX_NAME_LEN];
} bmiva_detect_rect_t;

typedef struct {
  std::array<float, MAX_FACE_PTS> x;
  std::array<float, MAX_FACE_PTS> y;
} bmiva_face_pts_t;

typedef struct {
  bmiva_detect_rect_t bbox;
  bmiva_face_pts_t face_pts;
  std::array<float, 4> regression;
} bmiva_face_info_t;

typedef struct {
  bmiva_chip_t chip_type;
  bmiva_datatype_t data_type;
  int nodechip_num;
} bmiva_devinfo_t;

typedef void* bmiva_dev_t;

typedef void* bmiva_handle_t;

typedef struct {
    bmiva_handle_t pHnd;
    bmiva_face_algorithm_t algo;
} bmiva_face_handle;

typedef bmiva_face_handle* bmiva_face_handle_t;

class BmivaTensor {
 public:
  BmivaTensor() {
    dim = 0;
    dtype = BMIVA_DTYPE_FLOAT32;  // TODO(Mengya): support float32 and int8
    dsize = 4;
    size = 0;
    count = 0;
    data = nullptr;
    memset(shape, 0, sizeof(shape));
  }
  BmivaTensor(int ndim, int N, int C, int H, int W) {
    dim = ndim;
    dtype = BMIVA_DTYPE_FLOAT32;
    dsize = 4;
    data = nullptr;
    shape[0] = W;
    shape[1] = H;
    shape[2] = C;
    shape[3] = N;
    count = N * C * H * W;
    size = dsize * count;
  }
  ~BmivaTensor() { }

  int get_channel() {
    return shape[2];
  }

  int get_w() {
    return shape[0];
  }

  int get_h() {
    return shape[1];
  }

  int get_batch() {
    return shape[3];
  }

  void set_data(char *src) {
    data = src;
  }

  char *get_data() {
    return data;
  }

  int get_dim() {
    return dim;
  }

  const int* get_shape() {
    return shape;
  }

  int get_count() {
    return count;
  }

  int get_size() {
    return size;
  }

  int get_type() {
    return dtype;
  }

  bool equal(int N, int C, int H, int W) {
    return (shape[0] == W) &&
           (shape[1] == H) &&
           (shape[2] == C) &&
           (shape[3] == N);
  }

  bool equal(BmivaTensor *target) {
    const int *ts = target->get_shape();
    return (shape[0] == ts[0]) &&
           (shape[1] == ts[1]) &&
           (shape[2] == ts[2]) &&
           (shape[3] == ts[3]);
  }

  void copy(BmivaTensor *src) {
    dim = src->dim;
    memcpy(shape, src->shape, sizeof(shape));
    dtype = src->dtype;
    dsize = src->dsize;
    size = src->size;
    count = src->count;
    data = src->data;
  }

  void re_shape(int ndim, int N, int C, int H, int W) {
    dim = ndim;
    shape[0] = W;
    shape[1] = H;
    shape[2] = C;
    shape[3] = N;
    count = N * C * H * W;
    size = count * dsize;
  }

  void re_shape(int ndim, const int *new_shape) {
    dim = ndim;
    memcpy(shape, new_shape, sizeof(shape));
    count = shape[0] * shape[1] * shape[2] * shape[3];
    size = count * dsize;
  }

  void set_batch(int batch_num) {
    shape[3] = batch_num;
    count = shape[0] * shape[1] * shape[2] * shape[3];
    size = count * dsize;
  }

 private:
  int dim;
  int batch;
  int shape[4];  // shape[0]-[3]: W,H,C,N
  bmiva_datatype_t dtype;
  int dsize;
  int size;
  int count;
  char *data;
};

#endif  // INCLUDE_BMIVA_TYPE_HPP_
