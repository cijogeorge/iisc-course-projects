#include <stdio.h>
#include <stdlib.h>

#define NPROCS 512
#define Q_SIZE 50

int
    /* WJ : Set of Waiting Jobs.
       WJ [0]: No. of procs requested 
       WJ [1]: User estimated running time
       WJ [2]: Actual running time
    */
    WJ [Q_SIZE + 1][3],

    /* RJ : Set of Running Jobs
       RJ [0] : No. of procs allocated
       RJ [1] : Remaining running time (based on user estimate)
       RJ [2] : Actual finishing time
    */
    RJ [Q_SIZE][3],

    /* Matrix M as defined in LOS Algorithm */
    M [Q_SIZE + 1][NPROCS][2],

    /* Set S as defined in LOS Algorithm */
    S [Q_SIZE],
  
    /* No. of free processors available at a time */
    freeProcs_n, 
   
    /* No. of jobs in WJ */
    wq_n, 

    /* Variable to keep track of time */
    T,
    
    /* Numerator to calculate Utilization */
    numerator,
    
    /* Denominator to calculate Utilization */
    denominator,
   
    /* Buffer to store values read from input file */
    inp [4];


/* Function to read the next line from the input file */
int readNxtLine (FILE *fp)
{
  int i;
  char buf [6];

  for (i=0; i<4; i++)
  {
    fscanf (fp, "%s", buf);

    if (buf == NULL)
      return 1;

    inp [i] = atoi (buf);
  }
 
  numerator += inp [0] * inp [3]; 
  return 0;
}

/* Function to construct M as defined in LOS Algorithm */
void construct_M (int wq_n, int freeProcs_n)
{
  int i, j, util;
  
  for (j=0; j<=freeProcs_n; j++)
    M [0][j][0] = M [0][j][1] = 0;

  for (i=1; i<=wq_n; i++)
  {
    M [i][0][0] = 0;

    for (j=1; j<=freeProcs_n; j++)
    {
      M [i][j][0] = M[i-1][j][0];
      M [i][j][1] = 0;

      if (WJ [i][0] <= j)
      {
        util = M [i - 1][j - WJ [i][0]][0] + WJ [i][0];
        
        if (util > M [i - 1][j][0])
        {
          M [i][j][0] = util;
          M [i][j][1] = 1;
        }
      }
    }
  }
}

/* Function to construct set S as defined in LOS Algorithm */
void construct_S (int wq_n, int freeProcs_n)
{
  int i, j, k;
 
  for (k=0; S [k] != -1 && k < Q_SIZE; k++)
    S [k] = -1;

  i = wq_n;
  j = freeProcs_n;

  k = 0;

  while (i>0 && j>0)
  {
    if (M [i][j][1] == 1)
    {
      S [k++] = i;
      j = j - WJ [i][0];
    }

    i--;
  }      
}

/* Function to find the next job to finish from RJ and to remove the job.
   Returns the time at which the job finishes  */
int finishNextJob ()
{
  int i, j, index=0, small = RJ [0][2];

  /* Find the next job to finish */
  for (i=1; RJ [i][0] != -1 && i < Q_SIZE; i++)
  {
    if (RJ [i][2] < small)
    {
      small = RJ [i][2];
      index = i;
    }
  }

  /* Remove the finished job from RJ */
  freeProcs_n += RJ [index][0];

  for (i=index; RJ [i][0] != -1 && i < Q_SIZE; i++)
  {
     for (j=0; j<3; j++)
       RJ [i][j] = RJ [i+1][j];
  }
    
  return small;
}

/* Function to update remaining times for jobs in RJ */
void updateRemTimes (int t)
{
  int i;
  
  for (i=0; RJ [i][0] != -1 && i< Q_SIZE; i++)
    RJ [i][1] -= t;
}
      
