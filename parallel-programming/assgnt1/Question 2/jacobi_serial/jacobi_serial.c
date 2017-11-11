#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>

#define NXMAX 6000
#define NYMAX 6000
#define NSTEPS 1024

void main(int argc, char *argv [])
{ 
  MPI_Init(&argc, &argv);
  double start_time = MPI_Wtime ();
 
  double (*phi)[NYMAX];
  phi = malloc ( NXMAX * sizeof(*phi));
  double (*old_phi)[NYMAX];
  old_phi = malloc ( NXMAX * sizeof(*old_phi));
  int (*mask)[NYMAX];
  mask = malloc ( NXMAX * sizeof(*mask));
  int nsteps, nptsx, nptsy, i, j, k;
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

  for(k=0; k<nsteps; k++)
  {
    for(i=0; i<nptsx; i++)
      for(j=0; j<nptsy; j++)
        old_phi[i][j] = phi[i][j];

    for(i=0; i<nptsx; i++)
      for(j=0; j<nptsy; j++)
        if(mask[i][j]) 
          phi[i][j] = 0.25*(old_phi[i][j-1] + old_phi[i][j+1] + old_phi[i-1][j] + old_phi[i+1][j]);
  }
  
  /*
  for (i=0; i<nptsx; i++)
  {
    printf("\n");
    for (j=0; j<nptsy; j++)
      printf("%f ", phi[i][j]);
  }
  printf("\n");
  */

  free (phi);
  free (old_phi);
  free (mask);

  double end_time = MPI_Wtime ();
  printf("Execution time (s) : %f\n", end_time - start_time);

  MPI_Finalize ();

}
