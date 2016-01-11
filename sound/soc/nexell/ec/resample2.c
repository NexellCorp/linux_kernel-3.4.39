#include "resample.h"
//#include "dsputil.h"

/********************************************************************************************/
// Math Function

#define USE_MATH_H	0
#define MATH_NEON	0

#if USE_MATH_H
//#include <math.h>
#else
#if MATH_NEON
#define __MATH_NEON
#endif
#endif

#define M_PI       3.14159265358979323846
#define M_PI_2     1.57079632679489661923

static float sin_f(float x);
#ifdef __MATH_NEON
static float sin_f_neon(float x);
#endif

static float ceil_f(float x)
{
 	int n;
 	float r;
 	n = (int) x;
 	r = (float) n;
 	r = r + (x > r);
 	return r;
}

static float fabs_f(float x)
{
 	union {
 		int i;
 		float f;
 	} xx;

 	xx.f = x;
 	xx.i = xx.i & 0x7FFFFFFF;
 	return xx.f;
}

#ifdef __MATH_NEON
static float fabs_f_neon(float x)
{
	asm volatile (
 	"fabss	 		s0, s0					\n\t"	//s0 = fabs(s0)
 	);
}
#endif

static float sqrt_f(float x)
{
 	float b, c;
 	int m;
 	union {
 		float 	f;
 		int 	i;
 	} a;

 	//fast invsqrt approx
 	a.f = x;
 	a.i = 0x5F3759DF - (a.i >> 1);		//VRSQRTE
 	c = x * a.f;
 	b = (3.0f - c * a.f) * 0.5;		//VRSQRTS
 	a.f = a.f * b;
 	c = x * a.f;
 	b = (3.0f - c * a.f) * 0.5;
     a.f = a.f * b;


 	//fast inverse approx
 	x = a.f;
 	m = 0x3F800000 - (a.i & 0x7F800000);
 	a.i = a.i + m;
 	a.f = 1.41176471f - 0.47058824f * a.f;
 	a.i = a.i + m;
 	b = 2.0 - a.f * x;
 	a.f = a.f * b;
 	b = 2.0 - a.f * x;
 	a.f = a.f * b;


	return a.f;
}

#ifdef __MATH_NEON
static float sqrt_f_neon(float x)
{
 	asm volatile (

 	//fast invsqrt approx
 	"vmov.f32 		d1, d0					\n\t"	//d1 = d0
 	"vrsqrte.f32 	d0, d0					\n\t"	//d0 = ~ 1.0 / sqrt(d0)
 	"vmul.f32 		d2, d0, d1				\n\t"	//d2 = d0 * d1
 	"vrsqrts.f32 	d3, d2, d0				\n\t"	//d3 = (3 - d0 * d2) / 2
 	"vmul.f32 		d0, d0, d3				\n\t"	//d0 = d0 * d3
 	"vmul.f32 		d2, d0, d1				\n\t"	//d2 = d0 * d1
 	"vrsqrts.f32 	d3, d2, d0				\n\t"	//d4 = (3 - d0 * d3) / 2
 	"vmul.f32 		d0, d0, d3				\n\t"	//d0 = d0 * d3

 	//fast reciporical approximation
 	"vrecpe.f32		d1, d0					\n\t"	//d1 = ~ 1 / d0;
 	"vrecps.f32		d2, d1, d0				\n\t"	//d2 = 2.0 - d1 * d0;
 	"vmul.f32		d1, d1, d2				\n\t"	//d1 = d1 * d2;
 	"vrecps.f32		d2, d1, d0				\n\t"	//d2 = 2.0 - d1 * d0;
 	"vmul.f32		d0, d1, d2				\n\t"	//d0 = d1 * d2;


 	::: "d0", "d1", "d2", "d3"
 	);
}
#endif

static float cos_f(float x)
{
 	return sin_f(x + M_PI_2);
}

