#define OWN_P_TAG 1
#define P_TAG 2
#define FREEDYA_TAG 3
#define NUMBER_TAG 4
#define T_TAG 5
#define C_TAG 6
#define WRAPUP_TAG 7
#define THREAD_WINDUP_TAG 9
#define LISTENER0_WINDUP_TAG 10

#define GLOBAL_COUNT_TAG 20
#define READ_GLOBAL_COUNT_TAG 21

#define READ_P_TAG 12
#define READ_FREEDYA_TAG 13
#define READ_NUMBER_TAG 14
#define READ_T_TAG 15
#define READ_C_TAG 16
#define READ_WRAPUP_TAG 17

#define SENDER_TAG 7
#define RECEIVER_TAG 8

#define TRUE 1
#define FALSE 0
#define VERBOSE 0 

#define TOTAL_PROCESSES 32	/*12.4.11: has to match during MPI run instantiation at CLI*/

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <math.h>
#include <mpi.h>
#include <unistd.h>
#include <time.h>
#include <sys/timeb.h>

extern int C[2];
extern int T;
extern int P;
extern int Freed_YA;
extern int Number;
extern int time_horizon;
extern int total_processes;
extern int LISTENER_ALIVE;
extern int GLOBAL_COUNT;

extern struct timeb baseline_precise_time;
MPI_Status status_listener;

extern pthread_t Process_tid;

