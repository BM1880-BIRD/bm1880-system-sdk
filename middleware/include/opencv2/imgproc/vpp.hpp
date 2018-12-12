#ifndef OPENCV_VPP_HPP
#define OPENCV_VPP_HPP

#include "opencv2/core/types_c.h"

typedef  char CSC_TYPE;

namespace cv { namespace vpp {

CV_EXPORTS void resize(Mat& src, Mat& dst);
CV_EXPORTS Mat border(Mat& src, int top, int bottom, int left, int right);
CV_EXPORTS void split(Mat& src, Mat* dst);
CV_EXPORTS void split(Mat& src, unsigned long addr0, unsigned long addr1, unsigned long addr2);
CV_EXPORTS void split(Mat& src, Mat& dst);
CV_EXPORTS void crop(Mat& src, std::vector<Rect>& loca, Mat* dst);
CV_EXPORTS void crop(Mat* src, std::vector<Rect>& loca, Mat* dst);
CV_EXPORTS Mat iplImageToMat(IplImage* img);
/*fix by lh*/
CV_EXPORTS Mat iplImageToMat(IplImage* img, int test_flg);
CV_EXPORTS Mat iplImageToMat(IplImage* img, CSC_TYPE csc_type);

/* add by jinf */
CV_EXPORTS Mat cvtColor1682(Mat& src, int srcFmt, int dstFmt);
CV_EXPORTS void cvtColor1682(Mat& src, IplImage* img, int srcFmt, int dstFmt);
}}

/*color space BM1682 supported*/
#define FMT_SRC_I420    (0)
#define FMT_SRC_NV12    (1)
#define FMT_SRC_RGBP    (2)
#define FMT_SRC_BGRA    (3)
#define FMT_SRC_RGBA    (4)
#define FMT_SRC_BGR     (5)
#define FMT_SRC_RGB     (6)

#define FMT_DST_I420    (0)
#define FMT_DST_Y       (1)
#define FMT_DST_RGBP    (2)
#define FMT_DST_BGRA    (3)
#define FMT_DST_RGBA    (4)
#define FMT_DST_BGR     (5)
#define FMT_DST_RGB     (6)

#ifdef VPP_BM1880
/*color space BM1880 supported*/
#define YUV420        0
#define YOnly         1
#define RGB24         2
#define ARGB32        3

/*maximum and minimum image resolution BM1880 supported*/
#define MAX_RESOLUTION_W    (1920)
#define MAX_RESOLUTION_H    (1440)

#define MIN_RESOLUTION_W_LINEAR    (8)    /*linear mode to linear mode*/
#define MIN_RESOLUTION_H_LINEAR    (8)    /*linear mode to linear mode*/
#define MIN_RESOLUTION_W_TILE1    (10)    /*nv12 tile to yuv420p*/
#define MIN_RESOLUTION_H_TILE1    (10)    /*nv12 tile to yuv420p*/
#define MIN_RESOLUTION_W_TILE2    (9)    /*nv12 tile to rgb24, rgb32, rgbp*/
#define MIN_RESOLUTION_H_TILE2    (9)    /*nv12 tile to rgb24, rgb32, rgbp*/
#define MIN_RESOLUTION_W_TILE3    (8)    /*yonly tile to yonly linear*/
#define MIN_RESOLUTION_H_TILE3    (8)    /*yonly tile to yonly linear*/
#else
/*maximum and minimum image resolution BM1682 supported*/
#define MAX_RESOLUTION_W    (3840)
#define MAX_RESOLUTION_H    (2160)
#define MIN_RESOLUTION_W    (16)
#define MIN_RESOLUTION_H    (16)
#endif

enum csc_coe_type {
  Default = 0, YPbPr
};

struct vpp_cmd {
  int src_format;
  int src_stride;
#ifdef VPP_BM1880
  int src_endian;
  int src_endian_a;
  int src_plannar;
#endif
  int src_fd0;
  int src_fd1;
  int src_fd2;
  unsigned long src_addr0;
  unsigned long src_addr1;
  unsigned long src_addr2;
  unsigned short src_axisX;
  unsigned short src_axisY;
  unsigned short src_cropW;
  unsigned short src_cropH;

  int dst_format;
  int dst_stride;
#ifdef VPP_BM1880
  int dst_endian;
  int dst_endian_a;
  int dst_plannar;
#endif
  int dst_fd0;
  int dst_fd1;
  int dst_fd2;
  unsigned long dst_addr0;
  unsigned long dst_addr1;
  unsigned long dst_addr2;
  unsigned short dst_axisX;
  unsigned short dst_axisY;
  unsigned short dst_cropW;
  unsigned short dst_cropH;
#ifdef VPP_BM1880
  int tile_mode;
  int hor_filter_sel;
  int ver_filter_sel;
  int scale_x_init;
  int scale_y_init;
#endif
  int csc_type;
};
struct vpp_batch {
  int num;
  struct vpp_cmd cmd[16];
};

#define VPP_UPDATE_BATCH _IOWR('v', 0x01, unsigned long)
#define VPP_UPDATE_BATCH_VIDEO _IOWR('v', 0x02, unsigned long)
#define VPP_UPDATE_BATCH_SPLIT _IOWR('v', 0x03, unsigned long)
#define VPP_UPDATE_BATCH_NON_CACHE _IOWR('v', 0x04, unsigned long)
#define VPP_UPDATE_BATCH_CROP_TEST _IOWR('v', 0x05, unsigned long)

#endif