#ifdef __MATH_NEON
static float cos_f_neon(float x)
{
 	float xx = x + M_PI_2;
 	return sin_f_neon(xx);
}
#endif

static const float __sinf_rng[2] = {
 	2.0 / M_PI,
 	M_PI / 2.0
 }; //ALIGN(16);


 static const float __sinf_lut[4] = {
 	-0.00018365f,	//p7
 	-0.16664831f,	//p3
 	+0.00830636f,	//p5
 	+0.99999661f,	//p1
 }; //ALIGN(16);

static float sin_f(float x)
{
 	union {
 		float 	f;
 		int 	i;
 	} ax;

 	float r, a, b, xx;
 	int m, n;

 	ax.f = fabs_f(x);

 	//Range Reduction:
 	m = (int) (ax.f * __sinf_rng[0]);
 	ax.f = ax.f - (((float)m) * __sinf_rng[1]);

 	//Test Quadrant
 	n = m & 1;
 	ax.f = ax.f - n * __sinf_rng[1];
 	m = m >> 1;
 	n = n ^ m;
 	m = (x < 0.0);
 	n = n ^ m;
 	n = n << 31;
 	ax.i = ax.i ^ n;

 	//Taylor Polynomial (Estrins)
 	xx = ax.f * ax.f;
 	a = (__sinf_lut[0] * ax.f) * xx + (__sinf_lut[2] * ax.f);
 	b = (__sinf_lut[1] * ax.f) * xx + (__sinf_lut[3] * ax.f);
 	xx = xx * xx;
 	r = b + a * xx;

 	return r;
 }

#ifdef __MATH_NEON
static float sin_f_neon(float x)
{
	asm volatile (

	"vld1.32 		d3, [%0]				\n\t"	//d3 = {invrange, range}
	"vdup.f32 		d0, d0[0]				\n\t"	//d0 = {x, x}
	"vabs.f32 		d1, d0					\n\t"	//d1 = {ax, ax}

	"vmul.f32 		d2, d1, d3[0]			\n\t"	//d2 = d1 * d3[0]
	"vcvt.u32.f32 	d2, d2					\n\t"	//d2 = (int) d2
	"vmov.i32	 	d5, #1					\n\t"	//d5 = 1
	"vcvt.f32.u32 	d4, d2					\n\t"	//d4 = (float) d2
	"vshr.u32 		d7, d2, #1				\n\t"	//d7 = d2 >> 1
	"vmls.f32 		d1, d4, d3[1]			\n\t"	//d1 = d1 - d4 * d3[1]

	"vand.i32 		d5, d2, d5				\n\t"	//d5 = d2 & d5
	"vclt.f32 		d18, d0, #0				\n\t"	//d18 = (d0 < 0.0)
	"vcvt.f32.u32 	d6, d5					\n\t"	//d6 = (float) d5
	"vmls.f32 		d1, d6, d3[1]			\n\t"	//d1 = d1 - d6 * d3[1]
	"veor.i32 		d5, d5, d7				\n\t"	//d5 = d5 ^ d7
	"vmul.f32 		d2, d1, d1				\n\t"	//d2 = d1*d1 = {x^2, x^2}

	"vld1.32 		{d16, d17}, [%1]		\n\t"	//q8 = {p7, p3, p5, p1}
	"veor.i32 		d5, d5, d18				\n\t"	//d5 = d5 ^ d18
	"vshl.i32 		d5, d5, #31				\n\t"	//d5 = d5 << 31
	"veor.i32 		d1, d1, d5				\n\t"	//d1 = d1 ^ d5

 	"vmul.f32 		d3, d2, d2				\n\t"	//d3 = d2*d2 = {x^4, x^4}
 	"vmul.f32 		q0, q8, d1[0]			\n\t"	//q0 = q8 * d1[0] = {p7x, p3x, p5x, p1x}
 	"vmla.f32 		d1, d0, d2[0]			\n\t"	//d1 = d1 + d0*d2 = {p5x + p7x^3, p1x + p3x^3}
 	"vmla.f32 		d1, d3, d1[0]			\n\t"	//d1 = d1 + d3*d0 = {...., p1x + p3x^3 + p5x^5 + p7x^7}


 	"vmov.f32 		s0, s3					\n\t"	//s0 = s3
 	:
 	: "r"(__sinf_rng), "r"(__sinf_lut)
     : "q0", "q1", "q2", "q3", "q8", "q9"
 	);
}
#endif


 /********************************************************************************************/


