#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "mpi.h"

/* Matrix Size 
   (for generating input matrix) */
#define N 1024

float *value;
int *colInd;
int rowPtr [N];
int colIndSize;

int *LcolInd;
int LrowPtr [N];
int myCols [N];
float U [N];

float A [N][N];
int  L[N][N], P[N], C[N], map[N];

/* Function to generate input matrix 
   in row compressed format */
void inputGen ()
{
  int i,j, temp;

  colIndSize = 0;
  for (i=0; i<N; i++)
  {
    for (j=0; j<N; j++)
    {
      if (i==j)
      {  
        colIndSize++;
        A [i][j] = 1+rand()%50;
      }
      else
        A[i][j] = 0;
    }
  }

  for (i=N/2, j=0; i<N; i++)
  {
    if (j<i)
    {
      colIndSize ++;
      A[i][j++] = 1+rand()%10;
    }
    if (j<i)
    {
      colIndSize ++;
      A[i][j++] = 1+rand()%10;
    }
  }

  value = (float *) malloc (colIndSize * sizeof (float));
  colInd = (int *) malloc (colIndSize * sizeof (int));
  
  rowPtr [0] = 0;

  for (i=0; i<N; i++)
  {
    temp = 0;
    for (j=0; j<=i; j++)
    {
      if (A[i][j])
      {
        temp++;
        colInd [rowPtr [i] + temp - 1] = j;
        value [rowPtr [i] + temp - 1] = A[i][j];
      }

      if (i+1<N)
       rowPtr [i+1] = rowPtr [i] + temp;
    }
  }
}

/* cdiv funtion */
void cdiv (int i)
{
   int j;

   A[i][i] = sqrt (A[i][i]);
  
   for (j=i+1; j<N; j++)
     A[j][i] = A[j][i]/A[i][i];
}

/* u(k,i) */
void updateU (int k, int i)
{
  int j;

  for (j=k; j<N; j++)
    U [j] += A[j][i] * A[k][i];
}

/* Function for Symbolic Factorization
   to generate L */
