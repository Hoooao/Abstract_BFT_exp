#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/param.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include "th_assert.h"
#include "libbyz.h"
#include "Timer.h"

#include "benchmarks.h"
#include "incremental_stats.h"

int main(int argc, char **argv)
{
	int num_clients;
	int num_bursts;
	int num_messages_per_burst;
	int num_malicious = 0;
	float limit_thr = 100000.0;

	int argNb=1;

	num_clients = atoi(argv[argNb++]);
	num_bursts = atoi(argv[argNb++]);
	num_messages_per_burst = atoi(argv[argNb++]);
	limit_thr = atof(argv[argNb++]);
	limit_thr /= num_clients;
	if (argNb < argc) {
	    num_malicious = atoi(argv[argNb++]);
	}

	// Priting parameters
	fprintf(stderr, "********************************************\n");
	fprintf(stderr, "*       Throughput bench parameters        *\n");
	fprintf(stderr, "********************************************\n");
	fprintf(stderr,
	"Nb clients = %d\nNb bursts = %d \nNb messages per burst = %d\nNb malicious = %d\nLimit thr = %.2f\n",
	num_clients, num_bursts, num_messages_per_burst, num_malicious, limit_thr);
	fprintf(stderr, "********************************************\n\n");

	Address client_addrs[num_clients];
	int clients;
	incr_stats throughput_stats;
	incr_stats rt_stats;

	/* Create socket to communicate with clients */
	if ((clients = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		th_fail("Could not create socket");
	}

	Address a;
	bzero((char *)&a, sizeof(a));
	a.sin_addr.s_addr = INADDR_ANY;
	a.sin_family = AF_INET;
	a.sin_port = htons(MANAGER_PORT);
	if (bind(clients, (struct sockaddr *) &a, sizeof(a)) == -1)
	{
		perror(NULL);
		th_fail("Could not bind name to socket");
	}

	if (listen(clients, 1024) < 0) {
	    th_fail("Could not listen on socket");
	}

	int *clientfds = (int*)malloc(num_clients*sizeof(int));
	if (clientfds == NULL) {
	    th_fail("Could not allocate clientfds");
	}

	thr_command out, in;
	int i;
	fprintf(stderr, "Will wait for all clients\n");
	for (i=0; i < num_clients; i++)
	{
		size_t addr_len = sizeof(a);

		int confd = accept(clients, (struct sockaddr*)&a, &addr_len);
		if (confd < 0) {
		    fprintf(stderr, "problem with accept, retrying %d, %d\n", errno, i);
		    perror("THis is the problem\n");
		    i--;
		    continue;
		}
		clientfds[i] = confd;

		char *data = (char*)&in;
		int ssize = sizeof(in);
		int ret = 0;
		while (ssize) {
		    ret = recv(confd, data, ssize, MSG_WAITALL);
		    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
		    } else if (ret < 0) {
			fprintf(stderr, "Error receiving handshake from client, at %d/%d\n", i+1, num_clients);
			exit(1);
		    } else {
			ssize -= ret;
			data += ret;
		    }
		}

		client_addrs[i] = a;
	}
	fprintf(stderr, "All ready\n");

    // wait for the clients to be properly connected to the replicas
    sleep(5);

	init_stats(&throughput_stats);
	init_stats(&rt_stats);

	Timer t;

	for (i = 0; i < num_bursts; i++)
	{
		t.reset();
		t.start();

		/* Send start order */
		// for (int j=0; j < num_clients; j++)
		int j;
		for (j=0; j < 1; j++)
		{
			if (j < num_malicious)
			    out.malicious = 1;
			else
			    out.malicious = 0;
			if (j>0)
			{
				out.num_iter = q_panic_thresh-1;
				out.num_iter = 0;
			} else
			{
				out.num_iter=0;
			}
			out.avg_thr = limit_thr;
			if (send(clientfds[j], &out, sizeof(out), 0) <= 0)
			{
				th_fail("Send");
			}
		}
		for (j=0; j < 1; j++)
		{
			//int ret = recvfrom(clients, &in, sizeof(in), 0, 0, 0);
			//if (ret != sizeof(in))
			//{
			//   th_fail("Invalid message");
			//}
		}

		for (j=1; j < num_clients; j++)
		{
			if (j < num_malicious)
			    out.malicious = 1;
			else
			    out.malicious = 0;
			if (j>0)
			{
				out.num_iter = q_panic_thresh-1;
				out.num_iter=0;
			} else
			{
				out.num_iter=0;
			}
      out.avg_thr = limit_thr;
			if (send(clientfds[j], &out, sizeof(out), 0) <= 0)
			{
				th_fail("Send");
			}
		}


		/* Wait for results */
		int sent_messages = 0;
        double sum_throughput = 0.0;
		for (j=0; j < num_clients; j++)
		{
			char *data = (char*)&in;
			int ssize = sizeof(in);
			int ret = 0;
			while (ssize) {
			    ret = recv(clientfds[j], data, ssize, MSG_WAITALL);
			    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
				continue;
			    } else if (ret < 0) {
				fprintf(stderr, "Error receiving handshake from client, at %d/%d\n", i+1, num_clients);
				exit(1);
			    }
			    ssize -= ret;
			    data += ret;
			}
			if (in.num_iter != num_messages_per_burst)
			{
				//        ret = recvfrom(clients, &in, sizeof(in), 0, 0, 0);

			}
			if (ret != sizeof(in))
			{
				th_fail("Invalid message");
			}
			if (!in.malicious) {
			    sent_messages += in.num_iter;
                sum_throughput += in.avg_thr;
			    add(&rt_stats, in.avg_rt);
			}
		}
		t.stop();

		//		double throughput = ((double) (num_messages_per_burst- history_length)
		//				* num_clients) / t.elapsed();
		//		double throughput = ((double)(((num_messages_per_burst- q_panic_thresh)
		//				* (num_clients -1))+ num_messages_per_burst)) / t.elapsed();
		double throughput = ((double) sent_messages)
				/ t.elapsed();
        fprintf(stderr, "Burst %d, I measure %10.6g ops/ while the clients say %10.6g ops/s\n", i, throughput, sum_throughput);

		add(&throughput_stats, throughput);
		//add(&throughput_stats, sum_throughput);
		//fprintf(stderr, "End of burst\n");
		fprintf(stderr, "Burst %d: throughput : %10.6g op/s avg = %10.6g std dev = %10.6g (elapsed time = %f),%d\n", i,
		throughput, get_avg(&throughput_stats),
		get_std_dev(&throughput_stats), t.elapsed(),in.num_iter);
		fprintf(stderr, "\tresponse time: avg = %10.6e std dev = %10.6e\n", get_avg(&rt_stats), get_std_dev(&rt_stats));

		/* Separate out experiments to improve independence */
		sleep(8);

	}
	for (int j=0; j < num_clients; j++)
	{
	    shutdown(clientfds[j], SHUT_RDWR);
	    close(clientfds[j]);
	}

	shutdown(clients, SHUT_RDWR);
	sleep(2);
	close(clients);
	fprintf(stderr, "thr_manager exiting\n");
}

