#ifndef APP_COMMON_H
#define APP_COMMON_H


#ifdef CONFIG_BMODEL
#define DET1_MODEL_CFG_FILE_0       "/bmodel/mtcnn/det1_1_3_81_144.bmodel"
#define DET1_MODEL_CFG_FILE_1       "/bmodel/mtcnn/det1_1_3_41_72.bmodel"
#define DET1_MODEL_CFG_FILE_2       "/bmodel/mtcnn/det1_1_3_21_36.bmodel"
#define DET1_MODEL_CFG_FILE_3       "/bmodel/mtcnn/det1_1_3_12_12.bmodel"
#define DET1_MODEL_CFG_FILE       "/bmodel/mtcnn/det1.bmodel"
#define DET2_MODEL_CFG_FILE       "/bmodel/mtcnn/det2.bmodel"
#define DET3_MODEL_CFG_FILE       "/bmodel/mtcnn/det3.bmodel"
#define SSH_MODEL_CFG_FILE        "/bmodel/tiny_ssh.bmodel"
#define ONET_MODEL_CFG_FILE       "/bmodel/det3.bmodel"
#define EXTRACTOR_MODEL_CFG_FILE  "/bmodel/bmface.bmodel"
#define FACE_RECOG_IMG_H          112
#define FACE_RECOG_IMG_W          112
#else
#define DET1_MODEL_CFG_FILE       "/face_detector/bm_face_detector_det1.json"
#define DET2_MODEL_CFG_FILE       "/face_detector/bm_face_detector_det2.json"
#define DET3_MODEL_CFG_FILE       "/face_detector/bm_face_detector_det3.json"
#define SSH_MODEL_CFG_FILE        "/face_tiny_ssh/bmiva_face_tiny_ssh.json"
#define ONET_MODEL_CFG_FILE       "/face_detector/bm_face_detector_det3.json"
#define EXTRACTOR_MODEL_CFG_FILE  "/face_extractor/bmiva_face_extractor.json"
#define FACE_RECOG_IMG_H          112
#define FACE_RECOG_IMG_W          96
#endif // CONFIG_BMODEL

#define FACE_FEATURE_FILE         "/features.txt"

#define DEFAULT_IMG_SIZE          (1024 * 1024 * 4)
#define STAT_INTERVAL             (10)
#define STAT_DISPLAY_INTERVAL     (0)
#define MAX_THREAD_NUM            (8)
#define DROP_THRESH               (16)
#define PIC_DROP_THRESH           (50)
#define MAX_SUPPORTED_BATCH       (8)
#define MAX_SAVED_RESULTS         (1024)
#define TIMER_SUB_USEC            (0)
#define TIMER_ADD_USEC            (50000)
#define VIDEO_CHANNEL             (8)
#define VIDEO_PIC_CHANNEL         (16)
#define DONE_VECTOR_NUM           (MAX_THREAD_NUM + VIDEO_PIC_CHANNEL + 1)
#define MAX_STREAM_BUF            (4)
#define MAX_RECOG_Q_SIZE          (2)
#define MAX_RESPONSE_Q_SIZE       (8)
#define MIN_PIC_SIZE              (128)
#define MAX_PIC_SIZE              (1024 * 1024 * 4)
#define MAX_SUPPORT_IMG_HEIGHT    (2160)
#define MIN_SUPPORT_IMG_HEIGHT    (90)
#define MAX_SUPPORT_IMG_WIDTH     (4096)
#define MIN_SUPPORT_IMG_WIDTH     (120)
#define STREAM_FRAME_COLS         (192)
#define STREAM_FRAME_ROWS         (108)
#define FACTORY_VIDEO_FRAMES      (2000)

#endif
