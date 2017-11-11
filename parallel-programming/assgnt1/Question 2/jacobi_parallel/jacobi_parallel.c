#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>

#define to_right 201
#define to_left  202
#define to_top 203
#define to_bottom 204
#define NXMAX 6000
#define NYMAX 6000
#define NSTEPS 1024

void main(int argc, char *argv [])
{ 
  MPI_Init(&argc, &argv);

  double start_time = MPI_Wtime ();

  int nrow, ncol;
  int rank, nprocs, rank2D, ndim;
  int dims[2], coords[2];
  int periods[2], reorder;
      
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm comm2D;
  MPI_Status recv_status;
  
  double (*phi)[NYMAX];
  phi = malloc ( NXMAX * sizeof(*phi));
  double (*old_phi)[NYMAX];
  old_phi = malloc ( NXMAX * sizeof(*old_phi));
  int (*mask)[NYMAX];
  mask = malloc ( NXMAX * sizeof(*mask)); 
  
  int nsteps, nptsx, nptsy, i, j, k, l, m;
  int nptsx_start, nptsx_end, nptsy_start, nptsy_end;
  
  nsteps = NSTEPS;  
  nptsx = NXMAX;
  nptsy = NYMAX;

  for(i=0; i<nptsx; i++)
  {
    for(j=0; j<nptsy; j++)
    {
      phi[i][j] = 0;
      mask[i][j] = 1;
    }
  }

  for (i=0; i<nptsx; i++)
  {
    mask [i][0] = mask [i][nptsy-1] = 0;
    phi [i][0] = phi [i][nptsy-1] = 1;
    
  }
  
  for (i=0; i<nptsy; i++)
  {
    mask [0][i] = mask [nptsx-1][i] = 0; 
    phi [0][i] = phi [nptsx-1][i] = 1;
  }

  mask [nptsx/2][nptsy/2] =
  mask [nptsx/2-1][nptsy/2-1] =
  mask [nptsx/2][nptsy/2-1] =
  mask [nptsx/2-1][nptsy/2] = 0;

  phi [nptsx/2][nptsy/2] =
  phi [nptsx/2-1][nptsy/2-1] =
  phi [nptsx/2][nptsy/2-1] =
  phi [nptsx/2-1][nptsy/2] = 1;
  
  if (nprocs == 4)
  { 
    nrow = 2; ncol = 2;
  }

  else if (nprocs == 8)
  {
    nrow = 2; ncol = 4;
  }
  
  else if (nprocs == 12)
  {
    nrow = 3; ncol = 4;
  }
  
  else return;
  
  ndim = 2;

  periods[0] = 0; periods[1] = 0; reorder = 1;

  dims[0] = nrow;
  dims[1] = ncol;

  MPI_Cart_create(MPI_COMM_WORLD, ndim, dims, periods, reorder, &comm2D);
 
  int cRank [nrow][ncol];
 
  for (i=0; i<nrow; i++)
    for (j=0; j<ncol; j++)
    { 
      coords [0] = i;
      coords [1] = j;
      MPI_Cart_rank(comm2D, coords, &rank2D);
      cRank [i][j] = rank2D;
    }

  int nptsx_part = nptsx/nrow;
  int nptsy_part = nptsy/ncol;
  
  for(k=0; k<nsteps; k++)
    for (i=0; i<nrow; i++)
    {
      for (j=0; j<ncol; j++) 
      {
        if (rank == cRank [i][j])
        {
          nptsx_start = i*nptsx_part;
          nptsx_end = nptsx_start + nptsx_part;
          nptsy_start = j*nptsy_part;  
          nptsy_end = nptsy_start + nptsy_part; 

          for (l=nptsx_start; l<nptsx_end; l++)
            for (m=nptsy_start; m<nptsy_end; m++)
              old_phi[l][m] = phi[l][m];

          double left_recv [nptsx_part], right_recv [nptsx_part], top_recv [nptsy_part], bottom_recv [nptsy_part];
          double left_send [nptsx_part], right_send [nptsx_part], top_send [nptsy_part], bottom_send [nptsy_part];
          
          if (i > 0) 
          { 
            for (l=0; l<nptsy_part; l++)
 	      top_send [l] = old_phi [nptsx_start][l+nptsy_start];
            MPI_Send (top_send, nptsy_part, MPI_DOUBLE, cRank [i-1][j], to_top, MPI_COMM_WORLD);
            MPI_Recv (top_recv, nptsy_part, MPI_DOUBLE, cRank [i-1][j], to_bottom, MPI_COMM_WORLD, &recv_status);
          }
         
          if (j > 0)
          {
            for (l=0; l<nptsx_part; l++)
              left_send [l] = old_phi [l+nptsx_start][nptsy_start];
            MPI_Send (left_send, nptsx_part, MPI_DOUBLE, cRank [i][j-1], to_left, MPI_COMM_WORLD);
            MPI_Recv (left_recv, nptsx_part, MPI_DOUBLE, cRank [i][j-1], to_right, MPI_COMM_WORLD, &recv_status);
          }
         
          if (i < nrow - 1)
          {
            MPI_Recv (bottom_recv, nptsy_part, MPI_DOUBLE, cRank [i+1][j], to_top, MPI_COMM_WORLD, &recv_status);
            for (l=0; l<nptsy_part; l++)
              bottom_send [l] = old_phi [nptsx_end - 1][l+nptsy_start];
            MPI_Send (bottom_send, nptsy_part, MPI_DOUBLE, cRank [i+1][j], to_bottom, MPI_COMM_WORLD);
          }
   
          if (j < ncol - 1)
          {
            MPI_Recv (right_recv, nptsx_part, MPI_DOUBLE, cRank [i][j+1], to_left, MPI_COMM_WORLD, &recv_status);
            for (l=0; l<nptsx_part; l++)
              right_send [l] = old_phi [l+nptsx_start][nptsy_end - 1];
            MPI_Send (right_send, nptsx_part, MPI_DOUBLE, cRank [i][j+1], to_right, MPI_COMM_WORLD);
          }
 
          for (l=nptsx_start; l<nptsx_end; l++)
            for (m=nptsy_start; m<nptsy_end; m++)
            {
              if (mask [l][m])
              {
                double LEFT, RIGHT, TOP, BOTTOM;

                if (l==nptsx_start)
                  TOP = top_recv[m-nptsy_start];
                else TOP = old_phi[l-1][m];
                
                if (m==nptsy_start) 
                  LEFT = left_recv[l-nptsx_start];
                else LEFT = old_phi[l][m-1];

                if (l==nptsx_end-1)
                  BOTTOM = bottom_recv[m-nptsy_start];
                else BOTTOM = old_phi[l+1][m];
 
                if (m==nptsy_end-1)
                  RIGHT = right_recv[l-nptsx_start];
                else RIGHT = old_phi[l][m+1];

                phi[l][m] = 0.25 * (LEFT + RIGHT + TOP + BOTTOM);
              }
            }
        }
      }
  }

  /*  
  for (j=0; j<ncol; j++)
  {
    for (i=0; i<nrow; i++) 
    { 
      MPI_Barrier (MPI_COMM_WORLD);
      if (rank == cRank [i][j])
      {
        printf ("\nCoordinates: (%d, %d), Rank: %d\n", i, j, rank);
        nptsx_start = i*nptsx_part;
        nptsx_end = nptsx_start + nptsx_part;
        nptsy_start = j*nptsy_part;
        nptsy_end = nptsy_start + nptsy_part;

        for (l=nptsx_start; l<nptsx_end; l++)
        {
          for (m=nptsy_start; m<nptsy_end; m++)
            printf ("%f ", phi [l][m]);
            printf ("\n");
        }
      }
    }
  }
  */
  
  free (phi);
  free (old_phi);
  free (mask);          
   
  double end_time = MPI_Wtime ();
  if(rank == 0)
   printf ("Execution time (s) : %f\n", end_time - start_time);
  
  MPI_Finalize();
}