/* Function to remove jobs in set S from WJ and add them to RJ */
void runJobsInS ()
{
  int i, j, k, wq_n_old = wq_n;

  for (i=0; RJ [i][0] != -1 && i < Q_SIZE; i++);

  for (j=0; S [j] != -1 && j < Q_SIZE; j++)
  {
    /* Add jobs to RJ */
    RJ [i][0] = WJ [S[j]][0];
    RJ [i][1] = WJ [S[j]][1];
    RJ [i][2] = T + WJ [S[j]][2];
    
    /* Remove jobs from WJ */
    WJ [S[j]][0] = WJ [S[j]][1] = WJ [S[j]][2] = -1;

    wq_n--;
    freeProcs_n -= RJ [i][0];
    i++;
  }

  /* Remove gaps from WJ matrix */
  for (i=wq_n_old; i>0; i--)
  {
    if (WJ [i][0] == -1)
    {
      for (j=i; j<=wq_n_old; j++)
      {
        for (k=0; k<3; k++)
          WJ [j][k] = WJ [j+1][k];
      }
    }
  }    
}
    
/* MAIN FUNCTION */
void main ()
{
  int i, j, T_Old, T_Start, nextLine_n = 1;
  
  FILE *fp = fopen ("input.txt", "r");

  numerator = denominator = 0;
  
  /* Initialize WJ & RJ */
  for (i=0; i<=Q_SIZE; i++)
  {
    for (j=0; j<3; j++)
      WJ [i][j] = -1;
 
    if (i<Q_SIZE)
    {
      S [i] = -1;
      RJ [i][0] = RJ [i][1] = RJ [i][2]= -1;
    }
  }

  /* ---------------INITIAL ASSIGNMENT---------------------------
     
     Assuming that when LOS runs for the first time, there are
     10 jobs in WJ and all the 512 processors are free. 
     Start Time (T_Start) of simulation is assumed to be 325000
     (a time between the submission times of Job 10 & Job 11) 

     ------------------------------------------------------------ */

  T = T_Start = 325000;
  wq_n = 10;
  freeProcs_n = NPROCS;
  
  for (i=1; i<=wq_n; i++)
  {
    readNxtLine (fp);
    nextLine_n ++;
    WJ [i][0] = inp [0];
    WJ [i][1] = inp [1];
    WJ [i][2] = inp [3];
  }

  construct_M (wq_n, freeProcs_n);
  construct_S (wq_n, freeProcs_n);
  
  runJobsInS ();   
  
  /* Henceforth LOS Runs whenever a job finishes execution */
  
  T_Old = T;
  T = finishNextJob ();
  updateRemTimes (T - T_Old);
  readNxtLine (fp);
  nextLine_n ++;

  /* Loop while the input file is not completely read 
     OR RJ is not empty OR WJ is not empty */ 
  while (nextLine_n <= 100 || RJ [0][0] != -1 || WJ [1][0] != -1)
  {
    /* Update WJ (read in Jobs from input file that has arrived
       before the current value of T */
    while (inp [2] < T && nextLine_n <= 100)
    {
      WJ [++wq_n][0] = inp [0];
      WJ [wq_n][1] = inp [1];
      WJ [wq_n][2] = inp [3];

      readNxtLine (fp);
      nextLine_n ++;
    } 
    
    /* Run LOS if WJ is not empty */
    if (WJ [1][0] != -1)
    { 
      construct_M (wq_n, freeProcs_n);
      construct_S (wq_n, freeProcs_n);
      runJobsInS ();
    }

    /* Skip to the time when the next job has finished */
    T_Old = T;
    T = finishNextJob ();
    updateRemTimes (T - T_Old);
  }

  denominator = (T - T_Start) * 512;

  printf ("Utilization: %.2f %c\n", ((float) numerator * 100)/denominator, '%');

  fclose (fp);
}


/********** -- RESULT -- ***********
 *                                 *
 *  UTILIZATION ACHIEVED: 48.85 %  *  
 *                                 *
 ***********************************/ 
