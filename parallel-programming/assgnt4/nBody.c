#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "mpi.h"

#define N 50000
#define DOMAIN_SIZE 512
#define SUBDOM_SIZE 64
#define TIMESTEPS 20
#define RADIUS 2

int rank, nprocs,

    /* procMap: Processor-SubDomain Mapping.
       procMap [P] gives the Morton Order number of
       the first subdomain belonging to processor P */
   *procMap, 
    
   /* subDomLoad: SubDomain Load.
      subDomLoad [subDom] gives the number of particles
      in 'subDom' where subDom is the Morton Order No.
      of a SubDomain */
   subDomLoad [SUBDOM_SIZE*SUBDOM_SIZE], 
   
   /* domain [][]: Domain, represented as a 2D Array.
      domain [i][j] = k => particle 'k' is at position coordinate (i,j) */
   domain [DOMAIN_SIZE][DOMAIN_SIZE],

   partPos [N][2], 

   /* Ideal load per processor */
   procLoad,

   /* Real load allocated to a given processor */
   procRealLoad;

/* Convert decimal number in Morton Order to corresponding 
   (x,y) Coordinates */
void mortonToCoords (int mortonNo, int *x, int *y)
{
  int i, j, bit;
  i = j = *x = *y = 0;

  while(mortonNo!=0)
  {
    bit = mortonNo % 2;
    *x = *x + (pow(2,i++) * bit);
    mortonNo = mortonNo/2;

    bit = mortonNo % 2;
    *y = *y + (pow(2,j++) * bit);
    mortonNo = mortonNo/2;
  }
}

/* Convert (x,y) Coordinates to corresponding
   Morton Order decimal number */
void coordsToMorton (int *mortonNo, int x, int y)
{
  int i, bit;

  i = *mortonNo = 0;

  while (x!=0 || y!=0)
  {
    bit = x % 2;
    *mortonNo = *mortonNo + (pow(2,i++) * bit);
    x = x/2;

    bit = y % 2;
    *mortonNo = *mortonNo + (pow(2,i++) * bit);
    y = y/2;
  }
}

/* Convert to SubDomain Morton No, given Coordinates */
int coordsToSubDom (int x, int y)
{
  int subDom;

  x = x / (DOMAIN_SIZE / SUBDOM_SIZE);
  y = y / (DOMAIN_SIZE / SUBDOM_SIZE);

  coordsToMorton (&subDom, x, y);

  return subDom;
}

/* Find the processor to which a given SubDomain belongs */
int subDomToProc (int subDom)
{
 
 int p;

 for (p=0; p<nprocs; p++)
 {
   if (subDom >= procMap [p] && subDom < (p+1 == nprocs ? SUBDOM_SIZE*SUBDOM_SIZE : procMap [p+1]))
     return p;
 }

 return -1;
}

/* Processor-SubDomain Mapping */
void procSubDomMap ()
{
  int load, i, k;
  procMap [0] = 0;

  for (i=1, k=1; i<nprocs && k<SUBDOM_SIZE*SUBDOM_SIZE; i++)
  {
    load = 0;

    while (load < procLoad)
      load += subDomLoad [k++];

    if ((load - procLoad) < (procLoad - (load - subDomLoad [k-1])))
      procMap [i] = k;

    else
      procMap [i] = --k;

  }
}

/* Find the load allocated to a given Processor */
int procRealLoadGen (int p)
{
  int load = 0, i, end; 
   
  end = (p + 1 != nprocs ? procMap [p + 1] : SUBDOM_SIZE*SUBDOM_SIZE);
  
  for (i = procMap [p]; i < end; i++)
    load += subDomLoad [i];  
   
  return load;
} 