int symFact ()
{
  int fillCount=0, i, j, k, R [N][N], S[N];

  for (j=0; j<N; j++)
  {
    R[j][0] = -1;
  }
  
  for (j=0; j<N; j++)
  {
    for (i=0; i<N; i++)
    {
      if (A[i][j] != 0)
        S[i] = 1;
      else
        S[i] = 0;
    }
    
    for (i=0; R[j][i] != -1; i++)
    {
      for (k=0; k<N; k++)
      {
        if (!S[k] && L[k][R[j][i]] && k!=j)
        {
          S[k] = 1;
          fillCount++; 
        }
      }
    }
     
    for (i=j+1; i<N; i++)
     L [i][j] = S[i];
 
    for (i=j+1; i<N; i++)
    {
      if (L[i][j] == 1)
      {
        P [j] = i;
        C [i]++;      
        for (k=0; R[j][k]!= -1 && k<N; k++);
        R[i][k] = j;
        R[i][k+1] = -1;
        break;
      }
    }
  }

  return fillCount;
}  

  
int main (int argc, char *argv [])
{
  int nprocs, rank, fillCount, LcolIndSize, i, j, k, l, colRecv [N], colSend [N];
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Status recv_status;
  
  /* Generate input matrix in root and broadcast it to other procs */
  if (rank == 0)
    inputGen ();
  
  MPI_Bcast (&colIndSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
  
  if (rank!=0)
  {
    value = (float *) malloc (colIndSize * sizeof (float));
    colInd = (int *) malloc (colIndSize * sizeof (int));
  }

  MPI_Bcast (value, colIndSize, MPI_FLOAT, 0, MPI_COMM_WORLD);
  MPI_Bcast (colInd, colIndSize, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast (rowPtr, N, MPI_INT, 0, MPI_COMM_WORLD);
  
  /* Initialize & Populate A */

  for (i=0; i<N; i++)
  {
    for (j=0; j<N; j++)
      A[i][j] = 0;

    int end = i+1 == N ? colIndSize : rowPtr [i+1];

    for (j=rowPtr[i]; j<end; j++)
    {
      A[i][colInd[j]] = value [j];
    }
  }

  if (rank == 0)
  {

    /* Initialize L, P, map */

    for (i=0; i<N; i++)
    {
      P[i] = -1;
      C[i] = 0;
      map[i] = -1;

      for (j=0; j<N; j++)
        L[i][j] = 0;
    }
  
    /* Generate L */

    fillCount = symFact ();
   
    LcolIndSize = colIndSize + fillCount - N;

    LcolInd = (int *) malloc (LcolIndSize * sizeof (int));
   
    int temp;
    
    LrowPtr [0] = 0;

    for (i=0; i<N; i++)
    {
      temp = 0;
      for (j=0; j<i; j++)
      {
        if (L[i][j])
        {
          temp++;
          LcolInd [LrowPtr [i] + temp - 1] = j;
        }
        
        if (i+1<N)
          LrowPtr [i+1] = LrowPtr [i] + temp;
      }

    }
    
    /* Subtree-Subcube Mapping */ 

    int nNum = 0;

    for (i=0; i<N; i++)
    {
      if (C[i] == 0)
        nNum++;
    }

    if (nNum <= nprocs)
    {
      nNum = 0;

      for (i=0; i<N; i++)
      {
        if (C[i] == 0)
        { 
          map [i] = nNum++;
        }
      }
    }

    else
    {
      int split = nNum/nprocs;
      int splitTemp = 1;

      nNum = 0;
      
      for (i=0; i<N; i++)
      {
        if (C[i] == 0)
        { 
          if (map [i]==-1) 
            map[i]= nNum;
               
          if (splitTemp < split)
          {
            for (j=i+1; j<N; j++)
            {
              if (C[j]==0 && P[j]==P[i] && map[j] == -1)
              {
                map[j] = nNum;
                splitTemp++;
              
                if (splitTemp == split)
                  break;
              }
            }
          }
 
          if (splitTemp == split)
          { 
            splitTemp = 1;
            nNum ++;
          }     
          
          if (nNum == nprocs)
            nNum = 0;
        }
      }
    }

    int count = 0;
    int visited [N];

    while (count!=N)  
    {
      count =0;

      for (i=0; i<N; i++)
        visited [i] = 0;

      for (i=0; i<N; i++)
      {
        if (map [i] != -1)
        {
          count++;
          if (!visited [i] && P[i]!=-1 && map[P[i]] == -1)
          {   
            map [P[i]] = map [i];
            visited [P[i]] = 1;
          }
        }
      }
    }
  }
  
  /* Broadcast L, map to other procs from root */
  
  MPI_Bcast (map, N, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast (&LcolIndSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
  
  if (rank!=0)
  {
    LcolInd = (int *) malloc (LcolIndSize * sizeof (int));
  }

  MPI_Bcast (LcolInd, LcolIndSize, MPI_INT, 0, MPI_COMM_WORLD);
  MPI_Bcast (LrowPtr, N, MPI_INT, 0, MPI_COMM_WORLD);

  /* Populate myCols */

  myCols [0] = -1;

  for (i=0, j=0; j<N; j++)
  {
    if (map[j] == rank)
    {
      myCols [i] = j;
      myCols [++i] = -1;
    }
  }

  /* Regenerate L matrix from row compressed format */

  for (i=0; i<N; i++)
  {
    for (j=0; j<N; j++)
      L[i][j] = 0;

    int end = i+1 == N ? LcolIndSize : LrowPtr [i+1];

    for (j=LrowPtr[i]; j<end; j++)
    {
      L[i][LcolInd[j]] = 1;
    }
  }

  /* Numerical Factorization */
  
  for (j=0; j<N; j++)
  {
    int myColsFlag = 0, structFlag = 0;
    
    for (i=0; myCols[i]!=-1; i++)
    {
      if (myCols[i] == j)
      {
        myColsFlag = 1;
        break;
      }
    }
   
    for (i=0; myCols[i] != -1; i++)
    {
      for (k=0; k<j; k++)
      {
        if (L[j][k] && myCols[i] == k)
        {
          structFlag = 1;
          break;
        }
      }
      
      if (structFlag)
        break;
    }
  
    if (myColsFlag || structFlag)
    { 
      for (i=0; i<N; i++)
        U[i] = 0;
      
      if (structFlag)
      {
        for (i=0; myCols[i] != -1; i++)
        {
          for (k=0; k<j; k++)
          {
            if (L[j][k] && myCols[i] == k)
              updateU (j,k);    
          }
        }
      }

      if (map[j] != rank)
        MPI_Send (U, N, MPI_FLOAT, map[j], 201, MPI_COMM_WORLD);
   
      else
      {
        for (i=0; i<N; i++)
          A[i][j] = A[i][j] - U[i];

        for (k=0; k<j; k++)
        {
          if (L[j][k])
          {
            i=0;
            while (myCols[i] != k && myCols[i] != -1) i++;

            if (myCols[i] == -1)
            { 
              MPI_Recv (U, N, MPI_FLOAT, map[k], 201, MPI_COMM_WORLD, &recv_status);
            
              for (l=0; l<N; l++)
                A[l][j] = A[l][j] - U[l];
            }
          }
        }    
        
        cdiv (j);
      }
    }
  }

  /* Send columns from other procs to root */
  
  for (j=0; myCols[j]!=-1; j++)
  {
    if (rank != 0)
    {
      for (i=0; i<N; i++)
      {
        colSend [i] = A[i][myCols[j]];
      }

      MPI_Send (colSend, N, MPI_FLOAT, 0, 101, MPI_COMM_WORLD);
    } 
  }
  
  /* Gather result at root */

  if (rank == 0)
  {
    for (j=0; j<N; j++)
    { 
      if (map[j] != 0)
      {
        MPI_Recv (colRecv, N, MPI_FLOAT, map[j], 101, MPI_COMM_WORLD, &recv_status);
        for (i=0; i<=j; i++)
          A[i][j] = colRecv[i];
      }
    }
  }
  
  MPI_Finalize();
  
  return 0;
}
