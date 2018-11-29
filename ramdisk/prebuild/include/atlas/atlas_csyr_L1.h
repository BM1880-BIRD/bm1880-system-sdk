/*
 * This file generated on line 432 of /build/atlas-MRbCZl/atlas-3.10.3/build/atlas-base/../..//tune/blas/ger/r1hgen.c
 */
#ifndef ATLAS_CSYR1_L1_H
   #define ATLAS_CSYR1_L1_H

#include "atlas_cr1_L1.h"

#define ATL_s1U_NU 8

#define ATL_s1L_NU 8
#define ATL_MIN_RESTRICTED_M 2
#define ATL_URGERK ATL_cgerk__900003
static void ATL_GENGERK(ATL_CINT M, ATL_CINT N, const TYPE *X,
                        const TYPE *Y, TYPE *A, ATL_CINT lda)
{
   int nu, minM, minN, i, FNU, aX, aX2A, aY;
   ATL_INT CEL;
   ATL_r1kern_t gerk;
   const TYPE one[2] = {ATL_rone, ATL_rzero};
   gerk = ATL_GetR1Kern(M, N, A, lda, &i, &nu, &minM, &minN,
                        &aX, &aX2A, &aY, &FNU, &CEL);
   if (aX2A)
      aX = ((size_t)A) % ATL_Cachelen == ((size_t)X) % ATL_Cachelen;
   else
      aX = (aX) ? (((size_t)X)/aX)*aX == (size_t)X : 1;
   aY = (aY) ? (((size_t)Y)/aY)*aY == (size_t)Y : 1;
   if (M >= minM && N >= minN && aX && aY)
   {
      if (FNU)
      {
          ATL_CINT n = (N/nu)*nu, nr=N-n;
          gerk(M, n, X, Y, A, lda);
          if (nr)
             ATL_cgerk_axpy(M, nr, one, X, 1, Y+(n+n), 1, A+(n+n)*lda, lda);
      } /* end if (FNU) */
      else
         gerk(M, N, X, Y, A, lda);
   } /* end if can call optimized kernel */
   else
      ATL_cgerk_Mlt16(M, N, one, X, 1, Y, 1, A, lda);
}

