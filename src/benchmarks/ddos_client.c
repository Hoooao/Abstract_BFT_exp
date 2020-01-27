#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>
#include <sys/param.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>

#include "th_assert.h"
//#include "Timer.h"
#include "libmodular_BFT.h"

#include "benchmarks.h"

typedef struct {
    int type;
    int id;
    int node_id;
    int size;
    int payload; //size of the payload
    // rest is the payload;
    } ddos_req;

static long received_total = 0;
static int do_run = 1;

int sock = -1;
int request_size = -1;
struct sockaddr* sa_server_address = NULL;

int ddos_send(ddos_req *m, int *len);
struct timeval tv_diff(const struct timeval tv1, const struct timeval tv2);

int main(int argc, char **argv)
{
	char host_name[MAXHOSTNAMELEN+1];
	char host_addr[MAXHOSTNAMELEN+1];
	char master_host_name[MAXHOSTNAMELEN+1];
	char server_address[MAXHOSTNAMELEN+1];

	short port;
	int num_bursts;
	int num_messages_per_burst;
	int num_participants;
	int node_id;

	int argNb=1;

	strcpy(host_name, argv[argNb++]);
	strcpy(host_addr, argv[argNb++]);
	node_id = atoi(argv[argNb++]);
	num_participants = atoi(argv[argNb++]);
	strcpy(master_host_name, argv[argNb++]);
	strcpy(server_address, argv[argNb++]);
	port = atoi(argv[argNb++]);
	num_bursts = atoi(argv[argNb++]);
	num_messages_per_burst = atoi(argv[argNb++]);
	request_size = atoi(argv[argNb++]);
	double rate = atof(argv[argNb++]); // rate represents desired rate in req/s
	const double delay = 1/rate;
	const long delay_ns = (long)(delay*1e9);

	fprintf(stderr, "******************************\n* multicast test\n****************************************\n");
	fprintf(stderr, "Data is:\n* hostname = %s\n* node_id = %d\n* master_host_name = %s\n* server address = %s\n* port = %d\n* num_bursts = %d\n* msgs per burst = %d\n* request size = %d\n* rate = %g\n", host_name, node_id, master_host_name, server_address, port, num_bursts, num_messages_per_burst, request_size, rate);

	char hname[MAXHOSTNAMELEN];
	gethostname(hname, MAXHOSTNAMELEN);

	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(server_address);
	server_addr.sin_port = htons(port);
	sa_server_address = (struct sockaddr*)&server_addr;

	// Initialize socket.
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	int flag_on = 1;
	/* set reuse port to on to allow multiple binds per host */
	if ((setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flag_on,
           sizeof(flag_on))) < 0) {
	       perror("setsockopt() failed");
	           exit(1);
	}

	/*int error = bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr));*/
	/*if (error < 0)*/
	/*{*/
		/*perror("Unable to name socket");*/
		/*exit(1);*/
	/*}*/

	// Create socket to communicate with manager
	int manager;
	if ((manager = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		th_fail("Could not create socket");
	}

	// Fill-in manager address
	Address desta;
	bzero((char *)&desta, sizeof(desta));

	struct hostent *hent = gethostbyname(master_host_name);
	if (hent == 0)
	{
		th_fail("Could not get hostent");
	}
	// desta.sin_addr.s_addr = inet_addr("192.168.20.6"); // sci6
	desta.sin_addr.s_addr = ((struct in_addr*)hent->h_addr_list[0])->s_addr;
	desta.sin_family = AF_INET;
	desta.sin_port = htons(MANAGER_PORT);
	if (connect(manager, (struct sockaddr *) &desta, sizeof(desta)) == -1)
	{
		th_fail("Could not connect name to socket");
	}

	thr_command out, in;

// Tell manager we are up
	if (send(manager, &out, sizeof(out), 0) < sizeof(out))
	{
		fprintf(stderr, "Problem with send to manager\n");
		exit(-1);
	}

	fprintf(stderr, "Starting the bursts (num_bursts = %d)\n", num_bursts);

	int req_cnt = 0;

	ddos_req *req = (ddos_req*)malloc(request_size);
	req->type = 0xdeadbeef;
	req->id = req_cnt;
	req->node_id = node_id;
	req->size = request_size;
	req->payload = req->size - sizeof(ddos_req);


	fprintf(stderr,"\n");
	for (int i = 0; i < num_bursts; i++)
	{
		char *data = (char*)&in;
		int ssize = sizeof(in);
		int ret = 0;
		while (ssize) {
		    ret = recv(manager, data, ssize, MSG_WAITALL);
		    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
			continue;
		    } else if (ret < 0) {
			fprintf(stderr, "Error receiving msg from manager\n");
			perror(NULL);
			exit(1);
		    }
		    ssize -= ret;
		    data += ret;
		}
		fprintf(stderr, "Starting burst #%d\n", i);
		//char a=getchar();
		// Loop invoking requests
		// int j=0;
		// while(true){
		//  j++
		int s = 0;
		/*s = in.num_iter;*/
		int j;

		struct timespec ts_obt;
		struct timespec ts_fire;
		struct timeval tv;
		gettimeofday(&tv, NULL);
		for (j = s; j < num_messages_per_burst; j++)
		{
			/*fprintf(stderr, "Send a new message\n");*/
			int retval = 0;

			int len = 0;
			req->id = req_cnt++;
			if (j == s) {
			    clock_gettime(CLOCK_REALTIME, &ts_obt);
			}
			retval = ddos_send(req, &len);
			if (retval == -1)
			{
			    fprintf(stderr, "multicast_replica: problem invoking request\n");
			} else {
						    /*fprintf(stderr, "Sent one\n");*/
			    if (j%1000 == 0)
				fprintf(stderr, ".");
			}
			{
			    struct timespec ts_remain;
			    ts_fire.tv_sec = ts_obt.tv_sec;
			    ts_fire.tv_nsec = ts_obt.tv_nsec + delay_ns;
			    while (ts_fire.tv_nsec >= 1000000000) {
				ts_fire.tv_nsec -= 1000000000;
				ts_fire.tv_sec += 1;
			    }
			    if (clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME,
				    &ts_fire, &ts_remain) == -1 && errno == EINTR)
			    {
				struct timespec ts_rem = ts_remain;
				while (clock_nanosleep(CLOCK_REALTIME, 0, &ts_remain, &ts_rem)==-1
					&& errno == EINTR)
				{
					ts_remain = ts_rem;
				}
			    }
			    ts_obt = ts_fire;
			}
		}
		fprintf(stderr,"\n");
		struct timeval tv_end;
		gettimeofday(&tv_end, NULL);
		struct timeval timediff;
		timediff = tv_diff(tv_end, tv);
		/*fprintf(stderr, "[Node %s] Finished burst %d: total time %d s, %d us\n", host_name, i,*/
			/*timediff.tv_sec, timediff.tv_usec);*/
		//
		out.num_iter = num_messages_per_burst;
		if (send(manager, &out, sizeof(out), 0) <= 0)
		{
			fprintf(stderr, "Sendto failed");
			exit(-1);
		}
	}
	free(req);
	shutdown(manager, SHUT_RDWR);
	close(manager);
	sleep(4);
	do_run = 0;
	sleep(1);
	return 0;
}

struct timeval tv_diff(const struct timeval tv1, const struct timeval tv2)
{
	struct timeval tv;
	tv.tv_sec = tv1.tv_sec - tv2.tv_sec;
	tv.tv_usec = tv1.tv_usec - tv2.tv_usec;
	while (tv.tv_usec < 0) {
	    tv.tv_sec--;
	    tv.tv_usec += 1000000;
	}
	return tv;
}

int ddos_send(ddos_req *m, int *len)
{
	int total = 0; // how many bytes we've sent
	int bytesleft = m->size; // how many we have left to send
	*len = m->size;
	int n = -1;

	while (total < *len)
	{
		n = sendto(sock, (char*)m + total, bytesleft, 0, sa_server_address, sizeof(struct sockaddr));
		if (n == -1 && (errno != EAGAIN || errno != EWOULDBLOCK))
		{
		    perror(NULL);
			return -1;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; // return number actually sent here

	return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}