/* Initialize */
void initialize ()
{
  int i, j, p;

  /* Initialize SubDomain Load */
  for (i=0; i<SUBDOM_SIZE*SUBDOM_SIZE; i++)
    subDomLoad [i] = 0;
 
  srand ((int) MPI_Wtime ());

  /* Assign random positions to particles 
     & update SubDomain Load */
  for (p=1; p<=N; p++)
  {
    int flag = 0;

    do 
    { 
      i = rand () % DOMAIN_SIZE;
      j = rand () % DOMAIN_SIZE;

      if (!domain[i][j])
      { 
        domain [i][j] = p;
        partPos [p-1][0] = i;
        partPos [p-1][1] = j;
        subDomLoad [coordsToSubDom (i,j)]++; 
        flag = 1;
      }
    }
    while (!flag); 
  }

  /* Processor - SubDomain Mapping */
  procSubDomMap ();
}

/* Change position of particles */
void changePos ()
{
  int i, j, x, y, flag [N];

  /* Initialize SubDomain Load */
  for (i=0; i<SUBDOM_SIZE*SUBDOM_SIZE; i++)
    subDomLoad [i] = 0;

  for (i=0; i<N; i++)
    flag [i] = 0;  
  
  /* Randomly change particle positions within given RADIUS */
  for (i=0; i<DOMAIN_SIZE; i++)
  {
    for (j=0; j<DOMAIN_SIZE; j++)
    {
      if (domain [i][j] && !flag [domain [i][j] - 1])
      {
        x = ((int) MPI_Wtime () % RADIUS) + (i - RADIUS);
        
        if (x < 0) 
          x = 0;
        if (x > (DOMAIN_SIZE - 1))
          x = DOMAIN_SIZE - 1;
        
        y = ((int) MPI_Wtime () % RADIUS) + (j - RADIUS); 
        
        if (y < 0)
          y = 0;
        if (y > (DOMAIN_SIZE - 1))
          y = DOMAIN_SIZE - 1;
        
        if (domain [x][y])
        {
          subDomLoad [coordsToSubDom (i,j)]++;
          flag [domain [i][j] - 1] = 1;
          continue;
        }

        domain [x][y] = domain [i][j];
        partPos [domain [i][j] - 1][0] = x;
        partPos [domain [i][j] - 1][1] = y;
        subDomLoad [coordsToSubDom (x,y)]++;
        flag [domain [i][j] - 1] = 1; 
        domain [i][j] = 0;
      }
    }
  }                        
}

