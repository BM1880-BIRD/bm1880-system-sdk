// Copyright 2018 Bitmain Inc.
// License
// Author
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <fstream>
#include <sys/time.h>
#include <unistd.h>
#include "bmiva.hpp"
#include "bmiva_util.hpp"
#include "bmiva_face.hpp"

#define SAVE_IMAGE

using std::vector;
using std::string;
using std::cout;
using std::endl;

#define DET1_MODEL_CFG_FILE "/bmodel/mtcnn/det1.bmodel"
#define DET2_MODEL_CFG_FILE "/bmodel/mtcnn/det2.bmodel"
#define DET3_MODEL_CFG_FILE "/bmodel/mtcnn/det3.bmodel"

vector<cv::Mat> images_all;
vector<string> models;
bmiva_dev_t g_dev;

void
print_current_time(int id) {
  struct timeval tv;
  struct timezone tz;
  gettimeofday(&tv, &tz);
  printf("[%d] time tick: %ld.%ld\n", id, tv.tv_sec, tv.tv_usec);
}

static void
dump_faceinfo(const vector<bmiva_face_info_t> &faceInfo) {
  std::ostringstream msg;
  msg << std::endl;
  for (size_t i = 0; i < faceInfo.size(); i++) {
    float x = faceInfo[i].bbox.x1;
    float y = faceInfo[i].bbox.y1;
    float w = faceInfo[i].bbox.x2 - faceInfo[i].bbox.x1 + 1;
    float h = faceInfo[i].bbox.y2 - faceInfo[i].bbox.y1 + 1;
    msg << "(x y w h): ("
        << x << " "
        << y << " "
        << w << " "
        << h << ")"
        << std::endl;
  }
  cout << msg.str();
}

void *
detect_thread(void *arg) {
  int times = *(int *)arg;
  int id = *((int *)arg + 1);
  cout << "thread " << id << " started..." << endl;
  bmiva_face_handle_t detector;
  bmiva_face_detector_create(&detector, g_dev, models, BMIVA_FD_MTCNN);
  vector< vector<bmiva_face_info_t> > results;
  print_current_time(id);
  bmiva_show_runtime_stat_start(detector);
  for (int loop = 0; loop < times; loop++) {
    results.clear();
    bmiva_face_detector_detect(detector, images_all, results);
  }
  print_current_time(id);
  cout << "NPU Usage: " << bmiva_show_runtime_stat_get(detector) << endl;
  for (size_t m = 0; m < images_all.size(); m++) {
    cv::Mat image = images_all[m].clone();
    vector<bmiva_face_info_t> faceInfo = results[m];
    dump_faceinfo(faceInfo);
    for (size_t i = 0; i < faceInfo.size(); i++) {
      float x = faceInfo[i].bbox.x1;
      float y = faceInfo[i].bbox.y1;
      float w = faceInfo[i].bbox.x2 - faceInfo[i].bbox.x1 +1;
      float h = faceInfo[i].bbox.y2 - faceInfo[i].bbox.y1 +1;
      cv::rectangle(image, cv::Rect(x, y, w, h),
                    cv::Scalar(255, 0, 0), 2);
    }
    for (size_t i = 0; i < faceInfo.size(); i++) {
      bmiva_face_pts_t facePts = faceInfo[i].face_pts;
      for (int j = 0; j < 5; j++)
        cv::circle(image, cv::Point(facePts.x[j], facePts.y[j]),
                   1, cv::Scalar(255, 255, 0), 2);
    }
#ifdef SAVE_IMAGE
    std::ostringstream name;
    name << m << ".jpg";
    cv::imwrite(name.str(), image);
    name.str("");
    name.clear();
#else
    cv::imshow("", image);
    cv::waitKey(1000);
#endif
  }
  bmiva_face_detector_destroy(detector);
  return ((void *)0);
}

int main(int argc, char **argv)
{
  if (argc < 6) {
    cout << "Usage: " << argv[0]
         << " model_dir thread times batch pic1 [pic2]" << endl;
    exit(1);
  }

  string det1(argv[1]), det2(argv[1]), det3(argv[1]);
  det1.append(DET1_MODEL_CFG_FILE);
  det2.append(DET2_MODEL_CFG_FILE);
  det3.append(DET3_MODEL_CFG_FILE);

  if (!FileExist(det1) || !FileExist(det2) || !FileExist(det3)) {
      cout << "Cannot find valid model file." << endl;
      exit(1);
  }

  if (!FileExist(argv[5])) {
      cout << "Cannot find valid pic file: " << argv[4] << endl;
      exit(1);
  }

  models.push_back(det1);
  models.push_back(det2);
  models.push_back(det3);
  int threads = atoi(argv[2]);
  int times = atoi(argv[3]);
  size_t batch = atoi(argv[4]);
  assert(times >= 1);

  cv::Mat image;

  for (int i = 5; i < argc; i++) {
    image = cv::imread(argv[i]);
    images_all.push_back(image);
  }

  for (size_t i = images_all.size(); i < batch; i++)
    images_all.push_back(image);

  bmiva_init(&g_dev);

  pthread_t tid[threads];
  for (int i = 0; i < threads; i++) {
    int *arg = new int[2];
    arg[0] = times;
    arg[1] = i;
    pthread_create(&tid[i], NULL, detect_thread, (void *)arg);
    usleep(300000);
  }

  for (int i = 0; i < threads; i++) {
    pthread_join(tid[i], NULL);
  }
  bmiva_deinit(g_dev);
  return 0;
}
