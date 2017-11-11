/* Implementation of Fixed-Point Arithmetic */

#include "threads/fixed-point.h"
#include <stdint.h>

/*fixed-point numbers in signed p.q format*/

#define q 14        /* fixed-point numbers in signed 17.14 format */
#define f 16384     /* f = 2^q = 2^14 = 16384 */

/* Convert integer (n) to fixed-point */
int n_to_x (int n)
{
  return n * f;
}

/* Convert fixed-point (x) to integer (rounding toward zero) */
int x_to_n_zero (int x)
{
  return x/f;
}

/* Convert fixed-point (x) to integer (rounding to nearest) */
int x_to_n_nearest (int x)
{
  if (x >= 0)
    return (x + f / 2) / f;
  else
    return (x - f / 2) / f;
}

/* Add two fixed-point numbers */
int x_plus_y (int x, int y)
{
  return x + y;
}

/* Subtract two fixed-point numbers (y from x) */
int x_minus_y (int x, int y)
{
  return x - y;
}

/* Add a fixed-point number (x) to an integer (n) */
int x_plus_n (int x, int n)
{
  return x + n * f;
}

/* Subtract an integer (n) from a fixed-point number (x) */
int x_minus_n (int x, int n)
{
  return x - n * f;
}

/* Multiply two fixed-point numbers */
int x_mul_y (int x, int y)
{
  return ((int64_t) x) * y / f;
}

/* Multuply a fixed-point number (x) with an integer (n) */ 
int x_mul_n (int x, int n)
{
  return x * n;
}

/* Divide two fixed-point numbers */
int x_div_y (int x, int y)
{
  return ((int64_t) x) * f / y;
}

/* Divide a fixed point number (x) with an integer (n) */ 
int x_div_n (int x, int n)
{
  return x / n;
}