int main (int argc, char *argv [])
{
  int t, i, j, subDom, load;
  double time_1 = 0, time_2 = 0, totalTime = 0;

  /* Initialize Domain */
  for (i=0; i<DOMAIN_SIZE; i++)
    for (j=0; j<DOMAIN_SIZE; j++)
      domain [i][j] = 0;

  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Status recv_status;

  procMap = (int *) malloc (nprocs * sizeof (int));
  procLoad = N/nprocs;

  int localPartPos [SUBDOM_SIZE*SUBDOM_SIZE][3];

  if (rank == 0)
  {
    /* Initialize Domain & Processor-SubDomain mapping */
    initialize ();
  }

  /* Broadcast the Particle Positions List & 
     Processor - SubDomain Mapping */
  MPI_Bcast (&partPos [0][0], N*2, MPI_INT, 0, MPI_COMM_WORLD); 
  MPI_Bcast (&procMap [0], nprocs, MPI_INT, 0, MPI_COMM_WORLD);
    
  /* Each processor creates a local list of particles
     which are allocated to it */
  for (i=0, j=0; i<N; i++)
  {
    subDom = coordsToSubDom (partPos [i][0], partPos [i][1]);
    if (subDom >= procMap [rank] && (rank + 1 == nprocs || subDom < procMap [rank+1]))
    {
      localPartPos [j][0] = i+1;
      localPartPos [j][1] = partPos [i][0];
      localPartPos [j][2] = partPos [i][1];
      j++;
    }
  }
   
  if (j < SUBDOM_SIZE*SUBDOM_SIZE)
  {
    localPartPos [j][0] = localPartPos [j][1] = localPartPos [j][2] = 0;
  }

  MPI_Barrier (MPI_COMM_WORLD);
 
  /* Timesteps Begin */
  for (t = 0; t < TIMESTEPS; t++)
  {
    /* Randomly move particles within a given RADIUS */
    if (rank == 0)
      changePos ();
   
    MPI_Barrier (MPI_COMM_WORLD);

    /* Broadcast the new particle positions */
    MPI_Bcast (&partPos [0][0], N*2, MPI_INT, 0, MPI_COMM_WORLD);


    /*----------------- LOAD BALANCING BEGIN -------------------------

        -> Each processor knows the new positions of the particles
           it owned in the previous timestep.
        -> Each processor knows the entire Processor-SubDomain Mapping,
           i.e it can find which processor currently owns a particle
           previously owned by it 

     ------------------------------------------------------------------*/
 
    /* Start Timer */
    if (rank == 0)
      time_1 = MPI_Wtime ();
  
    if (rank == 0)
    {
      /* Processor - SubDomain Mapping.
         Decide which SubDomains belong to which processor */
      procSubDomMap ();  
      
      /* Send to each processor, the number of particles it owns 
         after the change of particle positions */
      for (i=1; i<nprocs; i++)
      {
        procRealLoad = procRealLoadGen (i);
        MPI_Send (&procRealLoad, 1, MPI_INT, i, 201, MPI_COMM_WORLD);
      }

      procRealLoad = procRealLoadGen (0);
    }
  
    /* Each Processor receives the number of particles it owns
       after the change in particle positions */ 
    if (rank != 0)
      MPI_Recv (&procRealLoad, 1, MPI_INT, 0, 201, MPI_COMM_WORLD, &recv_status); 
   
    /* Broadcast Processor - SubDomain Mapping */
    MPI_Bcast (&procMap [0], nprocs, MPI_INT, 0, MPI_COMM_WORLD);
    
    /* Update Local Particle List based on the new procMap */
    load = 0;

    /* For each particle in the current localPartPos, see if the new position 
       belongs to a SubDomain owned by itself. If so, update the position in
       localPartPos. Else, send the particle to the processor that owns the
       particle in the new mapping. */
    for (i=0, j=0; localPartPos [i][0] != 0 && i < SUBDOM_SIZE*SUBDOM_SIZE; i++)
    {
      subDom = coordsToSubDom (partPos [localPartPos [i][0] - 1][0], partPos [localPartPos [i][0] - 1][1]);

      if (subDom >= procMap [rank] && (rank + 1 == nprocs || subDom < procMap [rank+1]))
      {
        load++;
        localPartPos [j][0] = localPartPos [i][0];
        localPartPos [j][1] = partPos [localPartPos [i][0] - 1][0];
        localPartPos [j][2] = partPos [localPartPos [i][0] - 1][1];
        j++;
      }

      else
      {
        localPartPos [i][1] = partPos [localPartPos [i][0] - 1][0];
        localPartPos [i][2] = partPos [localPartPos [i][0] - 1][1];
        MPI_Send (&localPartPos [i][0], 3, MPI_INT, subDomToProc (subDom), 101, MPI_COMM_WORLD);
      }
    }

    /* Each processor receives the particles it has to get from other processors */
    for (i=load; i<procRealLoad; i++)
    {
      MPI_Recv (&localPartPos [j][0], 3, MPI_INT, MPI_ANY_SOURCE, 101, MPI_COMM_WORLD, &recv_status);
      j++; 
    }

    if (j < SUBDOM_SIZE*SUBDOM_SIZE)
    {
      localPartPos [j][0] = localPartPos [j][1] = localPartPos [j][2] = 0;
    }
     
    /* Wait till all processors complete the communications */
    MPI_Barrier (MPI_COMM_WORLD);

    /* Stop Timer, and update Total Time */
    if (rank == 0)
    {
      time_2 = MPI_Wtime ();       
      totalTime += time_2 - time_1;
    }
    
    /*-------------------- LOAD BALANCING END -----------------------*/
  }

  if (rank == 0)
    printf ("N: %d Time: %f\n", N, totalTime);
   
  free (procMap);

  MPI_Finalize();
  return 0;
}
                     