/* Adding a float, x, to 2^23 will cause the result to be rounded based on
   the fractional part of x, according to the implementation's current rounding
   mode.  2^23 is the smallest float that can be represented using all 23 significant
   digits. */
static const float
TWO23[2]={
  8.3886080000e+06, /* 0x4b000000 */
 -8.3886080000e+06, /* 0xcb000000 */
};

typedef union
{
  float value;
  uint32_t word;
} ieee_float_shape_type;

#define GET_FLOAT_WORD(i,d)				\
do {									\
  ieee_float_shape_type gf_u;			\
  gf_u.value = (d);						\
  (i) = gf_u.word;						\
} while (0)


long int lrintf(float x)
{
  int32_t j0,sx;
  uint32_t i0;
  float t;
  volatile float w;
  long int result;

  GET_FLOAT_WORD(i0,x);

  /* Extract sign bit. */
  sx = (i0 >> 31);

  /* Extract exponent field. */
  j0 = ((i0 & 0x7f800000) >> 23) - 127;
  
  if (j0 < (int)(sizeof (long int) * 8) - 1)
    {
      if (j0 < -1)
        return 0;
      else if (j0 >= 23)
        result = (long int) ((i0 & 0x7fffff) | 0x800000) << (j0 - 23);
      else
        {
          w = TWO23[sx] + x;
          t = w - TWO23[sx];
          GET_FLOAT_WORD (i0, t);
          /* Detect the all-zeros representation of plus and
             minus zero, which fails the calculation below. */
          if ((i0 & ~(1U<< 31)) == 0)
              return 0;
          j0 = ((i0 >> 23) & 0xff) - 0x7f;
          i0 &= 0x7fffff;
          i0 |= 0x800000;
          result = i0 >> (23 - j0);
        }
    }
  else
    {
      return (long int) x;
    }
  return sx ? -result : result;
}


//#define MEMALIGN_HACK


void *av_malloc(unsigned int size)
{
    void *ptr;
#ifdef MEMALIGN_HACK
    int diff;
#endif

    /* lets disallow possible ambiguous cases */
    if(size > INT_MAX)
        return NULL;
    
#ifdef MEMALIGN_HACK
    ptr = MES_malloc(size+16+1);
    MES_ASSERT( CNULL != ptr );
    diff= ((-(int)ptr - 1)&15) + 1;
    //ptr += diff;
    ptr = (void*)((int)ptr + diff);
    ((char*)ptr)[-1]= diff;
#elif defined (HAVE_MEMALIGN) 
    ptr = memalign(16,size);
        
#else
    ptr = MES_malloc(size);
    MES_ASSERT( CNULL != ptr );
#endif
    return ptr;
}

/**
 * av_realloc semantics (same as glibc): if ptr is NULL and size > 0,
 * identical to malloc(size). If size is zero, it is identical to
 * free(ptr) and NULL is returned.  
 */
void *av_realloc(void *ptr, unsigned int size)
{
	void *pret;
	
#ifdef MEMALIGN_HACK
    int diff;
#endif

    /* lets disallow possible ambiguous cases */
    if(size > INT_MAX)
        return NULL;

#ifdef MEMALIGN_HACK
    //FIXME this isnt aligned correctly though it probably isnt needed
    if(!ptr) return av_malloc(size);
    diff= ((char*)ptr)[-1];
    pret = (void*)((int)MES_realloc( (void*)((int)ptr - diff), size + diff) + diff);
#else
    pret = MES_realloc(ptr, size);
#endif

	MES_ASSERT( CNULL != pret );
	return pret;
}

