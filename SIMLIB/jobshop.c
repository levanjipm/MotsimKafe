/* External definitions for job-shop model. */

#include "simlib.h"		/* Required for use of simlib.c. */

#define EVENT_ARRIVAL         1	/* Event type for arrival of a job to the
				   system. */
#define EVENT_DEPARTURE       2	/* Event type for departure of a job from a
				   particular station. */
#define EVENT_END_SIMULATION  3 /* Event type for end of the simulation. */
#define STREAM_INTERARRIVAL   1 /* Random-number stream for interarrivals. */
#define STREAM_GROUP_SIZE     2
#define STREAM_JOB_TYPE       3 /* Random-number stream for job types. */
#define STREAM_HOTFOOD        4 /* Random-number stream for service times. */
#define STREAM_SANDWICH       5 /* Random-number stream for service times. */
#define STREAM_DRINK          6 /* Random-number stream for service times. */
#define STREAM_HOTFOOD_CT     7 /* Random-number stream for service times. */
#define STREAM_SANDWICH_CT    8 /* Random-number stream for service times. */
#define STREAM_DRINK_CT       9 /* Random-number stream for service times. */
#define MAX_NUM_STATIONS      5 /* Maximum number of stations. */
#define MAX_NUM_JOB_TYPES     3 /* Maximum number of job types. */

/* Declare non-simlib global variables. */

int num_stations, num_job_types, i, j, num_machines[MAX_NUM_STATIONS + 1],
  num_tasks[MAX_NUM_JOB_TYPES + 1],
  num_customers,
  route[MAX_NUM_JOB_TYPES + 1][MAX_NUM_STATIONS + 1], num_machines_busy[MAX_NUM_STATIONS + 1], job_type[4], task[4];
double mean_interarrival, length_simulation, prob_distrib_job_type[26],
  mean_service[MAX_NUM_JOB_TYPES + 1][MAX_NUM_STATIONS + 1],
  mean_accu;
FILE *infile, *outfile;
double size_prob[5];
double ST[4][4], ACT[4][4];

void serve_customer (int customer) {
  /* Determine the station from the route matrix. */
  printf("serve customer %d\n", customer);
  int station = route[job_type[customer]][task[customer]];

  /* Check to see whether all machines in this station are busy. */

  if (num_machines_busy[station] == num_machines[station])
    {

      /* All machines in this station are busy, so place the arriving job at
         the end of the appropriate queue. Note that the following data are
         stored in the record for each job:
         1. Time of arrival to this station.
         2. Job type.
         3. Current task number. */
      transfer[1] = sim_time;
      transfer[2] = job_type[customer];
      transfer[3] = task[customer];
      list_file (LAST, station);
    }

  else
    {

      /* A machine in this station is idle, so start service on the arriving
         job (which has a delay of zero). */
    
      sampst (0.0, station);  /* For station. */
      sampst (0.0, num_stations + job_type[customer]);  /* For job type. */
      ++num_machines_busy[station];
      timest ((double) num_machines_busy[station], station);

      /* Schedule a service completion.  Note defining attributes beyond the
         first two for the event record before invoking event_schedule. */

      transfer[3] = job_type[customer];
      transfer[4] = task[customer];
      transfer[5] = customer;
      printf("%d\n", customer);
      event_schedule (sim_time + mean_service[job_type[customer]][task[customer]], EVENT_DEPARTURE);
    }
}

void arrive (int new_job)		/* Function to serve as both an arrival event of a job
				   to the system, as well as the non-event of a job's
				   arriving to a subsequent station along its
				   route. */
{
  int station;

  /* If this is a new arrival to the system, generate the time of the next
     arrival and determine the job type and task number of the arriving
     job. */


  if (new_job == 1)
    {

      event_schedule (sim_time + expon (mean_interarrival, STREAM_INTERARRIVAL), EVENT_ARRIVAL);
      num_customers = random_integer (size_prob, STREAM_GROUP_SIZE);
      printf("num cust = %d\n", num_customers);
      for (int i=0; i<num_customers; i++) {
        job_type[i] = random_integer (prob_distrib_job_type, STREAM_JOB_TYPE);
        task[i] = 1;
        printf("masuk job : %d, task : %d\n", job_type[i], task[i]);
      }
    }

  for (int i=0; i<num_customers; i++) {
    serve_customer(i);
  }
}


