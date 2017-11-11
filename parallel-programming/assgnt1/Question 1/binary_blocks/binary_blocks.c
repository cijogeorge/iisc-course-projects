#include "mpi.h"
#include <stdio.h>

#define to_right 201
#define to_left  102

void MPI_Reduce_Custom (int* sendbuf, int* recvbuf, int count,
                        MPI_Datatype datatype, MPI_Op op, int root,
                        MPI_Comm comm);

void MPI_Reduce_Scatter_Custom (int* sendbuf, int* recvbuf, int count,
                                MPI_Datatype datatype, MPI_Op op, int group_root,
                                MPI_Comm comm, int start_rank, int end_rank);

int decimal_to_binary (int dec);

int main(int argc, char *argv [])
{
 
 int rank, numprocs, i, j, msg_size;
 MPI_Init(&argc, &argv);
 MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
 MPI_Comm_rank(MPI_COMM_WORLD, &rank);

 msg_size = 131072; // 512 Kb
 
 int sendbuf[numprocs][msg_size];
 int recvbuf[msg_size];

 for(i=0; i<numprocs; i++)
   for(j=0; j<msg_size; j++)
     sendbuf[i][j] = 0;
 
 for(i=0; i<msg_size; i++)
   recvbuf[i] = 0; 

 double start_time = MPI_Wtime();
 MPI_Reduce_Custom (sendbuf[rank], recvbuf, msg_size, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
 double end_time = MPI_Wtime();

 /*if (rank==0)
 { 
   for(i=0; i<msg_size; i++)
     printf("%d ", recvbuf[i]);
 }*/


  MPI_Finalize();

  if(rank==0)
  printf ("Execution Time (s) = %f Msg_size : %d\n", end_time - start_time, msg_size);
}

void MPI_Reduce_Custom (int* sendbuf, int* recvbuf, int count,
                       MPI_Datatype datatype, MPI_Op op, int root,
                       MPI_Comm comm)
{
  int pow_2, rank, numprocs, numprocs_bin, temp, n, start_rank, start_rank_native, end_rank, i, j, 
      flag=0, nprocs_native=0, nprocs_previous=0, nprocs_next=0, msg_size_previous, 
      msg_size_native, start_rank_next, start_rank_previous, from_rank,
      to_rank_1, to_rank_2, start_rank_next_temp=0, nprocs_next_temp=0;
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Status recv_status;
  numprocs_bin = decimal_to_binary (numprocs);
  temp = numprocs_bin;
  n=0;
  end_rank = 0;
  start_rank = start_rank_previous = start_rank_next = start_rank_native = numprocs;
  while (temp!=0)
  {
    if (temp % 10 == 1)
    {
      pow_2 = 1;
      for(i=0; i<n; i++)
      	pow_2= pow_2 * 2;
     
      if (end_rank!=0)
      {
        start_rank_next_temp = start_rank; 
        nprocs_next_temp = end_rank - start_rank + 1;
      }

      end_rank = start_rank - 1;  
      start_rank = end_rank - pow_2 + 1;

      if (flag)
      {
        start_rank_previous = start_rank;
        nprocs_previous = end_rank - start_rank + 1;
        flag=0;
      }

      if (rank>=start_rank && rank<=end_rank)
      {
        start_rank_native = start_rank;
        if(start_rank!=0)
         flag = 1;
       
        nprocs_native = end_rank - start_rank + 1;
        
        if(start_rank_next_temp)
        {
          start_rank_next = start_rank_next_temp;
          nprocs_next = nprocs_next_temp;
        }
      }

      MPI_Reduce_Scatter_Custom (sendbuf, recvbuf, count,
                                 datatype, op, start_rank,
                                 comm, start_rank, end_rank);
    }

    temp = temp/10;
    n++;
  }
 
  if(nprocs_previous>0 || nprocs_next>0)
  {
    msg_size_native = count/nprocs_native;

    if (nprocs_next>0)
    {
      from_rank = ((rank-start_rank_native) % nprocs_next) + start_rank_next;
      MPI_Recv(&recvbuf[msg_size_native], msg_size_native, MPI_INT, from_rank, to_left, MPI_COMM_WORLD, &recv_status);
    
      for(i=0; i<msg_size_native; i++)
       recvbuf[i]+=recvbuf[i+msg_size_native];
    }

    if (nprocs_previous>0)
    {
      msg_size_previous = count/nprocs_previous;
      
      for(i=(rank-start_rank_native), j=0; j<msg_size_native; i++, j+=(2*msg_size_previous)) 
      {
        to_rank_1 = i + start_rank_previous;
        to_rank_2 = i + start_rank_previous + (nprocs_previous/2);
        MPI_Send(&recvbuf[j], msg_size_previous, MPI_INT, to_rank_1, to_left, MPI_COMM_WORLD);
        MPI_Send(&recvbuf[j+msg_size_previous], msg_size_previous, MPI_INT, to_rank_2, to_left, MPI_COMM_WORLD);
      }
    }
  }

  if(start_rank_native == 0)
  {
    int msg_size = count/nprocs_native;

    for(i=nprocs_native/2; i>=1; i/=2)
    {
      if(rank>=i && rank<2*i)
        MPI_Send(recvbuf, msg_size, MPI_INT, rank-i, to_left, MPI_COMM_WORLD);
      if(rank<i)
      {
        MPI_Recv(&recvbuf[msg_size], msg_size, MPI_INT, rank+i, to_left, MPI_COMM_WORLD, &recv_status);
        msg_size*=2;
      }
    }
  }
} 

void MPI_Reduce_Scatter_Custom (int* inputbuf, int* resultbuf, int count, 
                                MPI_Datatype datatype, MPI_Op op, int group_root, 
                                MPI_Comm comm, int start_rank, int end_rank)
{
  int numprocs = end_rank - start_rank + 1;
  int rank, i, j, k, l;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Status  recv_status;
  int flag[numprocs]; 
 
  for(i=1; i<=(numprocs/2); i*=2)
  {
    int tempbuf[count/2];

    for (j=0; j<numprocs; j++)
    flag[j]=0; 
 

    for(k=0; k<numprocs; k++)
    {
      if (flag[k] == 0)
      {
        flag[k] = 1;
        flag[k+i] = 1;
        
        if (rank == k + start_rank)
        {
          MPI_Send(&inputbuf[count/2], count/2, MPI_INT, k+start_rank+i, to_right, MPI_COMM_WORLD);
          MPI_Recv(&tempbuf, count/2, MPI_INT, k+start_rank+i, to_left, MPI_COMM_WORLD, &recv_status);
       
          
          for(l=0; l<count/2; l++)
          {  
            inputbuf[l+count/2] = tempbuf[l];
          }

          for(l=0; l<count/2; l++)
            inputbuf[l]+=inputbuf[l+count/2];

          count = count/2;
          break;
          
        }

        if (rank == k+start_rank+i)
        {
          MPI_Recv(&tempbuf, count/2, MPI_INT, k+start_rank, to_right, MPI_COMM_WORLD, &recv_status);
          MPI_Send(&inputbuf[0], count/2, MPI_INT, k+start_rank, to_left, MPI_COMM_WORLD);

          for(l=0; l<count/2; l++)
          {
            inputbuf[l] = tempbuf[l];
          }

          for(l=0; l<count/2; l++)
            inputbuf[l]+=inputbuf[l+count/2]; 
 
          count = count/2; 
          break;
          
        } 
      }
    }
  }
  
  for(i=0; i<count; i++)
    resultbuf[i] = inputbuf[i];
}

int decimal_to_binary (int dec)
{
  int i=1, rem, res=0;

  while(dec!=0)
  {
    rem=dec%2;
    dec=dec/2;
    res=res+(i * rem);
    i=i*10;
  }

  return res;
}
