/* Wine's i386 ucrtbase exports __libm_sse2_powf but not powf; mingw programs import
 * powf from api-ms-win-crt-math-l1-1-0, which then raises EXCEPTION_WINE_STUB. route
 * those calls through pow so the PE32 suite can run. */
#include <math.h>

float powf(float x, float y) { return (float)pow((double)x, (double)y); }
