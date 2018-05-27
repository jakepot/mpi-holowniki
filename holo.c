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
#define IMBEFOREYOU 1

// msg[0] = timestamp
// msg[1] = rank
// msg[2] = boatsReq

int inQueue = 0;
int holowniki = 15;
int position = -1;
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
		int reply[1];
		switch (status.MPI_TAG) {
			case 0:
				// mozna trzymac w kolejce senderow i potem ich usuwac z listy
				if (inQueue) {
					if (timestamp < msg[0] || timestamp == msg[0] && rank < msg[1]) {
						reply[0] = IMBEFOREYOU;
					}
				}
				else {
					reply[0] = GO_AHEAD;
				}
				MPI_Send(reply, 1, MPI_LONG, status.MPI_SOURCE, REPLY_TAG, MPI_COMM_WORLD);
				break;
			case 1:
				holowniki -= msg[2];
				if (inQueue)
					position -= 1;
				// wiadomosc o wyjsciu z kolejki jesli wychodzi
				break;
			case 2:
				holowniki += msg[2];
				break;
			case 3:
				if (msg[0] == IMBEFOREYOU)
					position += 1;
				// else
					// ;
				repliesRemaining -= 1;
				// printf("Dostalem odpowiedz od %d\n", status.MPI_SOURCE);
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
	int msg[MSG_SIZE];
	MPI_Status status;
	pthread_t tid;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank( MPI_COMM_WORLD, &rank );
	MPI_Comm_size( MPI_COMM_WORLD, &size );

	MPI_Barrier(MPI_COMM_WORLD);
	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	clock_settime(CLOCK_MONOTONIC, &ts);
	srand(time(NULL)+rank);

	boatsReq = rand()%7 + 1;
	printf("Statek %d, potrzebuje %d holownikow\n", rank, boatsReq);
	pthread_create(&tid, NULL, recvFun, NULL);

	while(1){
		// usleep(1000*(rand()%100));
		usleep(100000*(rand()%100+5));
		inQueue = 1;
		struct timespec tst;
		clock_gettime(CLOCK_MONOTONIC, &tst);
		timestamp = 1000 * tst.tv_sec + (tst.tv_nsec / 1000000);
		long msg[2];
		msg[0] = timestamp;
		msg[1] = rank;
		printf("%d chce wpłynąć do portu.\n", rank);
		// printf("Statek %d wysyła request time: %ld\n", rank, timestamp);
		repliesRemaining = size - 1;
		position = 0;
		sendToAll(msg, 2, REQUEST_TAG);
		while(repliesRemaining != 0){
			usleep(10000);
		}
		printf("%d : %d w kolejce\n", rank, position);
		while(position > 0){
			usleep(10000);
		}
		printf("%d : %d holownikow dostepnych\n", rank, holowniki);
		while(holowniki < boatsReq){
			usleep(10000);
		}
		inQueue = 0;
		long cmsg[3];
		msg[0] = timestamp;
		msg[1] = rank;
		msg[2] = boatsReq;
		// wejscie do strefy krytycznej
		holowniki -= boatsReq;
		sendToAll(msg, 3, TAKE_TUGBOATS_TAG);
		sleep(5); // strefa krytyczna
		sendToAll(msg, 3, RELEASE_TUGBOATS_TAG);
		holowniki += boatsReq;

		printf("%d, wpłynąłem\n", rank);
	}

	MPI_Finalize();
  return 0;
}