/* NOTE: ptr = NULL is explicetly allowed */
void av_free(void *ptr)
{
	//MES_ASSERT( CNULL != ptr );
    /* XXX: this test should not be needed on most libcs */
    if (ptr)
#ifdef MEMALIGN_HACK
        MES_free( (void*)((int)ptr - ((char*)ptr)[-1]) );
#else
        MES_free(ptr);
#endif
}

void *av_mallocz(unsigned int size)
{
    void *ptr;
    
    ptr = av_malloc(size);
    if (!ptr)
        return NULL;
    memset(ptr, 0, size);
    return ptr;
}

void av_freep(void *arg)
{
    void **ptr= (void**)arg;
    av_free(*ptr);
    *ptr = NULL;
}

static __inline int av_clip(int a, int amin, int amax)
{
    if (a < amin)
        return amin;
    else if (a > amax)
        return amax;
    else
        return a;
}

/**/#ifndef CONFIG_RESAMPLE_HP
/**/#define FILTER_SHIFT 15
/**/
/**/#define FELEM int16_t
/**/#define FELEM2 int32_t
/**/#define FELEML int64_t
/**/#define FELEM_MAX INT16_MAX
/**/#define FELEM_MIN INT16_MIN
/**/#define WINDOW_TYPE 9
/**/#elif !defined(CONFIG_RESAMPLE_AUDIOPHILE_KIDDY_MODE)
/**/#define FILTER_SHIFT 30
/**/
/**/#define FELEM int32_t
/**/#define FELEM2 int64_t
/**/#define FELEML int64_t
/**/#define FELEM_MAX INT32_MAX
/**/#define FELEM_MIN INT32_MIN
/**/#define WINDOW_TYPE 12
/**/#else
/**/#define FILTER_SHIFT 0
/**/
/**/#define FELEM double
/**/#define FELEM2 double
/**/#define FELEML double
/**/#define WINDOW_TYPE 24
/**/#endif
/**/
/**/
/**/typedef struct AVResampleContext{
/**/    FELEM *filter_bank;
/**/    int filter_length;
/**/    int ideal_dst_incr;
/**/    int dst_incr;
/**/    int index;
/**/    int frac;
/**/    int src_incr;
/**/    int compensation_distance;
/**/    int phase_shift;
/**/    int phase_mask;
/**/    int linear;
/**/}AVResampleContext;
/**/
/**/static double bessel(double x){
/**/    double v=1;
/**/    double t=1;
/**/    int i;
/**/
/**/    x= x*x/4;
/**/    for(i=1; i<50; i++){
/**/        t *= x/(i*i);
/**/        v += t;
/**/    }
/**/    return v;
/**/}
/**/

