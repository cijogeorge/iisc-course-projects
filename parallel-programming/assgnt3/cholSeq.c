#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#define N 128

float *value;
int *colInd;
int rowPtr [N];
int colIndSize;

int L[N][N], P[N], C[N], map[N];
float A[N][N];

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

void symFact ()
{
  int i, j, k, R [N][N], S[N];

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
}  

void cdiv (int i)
{
   int j;

   A[i][i] = sqrt (A[i][i]);
  
   for (j=i+1; j<N; j++)
     A[j][i] = A[j][i]/A[i][i];
}

void cmod (int k, int i)
{
  int j;
  
  for (j=k; j<N; j++)
    A[j][k] = A[j][k] - A[j][i] * A[k][i];
} 

main ()
{
  int i, j, k;

  inputGen();

  // Initialize A & L

  for (i=0; i<N; i++)
  {
    P[i] = -1;
    C[i] = 0;
    map[i] = -1;

    for (j=0; j<N; j++)
    {
      A[i][j] = 0;
      L[i][j] = 0;
    }
  }
  
  // Populate A

  for (i=0; i<N; i++)
  {
    int end = i+1 == N ? colIndSize : rowPtr [i+1];  

    for (j=rowPtr[i]; j<end; j++)
    {
      A[i][colInd[j]] = value [j];
    }
  }
 
  // Calculate L

  symFact ();

  // Numerical Factorization 

  for (j=0; j<N; j++)
  {
    for (k=0; k<j; k++)
    {
      if (L[j][k] == 1)
        cmod (j,k);
    }
    
    cdiv (j);
  } 
}
 
