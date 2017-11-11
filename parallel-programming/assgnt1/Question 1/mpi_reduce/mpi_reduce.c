#include "mpi.h"
#include <stdio.h>

int main(int argc, char *argv [])
{
 
 int rank, numprocs, i, j, msg_size;
 MPI_Init(&argc, &argv);
 MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
 MPI_Comm_rank(MPI_COMM_WORLD, &rank);

 msg_size = 131072;
 
 int sendbuf[numprocs][msg_size];
 int recvbuf[msg_size];
 
 for(i=0; i<numprocs; i++)
   for(j=0; j<msg_size; j++)
     sendbuf[i][j] = 1;
 
 for(i=0; i<msg_size; i++)
   recvbuf[i] = 0; 

 double start_time = MPI_Wtime ();
 MPI_Reduce (sendbuf[rank], recvbuf, msg_size, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
 double end_time = MPI_Wtime ();

 /*if (rank==0)
 { 
   for(i=0; i<msg_size; i++)
     printf("%d\n", recvbuf[i]);
 }*/

 MPI_Finalize();
 
 if(rank==0)
  printf ("Execution Time (s) = %f Msg_size = %d\n", end_time - start_time, msg_size);
 
}

