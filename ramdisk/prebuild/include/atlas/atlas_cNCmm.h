#ifndef CMM_H
   #define CMM_H

   #define ATL_mmMULADD
   #define ATL_mmLAT 7
   #define ATL_mmMU  4
   #define ATL_mmNU  5
   #define ATL_mmKU  1
   #define MB 80
   #define NB 80
   #define KB 80
   #define NBNB 6400
   #define MBNB 6400
   #define MBKB 6400
   #define NBKB 6400
   #define NB2 160
   #define NBNB2 12800

   #define ATL_MulByNB(N_) ((N_) * 80)
   #define ATL_DivByNB(N_) ((N_) / 80)
   #define ATL_MulByNBNB(N_) ((N_) * 6400)

#endif