#if USE_MATH_H
/**/void av_build_filter(FELEM *filter, double factor, int tap_count, int phase_count, int scale, int type){
/**/    int ph, i;
/**/    //double x, y, w, tab[tap_count];
/**/    //double x, y, w, *tab;
/**/	double x, y, w, tab[128];
/**/    const int center= (tap_count-1)/2;
/**/
/**/	//tab = MES_MALLOC( tap_count * sizeof(double) );
/**/	//MES_ASSERT( tab != CNULL );
/**/	for( i=0 ; i<128 ; i++ )	tab[i] = 0;
/**/
/**/    /* if upsampling, only need to interpolate, no filter */
/**/    if (factor > 1.0)
/**/        factor = 1.0;
/**/
/**/    for(ph=0;ph<phase_count;ph++) {
/**/        double norm = 0;
/**/        for(i=0;i<tap_count;i++) {
/**/            x = M_PI * ((double)(i - center) - (double)ph / phase_count) * factor;
/**/            if (x == 0) y = 1.0;
/**/            else        y = sin(x) / x;
/**/            switch(type){
/**/            case 0:{
/**/                const float d= -0.5; //first order derivative = -0.5
/**/                x = fabs(((double)(i - center) - (double)ph / phase_count) * factor);
/**/                if(x<1.0) y= 1 - 3*x*x + 2*x*x*x + d*(            -x*x + x*x*x);
/**/                else      y=                       d*(-4 + 8*x - 5*x*x + x*x*x);
/**/                break;}
/**/            case 1:
/**/                w = 2.0*x / (factor*tap_count) + M_PI;
/**/                y *= 0.3635819 - 0.4891775 * cos(w) + 0.1365995 * cos(2*w) - 0.0106411 * cos(3*w);
/**/                break;
/**/            default:
/**/                w = 2.0*x / (factor*tap_count*M_PI);
/**/                y *= bessel(type*sqrt(FFMAX(1-w*w, 0)));
/**/                break;
/**/            }
/**/
/**/            tab[i] = y;
/**/            norm += y;
/**/        }
/**/
/**/        /* normalize so that an uniform color remains the same */
/**/        for(i=0;i<tap_count;i++) {
/**/#ifdef CONFIG_RESAMPLE_AUDIOPHILE_KIDDY_MODE
/**/            filter[ph * tap_count + i] = tab[i] / norm;
/**/#else
/**/            filter[ph * tap_count + i] = av_clip(lrintf(tab[i] * scale / norm), FELEM_MIN, FELEM_MAX);
/**/#endif
/**/        }
/**/    }
/**/
#if 0
/**/    {
/**/#define LEN 1024
/**/        int j,k;
/**/        double sine[LEN + tap_count];
/**/        double filtered[LEN];
/**/        double maxff=-2, minff=2, maxsf=-2, minsf=2;
/**/        for(i=0; i<LEN; i++){
/**/            double ss=0, sf=0, ff=0;
/**/            for(j=0; j<LEN+tap_count; j++)
/**/                sine[j]= cos(i*j*M_PI/LEN);
/**/            for(j=0; j<LEN; j++){
/**/                double sum=0;
/**/                ph=0;
/**/                for(k=0; k<tap_count; k++)
/**/                    sum += filter[ph * tap_count + k] * sine[k+j];
/**/                filtered[j]= sum / (1<<FILTER_SHIFT);
/**/                ss+= sine[j + center] * sine[j + center];
/**/                ff+= filtered[j] * filtered[j];
/**/                sf+= sine[j + center] * filtered[j];
/**/            }
/**/            ss= sqrt(2*ss/LEN);
/**/            ff= sqrt(2*ff/LEN);
/**/            sf= 2*sf/LEN;
/**/            maxff= FFMAX(maxff, ff);
/**/            minff= FFMIN(minff, ff);
/**/            maxsf= FFMAX(maxsf, sf);
/**/            minsf= FFMIN(minsf, sf);
/**/            if(i%11==0){
/**/                av_log(NULL, AV_LOG_ERROR, "i:%4d ss:%f ff:%13.6e-%13.6e sf:%13.6e-%13.6e\n", i, ss, maxff, minff, maxsf, minsf);
/**/                minff=minsf= 2;
/**/                maxff=maxsf= -2;
/**/            }
/**/        }
/**/    }
/**/#endif
/**/
/**/	//MES_FREE( tab );
/**/}
/**/
#else
/**/void av_build_filter(FELEM *filter, double factor, int tap_count, int phase_count, int scale, int type){
/**/    int ph, i;
/**/    //double x, y, w, tab[tap_count];
/**/    //double x, y, w, *tab;
/**/	double x, y, w, tab[128];
/**/    const int center= (tap_count-1)/2;
/**/
/**/	//tab = MES_MALLOC( tap_count * sizeof(double) );
/**/	//MES_ASSERT( tab != CNULL );
/**/	for( i=0 ; i<128 ; i++ )	tab[i] = 0;
/**/
/**/    /* if upsampling, only need to interpolate, no filter */
/**/    if (factor > 1.0)
/**/        factor = 1.0;
/**/
/**/    for(ph=0;ph<phase_count;ph++) {
/**/        double norm = 0;
/**/        for(i=0;i<tap_count;i++) {
/**/            x = M_PI * ((double)(i - center) - (double)ph / phase_count) * factor;
/**/            if (x == 0) y = 1.0;
#if MATH_NEON
/**/            else        y = sin_f_neon(x) / x;
#else
/**/            else        y = sin_f(x) / x;
#endif
/**/            switch(type){
/**/            case 0:{
/**/                const float d= -0.5; //first order derivative = -0.5
#if MATH_NEON
/**/                x = fabs_f_neon(((double)(i - center) - (double)ph / phase_count) * factor);
#else
/**/                x = fabs_f(((double)(i - center) - (double)ph / phase_count) * factor);
#endif
/**/                if(x<1.0) y= 1 - 3*x*x + 2*x*x*x + d*(            -x*x + x*x*x);
/**/                else      y=                       d*(-4 + 8*x - 5*x*x + x*x*x);
/**/                break;}
/**/            case 1:
/**/                w = 2.0*x / (factor*tap_count) + M_PI;
#if MATH_NEON
/**/                y *= 0.3635819 - 0.4891775 * cos_f_neon(w) + 0.1365995 * cos_f_neon(2*w) - 0.0106411 * cos_f_neon(3*w);
#else
/**/                y *= 0.3635819 - 0.4891775 * cos_f(w) + 0.1365995 * cos_f(2*w) - 0.0106411 * cos_f(3*w);
#endif
/**/                break;
/**/            default:
/**/                w = 2.0*x / (factor*tap_count*M_PI);
#if MATH_NEON
/**/                y *= bessel(type*sqrt_f_neon(FFMAX(1-w*w, 0)));
#else
/**/                y *= bessel(type*sqrt_f(FFMAX(1-w*w, 0)));
#endif
/**/                break;
/**/            }
/**/
/**/            tab[i] = y;
/**/            norm += y;
/**/        }
/**/
/**/        /* normalize so that an uniform color remains the same */
/**/        for(i=0;i<tap_count;i++) {
/**/#ifdef CONFIG_RESAMPLE_AUDIOPHILE_KIDDY_MODE
/**/            filter[ph * tap_count + i] = tab[i] / norm;
/**/#else
/**/            filter[ph * tap_count + i] = av_clip(lrintf(tab[i] * scale / norm), FELEM_MIN, FELEM_MAX);
/**/#endif
/**/        }
/**/    }
/**/#if 0
/**/    {
/**/#define LEN 1024
/**/        int j,k;
/**/        double sine[LEN + tap_count];
/**/        double filtered[LEN];
/**/        double maxff=-2, minff=2, maxsf=-2, minsf=2;
/**/        for(i=0; i<LEN; i++){
/**/            double ss=0, sf=0, ff=0;
/**/            for(j=0; j<LEN+tap_count; j++)
/**/                sine[j]= cos(i*j*M_PI/LEN);
/**/            for(j=0; j<LEN; j++){
/**/                double sum=0;
/**/                ph=0;
/**/                for(k=0; k<tap_count; k++)
/**/                    sum += filter[ph * tap_count + k] * sine[k+j];
/**/                filtered[j]= sum / (1<<FILTER_SHIFT);
/**/                ss+= sine[j + center] * sine[j + center];
/**/                ff+= filtered[j] * filtered[j];
/**/                sf+= sine[j + center] * filtered[j];
/**/            }
/**/            ss= sqrt(2*ss/LEN);
/**/            ff= sqrt(2*ff/LEN);
/**/            sf= 2*sf/LEN;
/**/            maxff= FFMAX(maxff, ff);
/**/            minff= FFMIN(minff, ff);
/**/            maxsf= FFMAX(maxsf, sf);
/**/            minsf= FFMIN(minsf, sf);
/**/            if(i%11==0){
/**/                av_log(NULL, AV_LOG_ERROR, "i:%4d ss:%f ff:%13.6e-%13.6e sf:%13.6e-%13.6e\n", i, ss, maxff, minff, maxsf, minsf);
/**/                minff=minsf= 2;
/**/                maxff=maxsf= -2;
/**/            }
/**/        }
/**/    }
/**/#endif
/**/
/**/	//MES_FREE( tab );
/**/}
/**/
#endif

