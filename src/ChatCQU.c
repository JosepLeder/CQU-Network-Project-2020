#include <pthread.h>
#include <signal.h>
#include <stdio.h>

#include "send.h"
#include "recv.h"

pthread_t rcv_t;

int main(int argc, char *argv[])
{

	if (argc < 2)
    {
        fprintf(stderr, "ERROR, no port provided\n");
        exit(1);
    }
	short src = atoi(argv[1]);
	short dst = atoi(argv[2]);

	printf("Welcome to ChatCQU!\nPress CRTL + C to exit.\n");
	printf("Please input your messages:\n");
	pthread_create(&rcv_t, NULL, receiver, (void*)&src);
	pthread_detach(rcv_t);
	sender(src, dst);
	return 0; 
}
