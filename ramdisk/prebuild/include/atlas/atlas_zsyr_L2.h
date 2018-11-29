/*
 * This file generated on line 432 of /build/atlas-MRbCZl/atlas-3.10.3/build/atlas-base/../..//tune/blas/ger/r1hgen.c
 */
#ifndef ATLAS_ZSYR1_L2_H
   #define ATLAS_ZSYR1_L2_H

#include "atlas_zr1_L2.h"

#define ATL_s1U_NU 12

#define ATL_s1L_NU 12
#define ATL_MIN_RESTRICTED_M 2
#define ATL_URGERK ATL_zgerk__900002
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
             ATL_zgerk_axpy(M, nr, one, X, 1, Y+(n+n), 1, A+(n+n)*lda, lda);
      } /* end if (FNU) */
      else
         gerk(M, N, X, Y, A, lda);
   } /* end if can call optimized kernel */
   else
      ATL_zgerk_Mlt16(M, N, one, X, 1, Y, 1, A, lda);
}

#define ATL_HER1U_nu(A_, lda_, x_, xt_) \
{ \
   TYPE *aa=(A_); \
   ATL_CINT lda0_ = 0, lda1_ = lda0_+(lda_)+(lda_), lda2_ = lda1_+(lda_)+(lda_), lda3_ = lda2_+(lda_)+(lda_), lda4_ = lda3_+(lda_)+(lda_), lda5_ = lda4_+(lda_)+(lda_), lda6_ = lda5_+(lda_)+(lda_), lda7_ = lda6_+(lda_)+(lda_), lda8_ = lda7_+(lda_)+(lda_), lda9_ = lda8_+(lda_)+(lda_), lda10_ = lda9_+(lda_)+(lda_), lda11_ = lda10_+(lda_)+(lda_); \
   const TYPE x0r=*(x_), x0i=(x_)[1], x1r=(x_)[2], x1i=(x_)[3], x2r=(x_)[4], x2i=(x_)[5], x3r=(x_)[6], x3i=(x_)[7], x4r=(x_)[8], x4i=(x_)[9], x5r=(x_)[10], x5i=(x_)[11], x6r=(x_)[12], x6i=(x_)[13], x7r=(x_)[14], x7i=(x_)[15], x8r=(x_)[16], x8i=(x_)[17], x9r=(x_)[18], x9i=(x_)[19], x10r=(x_)[20], x10i=(x_)[21], x11r=(x_)[22], x11i=(x_)[23]; \
   const TYPE xt0r=*(xt_), xt0i=(xt_)[1], xt1r=(xt_)[2], xt1i=(xt_)[3], xt2r=(xt_)[4], xt2i=(xt_)[5], xt3r=(xt_)[6], xt3i=(xt_)[7], xt4r=(xt_)[8], xt4i=(xt_)[9], xt5r=(xt_)[10], xt5i=(xt_)[11], xt6r=(xt_)[12], xt6i=(xt_)[13], xt7r=(xt_)[14], xt7i=(xt_)[15], xt8r=(xt_)[16], xt8i=(xt_)[17], xt9r=(xt_)[18], xt9i=(xt_)[19], xt10r=(xt_)[20], xt10i=(xt_)[21], xt11r=(xt_)[22], xt11i=(xt_)[23]; \
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
   aa[lda8_+0] += x0r*xt8r-x0i*xt8i; \
   aa[lda8_+1] += x0r*xt8i+x0i*xt8r; \
   aa[lda8_+2] += x1r*xt8r-x1i*xt8i; \
   aa[lda8_+3] += x1r*xt8i+x1i*xt8r; \
   aa[lda8_+4] += x2r*xt8r-x2i*xt8i; \
   aa[lda8_+5] += x2r*xt8i+x2i*xt8r; \
   aa[lda8_+6] += x3r*xt8r-x3i*xt8i; \
   aa[lda8_+7] += x3r*xt8i+x3i*xt8r; \
   aa[lda8_+8] += x4r*xt8r-x4i*xt8i; \
   aa[lda8_+9] += x4r*xt8i+x4i*xt8r; \
   aa[lda8_+10] += x5r*xt8r-x5i*xt8i; \
   aa[lda8_+11] += x5r*xt8i+x5i*xt8r; \
   aa[lda8_+12] += x6r*xt8r-x6i*xt8i; \
   aa[lda8_+13] += x6r*xt8i+x6i*xt8r; \
   aa[lda8_+14] += x7r*xt8r-x7i*xt8i; \
   aa[lda8_+15] += x7r*xt8i+x7i*xt8r; \
   aa[lda8_+16] += x8r*xt8r-x8i*xt8i; \
   aa[lda8_+17] = 0.0; \
   aa[lda9_+0] += x0r*xt9r-x0i*xt9i; \
   aa[lda9_+1] += x0r*xt9i+x0i*xt9r; \
   aa[lda9_+2] += x1r*xt9r-x1i*xt9i; \
   aa[lda9_+3] += x1r*xt9i+x1i*xt9r; \
   aa[lda9_+4] += x2r*xt9r-x2i*xt9i; \
   aa[lda9_+5] += x2r*xt9i+x2i*xt9r; \
   aa[lda9_+6] += x3r*xt9r-x3i*xt9i; \
   aa[lda9_+7] += x3r*xt9i+x3i*xt9r; \
   aa[lda9_+8] += x4r*xt9r-x4i*xt9i; \
   aa[lda9_+9] += x4r*xt9i+x4i*xt9r; \
   aa[lda9_+10] += x5r*xt9r-x5i*xt9i; \
   aa[lda9_+11] += x5r*xt9i+x5i*xt9r; \
   aa[lda9_+12] += x6r*xt9r-x6i*xt9i; \
   aa[lda9_+13] += x6r*xt9i+x6i*xt9r; \
   aa[lda9_+14] += x7r*xt9r-x7i*xt9i; \
   aa[lda9_+15] += x7r*xt9i+x7i*xt9r; \
   aa[lda9_+16] += x8r*xt9r-x8i*xt9i; \
   aa[lda9_+17] += x8r*xt9i+x8i*xt9r; \
   aa[lda9_+18] += x9r*xt9r-x9i*xt9i; \
   aa[lda9_+19] = 0.0; \
   aa[lda10_+0] += x0r*xt10r-x0i*xt10i; \
   aa[lda10_+1] += x0r*xt10i+x0i*xt10r; \
   aa[lda10_+2] += x1r*xt10r-x1i*xt10i; \
   aa[lda10_+3] += x1r*xt10i+x1i*xt10r; \
   aa[lda10_+4] += x2r*xt10r-x2i*xt10i; \
   aa[lda10_+5] += x2r*xt10i+x2i*xt10r; \
   aa[lda10_+6] += x3r*xt10r-x3i*xt10i; \
   aa[lda10_+7] += x3r*xt10i+x3i*xt10r; \
   aa[lda10_+8] += x4r*xt10r-x4i*xt10i; \
   aa[lda10_+9] += x4r*xt10i+x4i*xt10r; \
   aa[lda10_+10] += x5r*xt10r-x5i*xt10i; \
   aa[lda10_+11] += x5r*xt10i+x5i*xt10r; \
   aa[lda10_+12] += x6r*xt10r-x6i*xt10i; \
   aa[lda10_+13] += x6r*xt10i+x6i*xt10r; \
   aa[lda10_+14] += x7r*xt10r-x7i*xt10i; \
   aa[lda10_+15] += x7r*xt10i+x7i*xt10r; \
   aa[lda10_+16] += x8r*xt10r-x8i*xt10i; \
   aa[lda10_+17] += x8r*xt10i+x8i*xt10r; \
   aa[lda10_+18] += x9r*xt10r-x9i*xt10i; \
   aa[lda10_+19] += x9r*xt10i+x9i*xt10r; \
   aa[lda10_+20] += x10r*xt10r-x10i*xt10i; \
   aa[lda10_+21] = 0.0; \
   aa[lda11_+0] += x0r*xt11r-x0i*xt11i; \
   aa[lda11_+1] += x0r*xt11i+x0i*xt11r; \
   aa[lda11_+2] += x1r*xt11r-x1i*xt11i; \
   aa[lda11_+3] += x1r*xt11i+x1i*xt11r; \
   aa[lda11_+4] += x2r*xt11r-x2i*xt11i; \
   aa[lda11_+5] += x2r*xt11i+x2i*xt11r; \
   aa[lda11_+6] += x3r*xt11r-x3i*xt11i; \
   aa[lda11_+7] += x3r*xt11i+x3i*xt11r; \
   aa[lda11_+8] += x4r*xt11r-x4i*xt11i; \
   aa[lda11_+9] += x4r*xt11i+x4i*xt11r; \
   aa[lda11_+10] += x5r*xt11r-x5i*xt11i; \
   aa[lda11_+11] += x5r*xt11i+x5i*xt11r; \
   aa[lda11_+12] += x6r*xt11r-x6i*xt11i; \
   aa[lda11_+13] += x6r*xt11i+x6i*xt11r; \
   aa[lda11_+14] += x7r*xt11r-x7i*xt11i; \
   aa[lda11_+15] += x7r*xt11i+x7i*xt11r; \
   aa[lda11_+16] += x8r*xt11r-x8i*xt11i; \
   aa[lda11_+17] += x8r*xt11i+x8i*xt11r; \
   aa[lda11_+18] += x9r*xt11r-x9i*xt11i; \
   aa[lda11_+19] += x9r*xt11i+x9i*xt11r; \
   aa[lda11_+20] += x10r*xt11r-x10i*xt11i; \
   aa[lda11_+21] += x10r*xt11i+x10i*xt11r; \
   aa[lda11_+22] += x11r*xt11r-x11i*xt11i; \
   aa[lda11_+23] = 0.0; \
}
#define ATL_HER1L_nu(A_, lda_, x_, xt_) \
{ \
   TYPE *aa=(A_); \
   ATL_CINT lda0_ = 0, lda1_ = lda0_+(lda_)+(lda_), lda2_ = lda1_+(lda_)+(lda_), lda3_ = lda2_+(lda_)+(lda_), lda4_ = lda3_+(lda_)+(lda_), lda5_ = lda4_+(lda_)+(lda_), lda6_ = lda5_+(lda_)+(lda_), lda7_ = lda6_+(lda_)+(lda_), lda8_ = lda7_+(lda_)+(lda_), lda9_ = lda8_+(lda_)+(lda_), lda10_ = lda9_+(lda_)+(lda_), lda11_ = lda10_+(lda_)+(lda_); \
   const TYPE x0r=*(x_), x0i=(x_)[1], x1r=(x_)[2], x1i=(x_)[3], x2r=(x_)[4], x2i=(x_)[5], x3r=(x_)[6], x3i=(x_)[7], x4r=(x_)[8], x4i=(x_)[9], x5r=(x_)[10], x5i=(x_)[11], x6r=(x_)[12], x6i=(x_)[13], x7r=(x_)[14], x7i=(x_)[15], x8r=(x_)[16], x8i=(x_)[17], x9r=(x_)[18], x9i=(x_)[19], x10r=(x_)[20], x10i=(x_)[21], x11r=(x_)[22], x11i=(x_)[23]; \
   const TYPE xt0r=*(xt_), xt0i=(xt_)[1], xt1r=(xt_)[2], xt1i=(xt_)[3], xt2r=(xt_)[4], xt2i=(xt_)[5], xt3r=(xt_)[6], xt3i=(xt_)[7], xt4r=(xt_)[8], xt4i=(xt_)[9], xt5r=(xt_)[10], xt5i=(xt_)[11], xt6r=(xt_)[12], xt6i=(xt_)[13], xt7r=(xt_)[14], xt7i=(xt_)[15], xt8r=(xt_)[16], xt8i=(xt_)[17], xt9r=(xt_)[18], xt9i=(xt_)[19], xt10r=(xt_)[20], xt10i=(xt_)[21], xt11r=(xt_)[22], xt11i=(xt_)[23]; \
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
   aa[lda0_+16] += x8r*xt0r-x8i*xt0i; \
   aa[lda0_+17] += x8r*xt0i+x8i*xt0r; \
   aa[lda0_+18] += x9r*xt0r-x9i*xt0i; \
   aa[lda0_+19] += x9r*xt0i+x9i*xt0r; \
   aa[lda0_+20] += x10r*xt0r-x10i*xt0i; \
   aa[lda0_+21] += x10r*xt0i+x10i*xt0r; \
   aa[lda0_+22] += x11r*xt0r-x11i*xt0i; \
   aa[lda0_+23] += x11r*xt0i+x11i*xt0r; \
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
   aa[lda1_+16] += x8r*xt1r-x8i*xt1i; \
   aa[lda1_+17] += x8r*xt1i+x8i*xt1r; \
   aa[lda1_+18] += x9r*xt1r-x9i*xt1i; \
   aa[lda1_+19] += x9r*xt1i+x9i*xt1r; \
   aa[lda1_+20] += x10r*xt1r-x10i*xt1i; \
   aa[lda1_+21] += x10r*xt1i+x10i*xt1r; \
   aa[lda1_+22] += x11r*xt1r-x11i*xt1i; \
   aa[lda1_+23] += x11r*xt1i+x11i*xt1r; \
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
   aa[lda2_+16] += x8r*xt2r-x8i*xt2i; \
   aa[lda2_+17] += x8r*xt2i+x8i*xt2r; \
   aa[lda2_+18] += x9r*xt2r-x9i*xt2i; \
   aa[lda2_+19] += x9r*xt2i+x9i*xt2r; \
   aa[lda2_+20] += x10r*xt2r-x10i*xt2i; \
   aa[lda2_+21] += x10r*xt2i+x10i*xt2r; \
   aa[lda2_+22] += x11r*xt2r-x11i*xt2i; \
   aa[lda2_+23] += x11r*xt2i+x11i*xt2r; \
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
   aa[lda3_+16] += x8r*xt3r-x8i*xt3i; \
   aa[lda3_+17] += x8r*xt3i+x8i*xt3r; \
   aa[lda3_+18] += x9r*xt3r-x9i*xt3i; \
   aa[lda3_+19] += x9r*xt3i+x9i*xt3r; \
   aa[lda3_+20] += x10r*xt3r-x10i*xt3i; \
   aa[lda3_+21] += x10r*xt3i+x10i*xt3r; \
   aa[lda3_+22] += x11r*xt3r-x11i*xt3i; \
   aa[lda3_+23] += x11r*xt3i+x11i*xt3r; \
   aa[lda4_+8] += x4r*xt4r-x4i*xt4i; \
   aa[lda4_+9] = 0.0; \
   aa[lda4_+10] += x5r*xt4r-x5i*xt4i; \
   aa[lda4_+11] += x5r*xt4i+x5i*xt4r; \
   aa[lda4_+12] += x6r*xt4r-x6i*xt4i; \
   aa[lda4_+13] += x6r*xt4i+x6i*xt4r; \
   aa[lda4_+14] += x7r*xt4r-x7i*xt4i; \
   aa[lda4_+15] += x7r*xt4i+x7i*xt4r; \
   aa[lda4_+16] += x8r*xt4r-x8i*xt4i; \
   aa[lda4_+17] += x8r*xt4i+x8i*xt4r; \
   aa[lda4_+18] += x9r*xt4r-x9i*xt4i; \
   aa[lda4_+19] += x9r*xt4i+x9i*xt4r; \
   aa[lda4_+20] += x10r*xt4r-x10i*xt4i; \
   aa[lda4_+21] += x10r*xt4i+x10i*xt4r; \
   aa[lda4_+22] += x11r*xt4r-x11i*xt4i; \
   aa[lda4_+23] += x11r*xt4i+x11i*xt4r; \
   aa[lda5_+10] += x5r*xt5r-x5i*xt5i; \
   aa[lda5_+11] = 0.0; \
   aa[lda5_+12] += x6r*xt5r-x6i*xt5i; \
   aa[lda5_+13] += x6r*xt5i+x6i*xt5r; \
   aa[lda5_+14] += x7r*xt5r-x7i*xt5i; \
   aa[lda5_+15] += x7r*xt5i+x7i*xt5r; \
   aa[lda5_+16] += x8r*xt5r-x8i*xt5i; \
   aa[lda5_+17] += x8r*xt5i+x8i*xt5r; \
   aa[lda5_+18] += x9r*xt5r-x9i*xt5i; \
   aa[lda5_+19] += x9r*xt5i+x9i*xt5r; \
   aa[lda5_+20] += x10r*xt5r-x10i*xt5i; \
   aa[lda5_+21] += x10r*xt5i+x10i*xt5r; \
   aa[lda5_+22] += x11r*xt5r-x11i*xt5i; \
   aa[lda5_+23] += x11r*xt5i+x11i*xt5r; \
   aa[lda6_+12] += x6r*xt6r-x6i*xt6i; \
   aa[lda6_+13] = 0.0; \
   aa[lda6_+14] += x7r*xt6r-x7i*xt6i; \
   aa[lda6_+15] += x7r*xt6i+x7i*xt6r; \
   aa[lda6_+16] += x8r*xt6r-x8i*xt6i; \
   aa[lda6_+17] += x8r*xt6i+x8i*xt6r; \
   aa[lda6_+18] += x9r*xt6r-x9i*xt6i; \
   aa[lda6_+19] += x9r*xt6i+x9i*xt6r; \
   aa[lda6_+20] += x10r*xt6r-x10i*xt6i; \
   aa[lda6_+21] += x10r*xt6i+x10i*xt6r; \
   aa[lda6_+22] += x11r*xt6r-x11i*xt6i; \
   aa[lda6_+23] += x11r*xt6i+x11i*xt6r; \
   aa[lda7_+14] += x7r*xt7r-x7i*xt7i; \
   aa[lda7_+15] = 0.0; \
   aa[lda7_+16] += x8r*xt7r-x8i*xt7i; \
   aa[lda7_+17] += x8r*xt7i+x8i*xt7r; \
   aa[lda7_+18] += x9r*xt7r-x9i*xt7i; \
   aa[lda7_+19] += x9r*xt7i+x9i*xt7r; \
   aa[lda7_+20] += x10r*xt7r-x10i*xt7i; \
   aa[lda7_+21] += x10r*xt7i+x10i*xt7r; \
   aa[lda7_+22] += x11r*xt7r-x11i*xt7i; \
   aa[lda7_+23] += x11r*xt7i+x11i*xt7r; \
   aa[lda8_+16] += x8r*xt8r-x8i*xt8i; \
   aa[lda8_+17] = 0.0; \
   aa[lda8_+18] += x9r*xt8r-x9i*xt8i; \
   aa[lda8_+19] += x9r*xt8i+x9i*xt8r; \
   aa[lda8_+20] += x10r*xt8r-x10i*xt8i; \
   aa[lda8_+21] += x10r*xt8i+x10i*xt8r; \
   aa[lda8_+22] += x11r*xt8r-x11i*xt8i; \
   aa[lda8_+23] += x11r*xt8i+x11i*xt8r; \
   aa[lda9_+18] += x9r*xt9r-x9i*xt9i; \
   aa[lda9_+19] = 0.0; \
   aa[lda9_+20] += x10r*xt9r-x10i*xt9i; \
   aa[lda9_+21] += x10r*xt9i+x10i*xt9r; \
   aa[lda9_+22] += x11r*xt9r-x11i*xt9i; \
   aa[lda9_+23] += x11r*xt9i+x11i*xt9r; \
   aa[lda10_+20] += x10r*xt10r-x10i*xt10i; \
   aa[lda10_+21] = 0.0; \
   aa[lda10_+22] += x11r*xt10r-x11i*xt10i; \
   aa[lda10_+23] += x11r*xt10i+x11i*xt10r; \
   aa[lda11_+22] += x11r*xt11r-x11i*xt11i; \
   aa[lda11_+23] = 0.0; \
}

#endif