#define AV_RESAMPLE_CLOSE		av_freep(&c->filter_bank);	\
														av_freep(&c);

void av_resample_close(AVResampleContext *c){
		AV_RESAMPLE_CLOSE
}

#ifndef __KERNEL__
#define AV_RESAMPLE_COMPENSATE	c->compensation_distance= compensation_distance;	\
	c->dst_incr = c->ideal_dst_incr - c->ideal_dst_incr * (int64_t)sample_delta / compensation_distance;
#else
#define AV_RESAMPLE_COMPENSATE	c->compensation_distance= compensation_distance;	\
	c->dst_incr = c->ideal_dst_incr - div_s64((c->ideal_dst_incr * (int64_t)sample_delta), compensation_distance);
#endif

void av_resample_compensate(AVResampleContext *c, int sample_delta, int compensation_distance){
	AV_RESAMPLE_COMPENSATE
        }

//av_resample_init(output_rate, input_rate, TAPS, 10, 0, 0.8);
/**/AVResampleContext *av_resample_init(int out_rate, int in_rate, int filter_size, int phase_shift, int linear, double cutoff){
/**/    AVResampleContext *c= av_mallocz(sizeof(AVResampleContext));
/**/    double factor= FFMIN(out_rate * cutoff / in_rate, 1.0);
/**/    int phase_count= 1<<phase_shift;
/**/
/**/    c->phase_shift= phase_shift;
/**/    c->phase_mask= phase_count-1;
/**/    c->linear= linear;
/**/
#if USE_MATH_H
/**/    c->filter_length= FFMAX((int)ceil(filter_size/factor), 1);
#else
/**/    c->filter_length= FFMAX((int)ceil_f(filter_size/factor), 1);
#endif
/**/    c->filter_bank= av_mallocz(c->filter_length*(phase_count+1)*sizeof(FELEM));
/**/    av_build_filter(c->filter_bank, factor, c->filter_length, phase_count, 1<<FILTER_SHIFT, WINDOW_TYPE);
/**/    memcpy(&c->filter_bank[c->filter_length*phase_count+1], c->filter_bank, (c->filter_length-1)*sizeof(FELEM));
/**/    c->filter_bank[c->filter_length*phase_count]= c->filter_bank[c->filter_length - 1];
/**/
/**/    c->src_incr= out_rate;
/**/    c->ideal_dst_incr= c->dst_incr= in_rate * phase_count;
/**/    c->index= -phase_count*((c->filter_length-1)/2);
/**/
/**/    return c;
/**/}
/**/

