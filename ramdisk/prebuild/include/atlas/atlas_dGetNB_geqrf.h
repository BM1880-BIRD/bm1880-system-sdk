#ifndef ATL_dGetNB_geqrf

/*
 * NB selection for GEQRF: Side='RIGHT', Uplo='UPPER'
 * M : 25,216,432,648,864,1800
 * N : 25,216,432,648,864,1800
 * NB : 1,24,24,72,72,72
 */
#define ATL_dGetNB_geqrf(n_, nb_) \
{ \
   if ((n_) < 120) (nb_) = 1; \
   else if ((n_) < 540) (nb_) = 24; \
   else (nb_) = 72; \
}


#endif    /* end ifndef ATL_dGetNB_geqrf */
