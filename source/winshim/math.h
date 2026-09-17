__declspec(dllimport) double sqrt(double);
static inline double fabs(double x){return x<0?-x:x;}
static inline int isfinite(double x){
    const double max_finite=1.7976931348623157e308;
    return x==x&&x<=max_finite&&x>=-max_finite;
}