void
depart (void)			/* Event function for departure of a job from a particular
				   station. */
{
  int station, job_type_queue, task_queue;

  /* Determine the station from which the job is departing. */
  int customer = transfer[5];
  job_type[customer] = transfer[3];
  task[customer] = transfer[4];

  station = route[job_type[customer]][task[customer]];

  /* Check to see whether the queue for this station is empty. */

  if (list_size[station] == 0)
    {

      /* The queue for this station is empty, so make a machine in this
         station idle. */

      --num_machines_busy[station];
      timest ((double) num_machines_busy[station], station);
    }

  else
    {

      /* The queue is nonempty, so start service on first job in queue. */

      list_remove (FIRST, station);

      /* Tally this delay for this station. */

      sampst (sim_time - transfer[1], station);

      /* Tally this same delay for this job type. */

      job_type_queue = transfer[2];
      task_queue = transfer[3];
      sampst (sim_time - transfer[1], num_stations + job_type_queue);

      /* Schedule end of service for this job at this station.  Note defining
         attributes beyond the first two for the event record before invoking
         event_schedule. */

      transfer[3] = job_type_queue;
      transfer[4] = task_queue;
      transfer[5] = customer;
      event_schedule (sim_time + mean_service[job_type_queue][task_queue], EVENT_DEPARTURE);
    }

  /* If the current departing job has one or more tasks yet to be done, send
     the job to the next station on its route. */

  if (task[customer] < num_tasks[job_type[customer]])
    {
      ++task[customer];
      serve_customer (customer);
    }
}


void report (void)			/* Report generator function. */
{
  int i;
  double overall_avg_job_tot_delay, avg_job_tot_delay, sum_probs;

  /* Compute the average total delay in queue for each job type and the
     overall average job total delay. */

  fprintf (outfile, "\n\n\n\nJob type     Average total delay in queue");
  overall_avg_job_tot_delay = 0.0;
  sum_probs = 0.0;
  for (i = 1; i <= num_job_types; ++i)
    {
      avg_job_tot_delay = sampst (0.0, -(num_stations + i)) * num_tasks[i];
      fprintf (outfile, "\n\n%4d%27.3f", i, avg_job_tot_delay);
      overall_avg_job_tot_delay += (prob_distrib_job_type[i] - sum_probs) * avg_job_tot_delay;
      sum_probs = prob_distrib_job_type[i];
    }
  fprintf (outfile, "\n\nOverall average job total delay =%10.3f\n", overall_avg_job_tot_delay);

  /* Compute the average number in queue, the average utilization, and the
     average delay in queue for each station. */

  fprintf (outfile, "\n\n\n Work      Average number      Average       Average delay");
  fprintf (outfile, "\nstation       in queue       utilization        in queue");
  for (j = 1; j <= num_stations; ++j)
    fprintf (outfile, "\n\n%4d%17.3f%17.3f%17.3f", j, filest (j), timest (0.0, -j) / num_machines[j], sampst (0.0, -j));
}

