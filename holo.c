#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define MSG_SIZE 3
#define REQUEST_TAG 0
#define TAKE_TUGBOATS_TAG 1
#define RELEASE_TUGBOATS_TAG 2
#define REPLY_TAG 3
#define GO_AHEAD 0
#define IMBEFOREYOU 10

// msg[0] = timestamp
// msg[1] = rank
// msg[2] = boatsReq

int inQueue = 0;
int holowniki = 25;
int position = -1;
int reserved = 0;
int rank = -1;
int boatsReq = 7;
long timestamp = -1;
int size = -1;
int repliesRemaining = 0;

void *recvFun()
{
	long msg[MSG_SIZE];
	MPI_Status status;
	while(1) {
		MPI_Recv(msg, MSG_SIZE, MPI_LONG, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		long reply[1];
		switch (status.MPI_TAG) {
			case REQUEST_TAG:
				if (inQueue) {
					if (timestamp < msg[0] || timestamp == msg[0] && rank < msg[1]) {
						reply[0] = boatsReq;
					}
					else
						reply[0] = GO_AHEAD;
				}
				else {
					reply[0] = GO_AHEAD;
				}
				MPI_Send(reply, 1, MPI_LONG, status.MPI_SOURCE, REPLY_TAG, MPI_COMM_WORLD);
				break;
			case TAKE_TUGBOATS_TAG:
				holowniki -= msg[2];
				if (inQueue) {
					reserved -= msg[2];
				}
				break;
			case RELEASE_TUGBOATS_TAG:
				holowniki += msg[2];
				break;
			case REPLY_TAG:
				if (msg[0] != GO_AHEAD) {
					reserved += msg[0];
				}
				repliesRemaining -= 1;
				break;
		}
	}
}

int sendToAll(long * msg, int msg_size, int tag){
	int i;
	for (i = 0; i < size; i++){
		if (i != rank){
			MPI_Send(msg, msg_size, MPI_LONG, i, tag, MPI_COMM_WORLD);
		}
	}
}


int main(int argc, char **argv)
{
	MPI_Status status;
	pthread_t tid;

	// MPI_Init(&argc, &argv);
	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	printf("provided: %d, required: %d\n", provided, MPI_THREAD_SERIALIZED);
	if (provided < MPI_THREAD_SERIALIZED) {
		printf("MPI does not provide needed threading level\n");
		return(-1);
	}

printf("time: %f\n", MPI_Wtime());

	MPI_Comm_rank( MPI_COMM_WORLD, &rank );
	MPI_Comm_size( MPI_COMM_WORLD, &size );

	MPI_Barrier(MPI_COMM_WORLD);
	double startTime = MPI_Wtime();
	srand(time(NULL)+rank);

	boatsReq = rand()%7 + 2;
	printf("[%d]: potrzebuje %d holownikow\n", rank, boatsReq);
	pthread_create(&tid, NULL, recvFun, NULL);

	while(1){
		usleep(100000*(rand()%100+5));
		inQueue = 1;
		timestamp = (long)(1000 * (MPI_Wtime() - startTime));
		long msg[2];
		msg[0] = timestamp;
		msg[1] = rank;
		printf("[%d]: chce wpłynąć do portu. time: %ld\n", rank, timestamp);
		repliesRemaining = size - 1;
		reserved = 0;
		sendToAll(msg, 2, REQUEST_TAG);
		while(repliesRemaining != 0){
			usleep(10000);
		}
		printf("[%d]: %d wolnych, %d zarezerwowanych holowników\n", rank, holowniki,
		reserved);
		while(holowniki - reserved < boatsReq) {
			usleep(10000);
		}
		inQueue = 0;
		long cmsg[3];
		cmsg[0] = timestamp;
		cmsg[1] = rank;
		cmsg[2] = boatsReq;
		// wejscie do strefy krytycznej
		printf("[%d]: wchodze do strefy krytycznej, zabieram %d holownikow\n", rank, boatsReq);
		holowniki -= boatsReq;
		sendToAll(cmsg, 3, TAKE_TUGBOATS_TAG);
		sleep(5); // strefa krytyczna
		sendToAll(cmsg, 3, RELEASE_TUGBOATS_TAG);
		holowniki += boatsReq;

		printf("[%d]: wyszedłem ze strefy krytycznej, oddaje %d holownikow\n", rank, boatsReq);
	}

	MPI_Finalize();
  return 0;
}
