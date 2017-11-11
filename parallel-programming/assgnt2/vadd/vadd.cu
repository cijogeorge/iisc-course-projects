#include <stdio.h>
#include <stdlib.h>

#define NUM 4096
#define NUM_THREADS 512
#define NUM_BLOCKS 8

/* Function to implement c[i] = a[i] + b[i] non coalesced memory access */

__global__ void no_coalesce(int *a, int *b, int *c)
{
  int idx = threadIdx.x;
  idx = NUM_THREADS - idx - 1;
  int index = idx + (blockIdx.x * blockDim.x);
  c [index] = a [index] + b [index]; 
}

/* Function to implement c[i] = a[i] + b[i], coalesced memory access */

__global__ void coalesce (int *a, int *b, int *c)
{
  int index = threadIdx.x + (blockIdx.x * blockDim.x);
  c [index] = a [index] + b [index];
}

int main ()
{
  /* Timing */
  cudaEvent_t start, stop;
  cudaEventCreate(&start);
  cudaEventCreate(&stop); 
  /* Timing */

  int a[NUM], b[NUM], c[NUM], i;
  float time1, time2;
  bool passed;

  for (i=0; i<NUM; i++)
  {
    a[i] = i;
    b[i] = i;
  }
    
  int *dev_a, *dev_b, *dev_c;
    
  cudaMalloc(&dev_a, NUM * sizeof(int));
  cudaMemcpy(dev_a, a, NUM * sizeof(int), cudaMemcpyHostToDevice);
  cudaMalloc(&dev_b, NUM * sizeof(int));
  cudaMemcpy(dev_b, b, NUM * sizeof(int), cudaMemcpyHostToDevice);
  cudaMalloc(&dev_c, NUM * sizeof(int));
  cudaMemcpy(dev_c, c, NUM * sizeof(int), cudaMemcpyHostToDevice);
   
  /* Timing */
  cudaEventRecord(start, 0); 
  /* Timing */

  no_coalesce<<<NUM_BLOCKS, NUM_THREADS>>>(dev_a, dev_b, dev_c);
  cudaThreadSynchronize();

  /* Timing */
  cudaEventRecord (stop, 0); 
  cudaEventSynchronize (stop); 
  cudaEventElapsedTime (&time1, start, stop); 
  /* Timing */

  cudaMemcpy(c, dev_c, NUM*sizeof(int), cudaMemcpyDeviceToHost);

  passed = true;

  for (i=0; i<NUM; i++)
  { 
    if (c [i] != a [i] + b [i])
      passed = false;
  }
  
  printf ("\nNon-Coalesced:\t%s\nTime:\t%f\n", passed ? "PASSED" : "FAILED", time1);

  /* Timing */
  cudaEventRecord(start, 0);
  /* Timing */

  coalesce<<<NUM_BLOCKS, NUM_THREADS>>>(dev_a, dev_b, dev_c);
  cudaThreadSynchronize();

  /* Timing */
  cudaEventRecord (stop, 0);   
  cudaEventSynchronize (stop); 
  cudaEventElapsedTime (&time2, start, stop);
  /* Timing */

  cudaMemcpy(c, dev_c, NUM*sizeof(int), cudaMemcpyDeviceToHost);
   
  passed = true;

  for (i=0; i<NUM; i++)
  { 
    if (c [i] != a [i] + b [i])
      passed = false;
  }
  
  printf ("\nCoalesced:\t%s\nTime:\t%f\n", passed ? "PASSED" : "FAILED", time2);
  
  /* Timing */
  cudaEventDestroy (start);  
  cudaEventDestroy (stop);
  /* Timing */
        
  cudaFree (dev_a);
  cudaFree (dev_b);
  cudaFree (dev_c);

  return 0;
}