int main ()				/* Main function. */
{
  /* Open input and output files. */

  infile = fopen ("jobshop.in", "r");
  outfile = fopen ("jobshop.out", "w");

  /* Define Case */
  num_stations = 4;
  num_job_types = 3;
  length_simulation = 90;
  num_tasks[1] = 3; num_tasks[2] = 3; num_tasks[3] = 2;
  route[1][1] = 1; route[1][2] = 3; route[1][3] = 4;
  route[2][1] = 2; route[2][2] = 3; route[2][3] = 4;
  route[3][1] = 3; route[3][2] = 4;
  mean_accu = 0;
  
  
  /* Read input parameters. */
  /* Read mean interarrival */
  fscanf (infile, "%lg", &mean_interarrival);
  printf ("%.2f\n", mean_interarrival);
  
  /* Read Probability Size */
  for (i = 1; i <= 4; ++i)
	{
	  fscanf (infile, "%lg", &size_prob[i]);
	  printf ("%.1f ", size_prob[i]);
	  printf ("\n");
	}
	
  /* Read Probability Distribution */	
  for (i = 1; i <= num_job_types; ++i)
    {
	  fscanf (infile, "%lg", &prob_distrib_job_type[i]);
	  printf ("%.2f ", prob_distrib_job_type[i]);
	  printf ("\n");
	}
	
  /* Read ST and ACT */
  for (i = 1; i <= (num_stations-1); ++i)//Cashier not included
    {
	  fscanf (infile, "%lg %lg", &ST[i][1], &ST[i][2]);
	  printf ("%.1f %.1f\n", ST[i][1], ST[i][2]);
	}
  for (i = 1; i <= (num_stations-1); ++i)//Cashier not included
    {
	  fscanf (infile, "%lg %lg", &ACT[i][1], &ACT[i][2]);
	  printf ("%.1f %.1f\n", ACT[i][1], ACT[i][2]);
	}

  /* Calculate Mean Service */
  for (i = 1; i <= num_job_types; ++i)
    {
      for (j = 1; j <= (num_tasks[i]-1); ++j)
		{
			mean_service[i][j] = (uniform (ST[route[i][j]][1], ST[route[i][j]][2], (route[i][j] + 3)))/60;
			mean_accu = mean_accu + ((uniform (ACT[route[i][j]][1], ACT[route[i][j]][2], (route[i][j] + 3)))/60);
			printf ("%.3f\n", mean_service[i][j]);
		}
		mean_service[i][num_tasks[i]] = mean_accu;
		printf ("%.3f\n", mean_service[i][num_tasks[i]]);
    }
  for (j = 1; j <= num_stations; ++j)
    {
	  fscanf (infile, "%d", &num_machines[j]);
	  printf ("%d ", num_machines[j]);
	}

  /* Write report heading and input parameters. */

  fprintf (outfile, "Job-shop model\n\n");
  fprintf (outfile, "Number of work stations%21d\n\n", num_stations);
  fprintf (outfile, "Number of machines in each station     ");
  for (j = 1; j <= num_stations; ++j)
    fprintf (outfile, "%5d", num_machines[j]);
  fprintf (outfile, "\n\nNumber of job types%25d\n\n", num_job_types);
  fprintf (outfile, "Number of tasks for each job type      ");
  for (i = 1; i <= num_job_types; ++i)
    fprintf (outfile, "%5d", num_tasks[i]);
  fprintf (outfile, "\n\nDistribution function of job types  ");
  for (i = 1; i <= num_job_types; ++i)
    fprintf (outfile, "%8.3f", prob_distrib_job_type[i]);
  fprintf (outfile, "\n\nMean interarrival time of jobs%14.2f minutes\n\n", mean_interarrival);
  fprintf (outfile, "Length of the simulation%20.1f minutes\n\n\n", length_simulation);
  fprintf (outfile, "Job type     Work stations on route");
  for (i = 1; i <= num_job_types; ++i)
    {
      fprintf (outfile, "\n\n%4d        ", i);
      for (j = 1; j <= num_tasks[i]; ++j)
	fprintf (outfile, "%5d", route[i][j]);
    }
  fprintf (outfile, "\n\n\nJob type     ");
  fprintf (outfile, "Mean service time (in hours) for successive tasks");
  for (i = 1; i <= num_job_types; ++i)
    {
      fprintf (outfile, "\n\n%4d    ", i);
      for (j = 1; j <= num_tasks[i]; ++j)
	fprintf (outfile, "%9.2f", mean_service[i][j]);
    }

    printf("\n========================================\n\n");

  /* Initialize all machines in all stations to the idle state. */

  for (j = 1; j <= num_stations; ++j)
    num_machines_busy[j] = 0;

  /* Initialize simlib */

  init_simlib ();

  /* Set maxatr = max(maximum number of attributes per record, 4) */

  maxatr = 5;			/* NEVER SET maxatr TO BE SMALLER THAN 4. */

  /* Schedule the arrival of the first job. */

  event_schedule (expon (mean_interarrival, STREAM_INTERARRIVAL), EVENT_ARRIVAL);

  /* Schedule the end of the simulation.  (This is needed for consistency of
     units.) */

  event_schedule (length_simulation, EVENT_END_SIMULATION);

  /* Run the simulation until it terminates after an end-simulation event
     (type EVENT_END_SIMULATION) occurs. */
  do
    {

      /* Determine the next event. */

      timing ();

      /* Invoke the appropriate event function. */

      switch (next_event_type)
	{
	case EVENT_ARRIVAL:
	  arrive (1);
	  break;
	case EVENT_DEPARTURE:
	  depart ();
	  break;
	case EVENT_END_SIMULATION:
	  report ();
	  break;
	}

      /* If the event just executed was not the end-simulation event (type
         EVENT_END_SIMULATION), continue simulating.  Otherwise, end the
         simulation. */

    }
  while (next_event_type != EVENT_END_SIMULATION);

  fclose (infile);
  fclose (outfile);

  return 0;
}
