#ifndef ATL_zGetNB_geqrf

/*
 * NB selection for GEQRF: Side='RIGHT', Uplo='UPPER'
 * M : 25,144,288,432,504,576,1152
 * N : 25,144,288,432,504,576,1152
 * NB : 2,24,24,24,72,72,72
 */
#define ATL_zGetNB_geqrf(n_, nb_) \
{ \
   if ((n_) < 84) (nb_) = 2; \
   else if ((n_) < 468) (nb_) = 24; \
   else (nb_) = 72; \
}


#endif    /* end ifndef ATL_zGetNB_geqrf */
