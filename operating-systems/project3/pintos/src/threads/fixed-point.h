/* Header file for Fixed-Point Arithmetic Implementation */

/*fixed-point numbers in signed p.q format*/

#define q 14        /* fixed-point numbers in signed 17.14 format */
#define f 16384     /* f = 2^q = 2^14 = 16384 */

/* Convert integer (n) to fixed-point */
int n_to_x (int n);

/* Convert fixed-point (x) to integer (rounding toward zero) */
int x_to_n_zero (int x);

/* Convert fixed-point (x) to integer (rounding to nearest) */
int x_to_n_nearest (int x);

/* Add two fixed-point numbers */
int x_plus_y (int x, int y);

/* Subtract two fixed-point numbers (y from x) */
int x_minus_y (int x, int y);

/* Add a fixed-point number (x) to an integer (n) */
int x_plus_n (int x, int n);

/* Subtract an integer (n) from a fixed-point number (x) */
int x_minus_n (int x, int n);

/* Multiply two fixed-point numbers */
int x_mul_y (int x, int y);

/* Multuply a fixed-point number (x) with an integer (n) */
int x_mul_n (int x, int n);

/* Divide two fixed-point numbers */
int x_div_y (int x, int y);

/* Divide a fixed point number (x) with an integer (n) */
int x_div_n (int x, int n);

