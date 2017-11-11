#include <stdio.h>
#include <stdlib.h>

#define NUM 1048576 
#define NUM_THREADS 512
#define NUM_BLOCKS 2048

/* Function to sort threads in each block using merge sort */

__global__ void sort_blocks(int *a)
{
  int i=2;

  __shared__ int temp [NUM_THREADS];

  while (i <= NUM_THREADS)
  {
    if ((threadIdx.x % i)==0)
    {
      int begin1 = threadIdx.x + (blockIdx.x * blockDim.x);
      int end1 = begin1 + i/2;
      int begin2 = end1;
      int end2 = begin2 + i/2;
      int target = threadIdx.x;

      do
      {
         if ((begin1 == end1) && (begin2 < end2))
           temp[target++] = a[begin2++];
        
         else if ((begin2 == end2) && (begin1 < end1))
           temp[target++] = a[begin1++];
        
         else if (a[begin1] < a[begin2])
           temp[target++] = a[begin1++];

         else
           temp[target++] = a[begin2++];

       }
       while ((begin1!=end1) && (begin2!=end2));
     } 

     __syncthreads();

     a[threadIdx.x + (blockIdx.x*blockDim.x)] = temp[threadIdx.x];

     __syncthreads();

     i *= 2;

   }
} 

/* Function to merge the sorted blocks using merge sort */

__global__ void merge_blocks(int *a, int *temp, int sortedsize)
{
  int id = blockIdx.x; 
  int begin1 = id * 2 * sortedsize;      
  int end1 = begin1 + sortedsize;
  int begin2 = end1;
  int end2 = begin2 + sortedsize;
  int target = id * 2 * sortedsize;

  do
  {
    if ((begin1 == end1) && (begin2 < end2))
      temp[target++] = a[begin2++];

    else if ((begin2 == end2) && (begin1 < end1))
      temp[target++] = a[begin1++];

    else if (a[begin1] < a[begin2])
      temp[target++] = a[begin1++];

    else
      temp[target++] = a[begin2++];

  }
  while ((begin1!=end1) && (begin2!=end2));

} 

int main()
{
  int *a = (int *) malloc (NUM * sizeof (int));
  int *dev_a, *dev_temp; 

  cudaMalloc((void **) &dev_a, NUM*sizeof(int));
  cudaMalloc((void **) &dev_temp, NUM*sizeof(int)); 
  
  for (int i = 0; i < NUM; i++)
  {
    a[i] = rand () % 10000; 
  }

  /* timing */
  cudaEvent_t start, stop;
  float time;
  cudaEventCreate(&start);
  cudaEventCreate(&stop);
  cudaEventRecord(start, 0);
  /* timing */
 
  cudaMemcpy(dev_a, a, NUM*sizeof(int), cudaMemcpyHostToDevice); 
  
  /* Sort the elements corresponding to the threads in each block */

  sort_blocks<<<NUM_BLOCKS, NUM_THREADS>>>(dev_a);

  cudaMemcpy(a, dev_a, NUM*sizeof(int), cudaMemcpyDeviceToHost); 

  /* Merge the sorted blocks */
		
  int blocks = NUM_BLOCKS/2;
  int sortedsize = NUM_THREADS;

  while (blocks > 0)
  {
     merge_blocks<<<blocks, 1>>>(dev_a, dev_temp, sortedsize);
     cudaMemcpy (dev_a, dev_temp, NUM*sizeof(int), cudaMemcpyDeviceToDevice);
     
     blocks /= 2;
     sortedsize *= 2;
  }

  cudaMemcpy(a, dev_a, NUM*sizeof(int), cudaMemcpyDeviceToHost);

  /* timing */
  cudaEventRecord (stop, 0);
  cudaEventSynchronize (stop);
  cudaEventElapsedTime (&time, start, stop);
  cudaEventDestroy (start);
  cudaEventDestroy (stop); 
  /* timing */
		
  bool passed = true;
   
  for(int i = 1; i < NUM; i++)
  {
    if (a [i-1] > a [i])
      passed = false;
  }
  
  printf("\nTest %s\n", passed ? "PASSED" : "FAILED");
  printf("Time : %f\n", time);
	
  cudaThreadExit();
		
  return 0;
}