#define ATL_HER1U_nu(A_, lda_, x_, xt_) \
{ \
   TYPE *aa=(A_); \
   ATL_CINT lda0_ = 0, lda1_ = lda0_+(lda_)+(lda_), lda2_ = lda1_+(lda_)+(lda_), lda3_ = lda2_+(lda_)+(lda_), lda4_ = lda3_+(lda_)+(lda_), lda5_ = lda4_+(lda_)+(lda_), lda6_ = lda5_+(lda_)+(lda_), lda7_ = lda6_+(lda_)+(lda_); \
   const TYPE x0r=*(x_), x0i=(x_)[1], x1r=(x_)[2], x1i=(x_)[3], x2r=(x_)[4], x2i=(x_)[5], x3r=(x_)[6], x3i=(x_)[7], x4r=(x_)[8], x4i=(x_)[9], x5r=(x_)[10], x5i=(x_)[11], x6r=(x_)[12], x6i=(x_)[13], x7r=(x_)[14], x7i=(x_)[15]; \
   const TYPE xt0r=*(xt_), xt0i=(xt_)[1], xt1r=(xt_)[2], xt1i=(xt_)[3], xt2r=(xt_)[4], xt2i=(xt_)[5], xt3r=(xt_)[6], xt3i=(xt_)[7], xt4r=(xt_)[8], xt4i=(xt_)[9], xt5r=(xt_)[10], xt5i=(xt_)[11], xt6r=(xt_)[12], xt6i=(xt_)[13], xt7r=(xt_)[14], xt7i=(xt_)[15]; \
   aa[lda0_+0] += x0r*xt0r-x0i*xt0i; \
   aa[lda0_+1] = 0.0; \
   aa[lda1_+0] += x0r*xt1r-x0i*xt1i; \
   aa[lda1_+1] += x0r*xt1i+x0i*xt1r; \
   aa[lda1_+2] += x1r*xt1r-x1i*xt1i; \
   aa[lda1_+3] = 0.0; \
   aa[lda2_+0] += x0r*xt2r-x0i*xt2i; \
   aa[lda2_+1] += x0r*xt2i+x0i*xt2r; \
   aa[lda2_+2] += x1r*xt2r-x1i*xt2i; \
   aa[lda2_+3] += x1r*xt2i+x1i*xt2r; \
   aa[lda2_+4] += x2r*xt2r-x2i*xt2i; \
   aa[lda2_+5] = 0.0; \
   aa[lda3_+0] += x0r*xt3r-x0i*xt3i; \
   aa[lda3_+1] += x0r*xt3i+x0i*xt3r; \
   aa[lda3_+2] += x1r*xt3r-x1i*xt3i; \
   aa[lda3_+3] += x1r*xt3i+x1i*xt3r; \
   aa[lda3_+4] += x2r*xt3r-x2i*xt3i; \
   aa[lda3_+5] += x2r*xt3i+x2i*xt3r; \
   aa[lda3_+6] += x3r*xt3r-x3i*xt3i; \
   aa[lda3_+7] = 0.0; \
   aa[lda4_+0] += x0r*xt4r-x0i*xt4i; \
   aa[lda4_+1] += x0r*xt4i+x0i*xt4r; \
   aa[lda4_+2] += x1r*xt4r-x1i*xt4i; \
   aa[lda4_+3] += x1r*xt4i+x1i*xt4r; \
   aa[lda4_+4] += x2r*xt4r-x2i*xt4i; \
   aa[lda4_+5] += x2r*xt4i+x2i*xt4r; \
   aa[lda4_+6] += x3r*xt4r-x3i*xt4i; \
   aa[lda4_+7] += x3r*xt4i+x3i*xt4r; \
   aa[lda4_+8] += x4r*xt4r-x4i*xt4i; \
   aa[lda4_+9] = 0.0; \
   aa[lda5_+0] += x0r*xt5r-x0i*xt5i; \
   aa[lda5_+1] += x0r*xt5i+x0i*xt5r; \
   aa[lda5_+2] += x1r*xt5r-x1i*xt5i; \
   aa[lda5_+3] += x1r*xt5i+x1i*xt5r; \
   aa[lda5_+4] += x2r*xt5r-x2i*xt5i; \
   aa[lda5_+5] += x2r*xt5i+x2i*xt5r; \
   aa[lda5_+6] += x3r*xt5r-x3i*xt5i; \
   aa[lda5_+7] += x3r*xt5i+x3i*xt5r; \
   aa[lda5_+8] += x4r*xt5r-x4i*xt5i; \
   aa[lda5_+9] += x4r*xt5i+x4i*xt5r; \
   aa[lda5_+10] += x5r*xt5r-x5i*xt5i; \
   aa[lda5_+11] = 0.0; \
   aa[lda6_+0] += x0r*xt6r-x0i*xt6i; \
   aa[lda6_+1] += x0r*xt6i+x0i*xt6r; \
   aa[lda6_+2] += x1r*xt6r-x1i*xt6i; \
   aa[lda6_+3] += x1r*xt6i+x1i*xt6r; \
   aa[lda6_+4] += x2r*xt6r-x2i*xt6i; \
   aa[lda6_+5] += x2r*xt6i+x2i*xt6r; \
   aa[lda6_+6] += x3r*xt6r-x3i*xt6i; \
   aa[lda6_+7] += x3r*xt6i+x3i*xt6r; \
   aa[lda6_+8] += x4r*xt6r-x4i*xt6i; \
   aa[lda6_+9] += x4r*xt6i+x4i*xt6r; \
   aa[lda6_+10] += x5r*xt6r-x5i*xt6i; \
   aa[lda6_+11] += x5r*xt6i+x5i*xt6r; \
   aa[lda6_+12] += x6r*xt6r-x6i*xt6i; \
   aa[lda6_+13] = 0.0; \
   aa[lda7_+0] += x0r*xt7r-x0i*xt7i; \
   aa[lda7_+1] += x0r*xt7i+x0i*xt7r; \
   aa[lda7_+2] += x1r*xt7r-x1i*xt7i; \
   aa[lda7_+3] += x1r*xt7i+x1i*xt7r; \
   aa[lda7_+4] += x2r*xt7r-x2i*xt7i; \
   aa[lda7_+5] += x2r*xt7i+x2i*xt7r; \
   aa[lda7_+6] += x3r*xt7r-x3i*xt7i; \
   aa[lda7_+7] += x3r*xt7i+x3i*xt7r; \
   aa[lda7_+8] += x4r*xt7r-x4i*xt7i; \
   aa[lda7_+9] += x4r*xt7i+x4i*xt7r; \
   aa[lda7_+10] += x5r*xt7r-x5i*xt7i; \
   aa[lda7_+11] += x5r*xt7i+x5i*xt7r; \
   aa[lda7_+12] += x6r*xt7r-x6i*xt7i; \
   aa[lda7_+13] += x6r*xt7i+x6i*xt7r; \
   aa[lda7_+14] += x7r*xt7r-x7i*xt7i; \
   aa[lda7_+15] = 0.0; \
}
#define ATL_HER1L_nu(A_, lda_, x_, xt_) \
{ \
   TYPE *aa=(A_); \
   ATL_CINT lda0_ = 0, lda1_ = lda0_+(lda_)+(lda_), lda2_ = lda1_+(lda_)+(lda_), lda3_ = lda2_+(lda_)+(lda_), lda4_ = lda3_+(lda_)+(lda_), lda5_ = lda4_+(lda_)+(lda_), lda6_ = lda5_+(lda_)+(lda_), lda7_ = lda6_+(lda_)+(lda_); \
   const TYPE x0r=*(x_), x0i=(x_)[1], x1r=(x_)[2], x1i=(x_)[3], x2r=(x_)[4], x2i=(x_)[5], x3r=(x_)[6], x3i=(x_)[7], x4r=(x_)[8], x4i=(x_)[9], x5r=(x_)[10], x5i=(x_)[11], x6r=(x_)[12], x6i=(x_)[13], x7r=(x_)[14], x7i=(x_)[15]; \
   const TYPE xt0r=*(xt_), xt0i=(xt_)[1], xt1r=(xt_)[2], xt1i=(xt_)[3], xt2r=(xt_)[4], xt2i=(xt_)[5], xt3r=(xt_)[6], xt3i=(xt_)[7], xt4r=(xt_)[8], xt4i=(xt_)[9], xt5r=(xt_)[10], xt5i=(xt_)[11], xt6r=(xt_)[12], xt6i=(xt_)[13], xt7r=(xt_)[14], xt7i=(xt_)[15]; \
   aa[lda0_+0] += x0r*xt0r-x0i*xt0i; \
   aa[lda0_+1] = 0.0; \
   aa[lda0_+2] += x1r*xt0r-x1i*xt0i; \
   aa[lda0_+3] += x1r*xt0i+x1i*xt0r; \
   aa[lda0_+4] += x2r*xt0r-x2i*xt0i; \
   aa[lda0_+5] += x2r*xt0i+x2i*xt0r; \
   aa[lda0_+6] += x3r*xt0r-x3i*xt0i; \
   aa[lda0_+7] += x3r*xt0i+x3i*xt0r; \
   aa[lda0_+8] += x4r*xt0r-x4i*xt0i; \
   aa[lda0_+9] += x4r*xt0i+x4i*xt0r; \
   aa[lda0_+10] += x5r*xt0r-x5i*xt0i; \
   aa[lda0_+11] += x5r*xt0i+x5i*xt0r; \
   aa[lda0_+12] += x6r*xt0r-x6i*xt0i; \
   aa[lda0_+13] += x6r*xt0i+x6i*xt0r; \
   aa[lda0_+14] += x7r*xt0r-x7i*xt0i; \
   aa[lda0_+15] += x7r*xt0i+x7i*xt0r; \
   aa[lda1_+2] += x1r*xt1r-x1i*xt1i; \
   aa[lda1_+3] = 0.0; \
   aa[lda1_+4] += x2r*xt1r-x2i*xt1i; \
   aa[lda1_+5] += x2r*xt1i+x2i*xt1r; \
   aa[lda1_+6] += x3r*xt1r-x3i*xt1i; \
   aa[lda1_+7] += x3r*xt1i+x3i*xt1r; \
   aa[lda1_+8] += x4r*xt1r-x4i*xt1i; \
   aa[lda1_+9] += x4r*xt1i+x4i*xt1r; \
   aa[lda1_+10] += x5r*xt1r-x5i*xt1i; \
   aa[lda1_+11] += x5r*xt1i+x5i*xt1r; \
   aa[lda1_+12] += x6r*xt1r-x6i*xt1i; \
   aa[lda1_+13] += x6r*xt1i+x6i*xt1r; \
   aa[lda1_+14] += x7r*xt1r-x7i*xt1i; \
   aa[lda1_+15] += x7r*xt1i+x7i*xt1r; \
   aa[lda2_+4] += x2r*xt2r-x2i*xt2i; \
   aa[lda2_+5] = 0.0; \
   aa[lda2_+6] += x3r*xt2r-x3i*xt2i; \
   aa[lda2_+7] += x3r*xt2i+x3i*xt2r; \
   aa[lda2_+8] += x4r*xt2r-x4i*xt2i; \
   aa[lda2_+9] += x4r*xt2i+x4i*xt2r; \
   aa[lda2_+10] += x5r*xt2r-x5i*xt2i; \
   aa[lda2_+11] += x5r*xt2i+x5i*xt2r; \
   aa[lda2_+12] += x6r*xt2r-x6i*xt2i; \
   aa[lda2_+13] += x6r*xt2i+x6i*xt2r; \
   aa[lda2_+14] += x7r*xt2r-x7i*xt2i; \
   aa[lda2_+15] += x7r*xt2i+x7i*xt2r; \
   aa[lda3_+6] += x3r*xt3r-x3i*xt3i; \
   aa[lda3_+7] = 0.0; \
   aa[lda3_+8] += x4r*xt3r-x4i*xt3i; \
   aa[lda3_+9] += x4r*xt3i+x4i*xt3r; \
   aa[lda3_+10] += x5r*xt3r-x5i*xt3i; \
   aa[lda3_+11] += x5r*xt3i+x5i*xt3r; \
   aa[lda3_+12] += x6r*xt3r-x6i*xt3i; \
   aa[lda3_+13] += x6r*xt3i+x6i*xt3r; \
   aa[lda3_+14] += x7r*xt3r-x7i*xt3i; \
   aa[lda3_+15] += x7r*xt3i+x7i*xt3r; \
   aa[lda4_+8] += x4r*xt4r-x4i*xt4i; \
   aa[lda4_+9] = 0.0; \
   aa[lda4_+10] += x5r*xt4r-x5i*xt4i; \
   aa[lda4_+11] += x5r*xt4i+x5i*xt4r; \
   aa[lda4_+12] += x6r*xt4r-x6i*xt4i; \
   aa[lda4_+13] += x6r*xt4i+x6i*xt4r; \
   aa[lda4_+14] += x7r*xt4r-x7i*xt4i; \
   aa[lda4_+15] += x7r*xt4i+x7i*xt4r; \
   aa[lda5_+10] += x5r*xt5r-x5i*xt5i; \
   aa[lda5_+11] = 0.0; \
   aa[lda5_+12] += x6r*xt5r-x6i*xt5i; \
   aa[lda5_+13] += x6r*xt5i+x6i*xt5r; \
   aa[lda5_+14] += x7r*xt5r-x7i*xt5i; \
   aa[lda5_+15] += x7r*xt5i+x7i*xt5r; \
   aa[lda6_+12] += x6r*xt6r-x6i*xt6i; \
   aa[lda6_+13] = 0.0; \
   aa[lda6_+14] += x7r*xt6r-x7i*xt6i; \
   aa[lda6_+15] += x7r*xt6i+x7i*xt6r; \
   aa[lda7_+14] += x7r*xt7r-x7i*xt7i; \
   aa[lda7_+15] = 0.0; \
}

#endif