void *Listener(void* parameters)
{
  int message, code, side, process, process_index, ierr, iprobe_flag, l, temp_P;
  float listening_time;
  int floor_listening_time, last_pinged_time, count_THREAD_WINDUP_TAG_received;
  struct timeb scrap_precise_time;
  MPI_Request Isend_request;

  process_index=((int *)(parameters))[0];
  count_THREAD_WINDUP_TAG_received=0;

  do {

    /* Receive a message from some process related to lock D-Ses C, T or own Number, P, Freed_YA D-Ses*/
    /*int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status) */
     MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &iprobe_flag, &status_listener);

     /*23.8.17: Blacklist avoids any messages with TAG values that _thread or _main should be receiving, not lock-job handling listener*/	
     if (iprobe_flag && status_listener.MPI_TAG != READ_C_TAG && status_listener.MPI_TAG != READ_P_TAG && status_listener.MPI_TAG != READ_T_TAG && status_listener.MPI_TAG != READ_NUMBER_TAG && status_listener.MPI_TAG != READ_FREEDYA_TAG && status_listener.MPI_TAG != READ_WRAPUP_TAG && status_listener.MPI_TAG != READ_GLOBAL_COUNT_TAG)
     {
     MPI_Recv(&message, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status_listener);

     /* check for message conditions */
     switch( status_listener.MPI_TAG )
     {

     case  C_TAG:		/*read or write for C*/
					{ 
					  code=abs(message);
					  side=(code-1)%2;
					  process=floor((code-1)/2);

					  if(message>0) { 
							if(C[side]==-1) { C[side]=process; /*T=process; 6.6.11: atomically performing both C[side(i)]=i; T=i*/}
							else if (C[side]==process) 
								{ 
								C[side]=-1;
								/*if (T==process) T=-1;*/						     /*6.6.11: reset of T done here*/

								/*ierr=MPI_Send( &T, 1, MPI_INT, process, READ_T_TAG, MPI_COMM_WORLD);*/ /*6.6.11: atomic set of C[side(i)]=-1 and rival=T*/
								}
							}
					  else 		{ 
							ierr=MPI_Send( &(C[side]), 1, MPI_INT, process, READ_C_TAG, MPI_COMM_WORLD);

							if (VERBOSE)
							{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
							printf("To P%d: C[%d][%d]==%d", process, process_index, side, C[side]);
							fflush(stdout);}

							}

					}

					break;

     case  T_TAG: 			/*read or write for T*/
					{ 
					code=abs(message);
					process=code-1000;

					if(message>=1000) T=process;
					else { 	
						ierr=MPI_Send( &T, 1, MPI_INT, process, READ_T_TAG, MPI_COMM_WORLD);

						if (VERBOSE)
						{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
						printf("To P%d: T[%d]==%d", process, process_index, T);
						fflush(stdout);}
					     }
					}
					break;

     case P_TAG:				/*read or write for P -- if P is written, pass information to owner who may be on non-blocking receive*/
					{
					  code=abs(message);
					  process=code-1500;
					  if(message>=1500) 
						{ 
						int candidate_P, candidate_status, candidate_level;
						int current_P, current_status, current_level;
						candidate_P=message-1500;
						/*7.6.11: Do not communicate P if Freed_YA is already set*/

						/*10.8.17: candidate_P is examined for higher or lower value vis-a-vis current value of P*/
						/*the idea is to not assign a value to P if it is communicated from a level of lock that is higher (or lower) than the current level of contention*/
						/*10.8.17: A check for this already conducted in YADL_thread (2 places, part of YA algorithm) but spinning on P *could* continue if a correctly-set value is over-ruled by a wrongly set value immediately after*/
						candidate_status = candidate_P%3;
						candidate_level  = (candidate_P-candidate_status)/3;

						current_status = P%3;
						current_level  = (P-current_status)/3;

						if (candidate_level > current_level || candidate_level < current_level)
							{
							if (VERBOSE)
							{ { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
							printf("P%d TRIES P[%d]:=%d", status_listener.MPI_SOURCE, process_index, P);
							fflush(stdout); }
							}
						else
							{
							P=candidate_P;
						
							if (VERBOSE)
							{ { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
							printf("P%d asks: P[%d]:=%d", status_listener.MPI_SOURCE, process_index, P);
							fflush(stdout); }
							}
						}

					  else  {
						ierr=MPI_Send( &P, 1, MPI_INT, process, READ_P_TAG, MPI_COMM_WORLD);
						
						if (VERBOSE)
						{ { printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
						printf("To P%d: P[%d]==%d", process, process_index, P);
						fflush(stdout); }
						}
					}

					break;

     case  NUMBER_TAG:			/*read or write for Number -- if Number is written, pass information to owner who may be on non-blocking receive*/
					{ 
					  code=abs(message);
					  process=code-3000;
					  if(code==3000)
						{

						if (Number<0) 
							Number=abs(Number);
						else
							{ 
							Number=0;

							if (VERBOSE)
							{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
							printf("P%d asks: N[%d]:=%d", status_listener.MPI_SOURCE, process_index, Number);
							fflush(stdout);}
							}

						}
					else 
						{
						if (message>0)
							{
							Number=-(message-3000);

							if (VERBOSE)
							{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
							printf("N[%d]:=%d", process_index, -(message-3000));
							fflush(stdout);}
							}
						else
							{
							ierr=MPI_Send( &Number, 1, MPI_INT, process-1, READ_NUMBER_TAG, MPI_COMM_WORLD);
							
							if (VERBOSE)
							{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
							printf("To P%d: N[%d]==%d", process-1, process_index, Number);
							fflush(stdout);}

							/*7.6.11: somebody enquires your Number, it must be set free from YA-tree*/	
							/*if (Number==-(process_index+1))
								Freed_YA=1;*/

							}
						}
					}
					break;

     case FREEDYA_TAG: 			/*read or write for Freed_YA -- if Freed_YA is written, pass information to owner who may be on non-blocking receive*/
					if (message>=0)
						Freed_YA=message-4000;
					break;

					/*12.8.17: segment where read/write requests for GLOBAL_COUNT variable arrives. Two prevent 2 listener thread on the same node, these requests arrive only from nodes numbered other than (TOTAL_PROCESSES-1), as and when they enter C-S*/

     case GLOBAL_COUNT_TAG:		
					if (message < 1)
						{

						ierr=MPI_Send( &GLOBAL_COUNT, 1, MPI_INT, -message, READ_GLOBAL_COUNT_TAG, MPI_COMM_WORLD);
						
						if (VERBOSE)
						{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
						printf("To P%d: GLOBAL_COUNT==%d", -message, GLOBAL_COUNT);
						fflush(stdout);}

						}
					else
						{
						/*Assigning GLOBAL_COUNT to whatever number is sent by YAL_thread at a node*/
						GLOBAL_COUNT=message;

						if (VERBOSE)
						{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
						printf("From P%d: GLOBAL_COUNT:=%d", status_listener.MPI_SOURCE, GLOBAL_COUNT);
						fflush(stdout);}
						}
					break;


					/*9.8.17: Handshake protocol to complete the simulation. When the YADL_thread process at each node completes, it sends a THREAD_WINDUP message. These messages are counted, and have to number TOTAL_PROCESSES for node 0, whereas they must number TOTAL_PROCESSES+1 for all other nodes. Reason in next 'case'*/
     case THREAD_WINDUP_TAG:		if (VERBOSE)
					{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
					printf("P%d: THREAD_WINDUP of P%d, #%d", process_index, message, count_THREAD_WINDUP_TAG_received+1);
					fflush(stdout);}
					count_THREAD_WINDUP_TAG_received++;				
					if (count_THREAD_WINDUP_TAG_received == TOTAL_PROCESSES+ (process_index>0))
						LISTENER_ALIVE=FALSE;
					
					break;

					/*9.8.17: Because all other nodes 1-TOTAL_PROCESSES will send result packet to node 0, they must do so only after ensuring that node 0 has its YADL_Listener completed (since it'd gobble the result packet). Hence, each node (other than 0) receives a LISTENER0_WINDUP_TAG from node 0, and proceeds to shut down its YADL_Listener only when count_THREAD_WINDUP_TAG_received hits (TOTAL_PROCESSES+1)*/

     case LISTENER0_WINDUP_TAG:		if (VERBOSE)
					{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
					printf("P%d: LISTENER0_WINDUP from P%d, #%d", process_index, message, count_THREAD_WINDUP_TAG_received+1);
					fflush(stdout);}
					count_THREAD_WINDUP_TAG_received++;				
					if (count_THREAD_WINDUP_TAG_received == TOTAL_PROCESSES+ (process_index>0))
						LISTENER_ALIVE=FALSE;

					break;

    }	/*end of switch*/
    }

    ftime(&scrap_precise_time);
    listening_time=((scrap_precise_time.time-baseline_precise_time.time)+(.001)*(scrap_precise_time.millitm-baseline_precise_time.millitm));

    }while(LISTENER_ALIVE);

	if (VERBOSE)
	{{ printf("\n"); fflush(stdout); for (l=0; l<process_index; l++) {printf("\t"); fflush(stdout);} }
	printf("Listener-%d out", process_index);
	fflush(stdout);}

    pthread_exit(&time_horizon);
}