/**/int av_resample(AVResampleContext *c, short *dst, short *src, int *consumed, int src_size, int dst_size, int update_ctx){
/**/    int dst_index, i;
/**/    int index= c->index;
/**/    int frac= c->frac;
/**/    int dst_incr_frac= c->dst_incr % c->src_incr;
/**/    int dst_incr=      c->dst_incr / c->src_incr;
/**/    int compensation_distance= c->compensation_distance;
/**/
/**/  if(compensation_distance == 0 && c->filter_length == 1 && c->phase_shift==0){
/**/        int64_t index2= ((int64_t)index)<<32;
/**/	#ifndef __KERNEL__
/**/        int64_t incr= (1LL<<32) * c->dst_incr / c->src_incr;
/**/        dst_size= FFMIN(dst_size, (src_size-1-index) * (int64_t)c->src_incr / c->dst_incr);
/**/	#else
/**/        int64_t incr= div_s64(((1LL<<32) * c->dst_incr), c->src_incr);
/**/        dst_size= FFMIN(dst_size, div_s64(((src_size-1-index) * (int64_t)c->src_incr), c->dst_incr));
/**/	#endif
/**/
/**/        for(dst_index=0; dst_index < dst_size; dst_index++){
/**/            dst[dst_index] = src[index2>>32];
/**/            index2 += incr;
/**/        }
/**/        frac += dst_index * dst_incr_frac;
/**/        index += dst_index * dst_incr;
/**/        index += frac / c->src_incr;
/**/        frac %= c->src_incr;
/**/  }else{
/**/    for(dst_index=0; dst_index < dst_size; dst_index++){
/**/        FELEM *filter= c->filter_bank + c->filter_length*(index & c->phase_mask);
/**/        int sample_index= index >> c->phase_shift;
/**/        FELEM2 val=0;
/**/
/**/        if(sample_index < 0){
/**/            for(i=0; i<c->filter_length; i++)
/**/                val += src[FFABS(sample_index + i) % src_size] * filter[i];
/**/        }else if(sample_index + c->filter_length > src_size){
/**/            break;
/**/        }else if(c->linear){
/**/            FELEM2 v2=0;
/**/            for(i=0; i<c->filter_length; i++){
/**/                val += src[sample_index + i] * (FELEM2)filter[i];
/**/                v2  += src[sample_index + i] * (FELEM2)filter[i + c->filter_length];
/**/            }
/**/		#ifndef __KERNEL__
/**/            val+=(v2-val)*(FELEML)frac / c->src_incr;
/**/		#else
/**/			val+=div_s64(((v2-val)*(FELEML)frac), c->src_incr);
/**/		#endif
/**/        }else{
/**/            for(i=0; i<c->filter_length; i++){
/**/                val += src[sample_index + i] * (FELEM2)filter[i];
/**/            }
/**/        }
/**/
/**/#ifdef CONFIG_RESAMPLE_AUDIOPHILE_KIDDY_MODE
/**/        dst[dst_index] = av_clip(lrintf(val), -32768, 32767);
/**/#else
/**/        val = (val + (1<<(FILTER_SHIFT-1)))>>FILTER_SHIFT;
/**/        dst[dst_index] = (unsigned)(val + 32768) > 65535 ? (val>>31) ^ 32767 : val;
/**/#endif
/**/
/**/        frac += dst_incr_frac;
/**/        index += dst_incr;
/**/        if(frac >= c->src_incr){
/**/            frac -= c->src_incr;
/**/            index++;
/**/        }
/**/
/**/        if(dst_index + 1 == compensation_distance){
/**/            compensation_distance= 0;
/**/            dst_incr_frac= c->ideal_dst_incr % c->src_incr;
/**/            dst_incr=      c->ideal_dst_incr / c->src_incr;
/**/        }
/**/    }
/**/  }
/**/    *consumed= FFMAX(index, 0) >> c->phase_shift;
/**/    if(index>=0) index &= c->phase_mask;
/**/
/**/    if(compensation_distance){
/**/        compensation_distance -= dst_index;
/**/        MES_ASSERT(compensation_distance > 0);
/**/    }
/**/    if(update_ctx){
/**/        c->frac= frac;
/**/        c->index= index;
/**/        c->dst_incr= dst_incr_frac + c->src_incr*dst_incr;
/**/        c->compensation_distance= compensation_distance;
/**/    }
/**/#if 0
/**/    if(update_ctx && !c->compensation_distance){
/**/#undef rand
/**/        av_resample_compensate(c, rand() % (8000*2) - 8000, 8000*2);
/**/av_log(NULL, AV_LOG_DEBUG, "%d %d %d\n", c->dst_incr, c->ideal_dst_incr, c->compensation_distance);
/**/    }
/**/#endif
/**/
/**/    return dst_index;
/**/}
