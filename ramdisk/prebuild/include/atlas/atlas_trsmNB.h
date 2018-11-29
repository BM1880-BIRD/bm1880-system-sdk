#ifndef ATLAS_TRSMNB_H
   #define ATLAS_TRSMNB_H

   #ifdef SREAL
      #define TRSM_NB 72
   #elif defined(DREAL)
      #define TRSM_NB 72
   #elif defined(SCPLX)
      #define TRSM_NB 72
   #elif defined(DCPLX)
      #define TRSM_NB 72
   #endif

#endif
