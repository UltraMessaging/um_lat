/* um_lat_jitter.c */
/*
  Copyright (c) 2021-2022 Informatica Corporation
  Permission is granted to licensees to use or alter this software for any
  purpose, including commercial applications, according to the terms laid
  out in the Software License Agreement.

  This source code example is provided by Informatica for educational
  and evaluation purposes only.

  THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES 
  EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF 
  NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR 
  PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE 
  UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES,
  BE LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR 
  INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE 
  TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF 
  THE LIKELIHOOD OF SUCH DAMAGES.
*/

#include "cprt.h"
#include <stdio.h>
#include <string.h>
#if ! defined(_WIN32)
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <stdlib.h>
  #include <unistd.h>
#endif

#include "um_lat.h"


/* Command-line options and their defaults */
static int o_affinity_cpu = -1;
static char *o_group = NULL;
static char *o_histogram = NULL;  /* -H */
static char *o_interface = NULL;

/* Parameters parsed out from command-line options. */
int hist_num_buckets;
int hist_ns_per_bucket;
struct in_addr iface_in;
struct in_addr group_in;

char usage_str[] = "Usage: um_lat_jitter [-h] [-g group] [-a affinity_cpu] [-H hist_num_buckets,hist_ns_per_bucket] [-i interface]";

void usage(char *msg) {
  if (msg) fprintf(stderr, "%s\n", msg);
  fprintf(stderr, "%s\n", usage_str);
  exit(1);
}

void help() {
  fprintf(stderr, "%s\n", usage_str);
  fprintf(stderr, "where:\n"
      "  -h : print help\n"
      "  -a affinity_cpu : bitmap for CPU affinity for send thread [%d]\n"
      "  -g group : multicast group address [%s]\n"
      "  -H hist_num_buckets,hist_ns_per_bucket : send time histogram [%s]\n"
      "  -i interface : interface for multicast bind [%s]\n"
      , o_affinity_cpu, o_group, o_histogram, o_interface
  );
  exit(0);
}


void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  /* Set defaults for string options. */
  o_group = CPRT_STRDUP("");
  o_histogram = CPRT_STRDUP("0,0");
  o_interface = CPRT_STRDUP("");

  while ((opt = cprt_getopt(argc, argv, "ha:g:H:i:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': CPRT_ATOI(cprt_optarg, o_affinity_cpu); break;
      case 'g': free(o_group); o_group = CPRT_STRDUP(cprt_optarg); break;
      case 'H': free(o_histogram); o_histogram = CPRT_STRDUP(cprt_optarg); break;
      case 'i': free(o_interface); o_interface = CPRT_STRDUP(cprt_optarg); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  char *strtok_context;

  /* Parse the histogram option: "hist_num_buckets,hist_ns_per_bucket". */
  char *work_str = CPRT_STRDUP(o_histogram);
  char *hist_num_buckets_str = CPRT_STRTOK(work_str, ",", &strtok_context);
  ASSRT(hist_num_buckets_str != NULL);
  CPRT_ATOI(hist_num_buckets_str, hist_num_buckets);

  char *hist_ns_per_bucket_str = CPRT_STRTOK(NULL, ",", &strtok_context);
  ASSRT(hist_ns_per_bucket_str != NULL);
  CPRT_ATOI(hist_ns_per_bucket_str, hist_ns_per_bucket);

  ASSRT(CPRT_STRTOK(NULL, ",", &strtok_context) == NULL);
  free(work_str);
  /* It doesn't make sense to not use histogram with a latency tool. */
  ASSRT(hist_num_buckets > 0);
  ASSRT(hist_ns_per_bucket > 0);

  /* Parse the group option. */
  ASSRT(strlen(o_group) > 0);
  memset((char *)&group_in, 0, sizeof(group_in));
  ASSRT(inet_aton(o_group, &group_in) != 0);

  /* Parse the interface option. */
  ASSRT(strlen(o_interface) > 0);
  memset((char *)&iface_in, 0, sizeof(iface_in));
  ASSRT(inet_aton(o_interface, &iface_in) != 0);

  if (cprt_optind != argc) { usage("Unexpected positional parameter(s)"); }
}  /* get_my_opts */


/* Histogram. */
uint64_t *hist_buckets = NULL;
uint64_t hist_min_sample = 999999999;
uint64_t hist_max_sample = 0;
int hist_overflows = 0;  /* Number of values above the last bucket. */
int hist_num_samples = 0;
uint64_t hist_sample_sum = 0;

void hist_init()
{
  /* Re-initialize the data. */
  hist_min_sample = 999999999;
  hist_max_sample = 0;
  hist_overflows = 0;  /* Number of values above the last bucket. */
  hist_num_samples = 0;
  hist_sample_sum = 0;

  /* Init histogram (also makes sure it is mapped to physical memory. */
  int i;
  for (i = 0; i < hist_num_buckets; i++) {
    hist_buckets[i] = 0;
  }
}  /* hist_init */

void hist_create()
{
  hist_buckets = (uint64_t *)malloc(hist_num_buckets * sizeof(uint64_t));

  hist_init();
}  /* hist_create */

void hist_input(uint64_t in_sample)
{
  ASSRT(hist_buckets != NULL);

  hist_num_samples++;
  hist_sample_sum += in_sample;

  if (in_sample > hist_max_sample) {
    hist_max_sample = in_sample;
  }
  if (in_sample < hist_min_sample) {
    hist_min_sample = in_sample;
  }

  uint64_t bucket = in_sample / hist_ns_per_bucket;
  if (bucket >= hist_num_buckets) {
    hist_overflows++;
  }
  else {
    hist_buckets[bucket]++;
  }
}  /* hist_input */

/* Get the latency (in ns) which "percentile" percent of samples are below.
 * Returns -1 if not calculable (i.e. too many overflows). */
int hist_percentile(double percentile)
{
  int i;
  int needed_samples = (int)((double)hist_num_samples * percentile / 100.0);
  int found_samples = 0;

  for (i = 0; i < hist_num_buckets; i++) {
    found_samples += hist_buckets[i];
    if (found_samples > needed_samples) {
      return (i+1) * hist_ns_per_bucket;
    }
  }

  return -1.0;
}  /* hist_percentile */

void hist_print()
{
  int i;
  for (i = 0; i < hist_num_buckets; i++) {
    printf("%"PRIu64"\n", hist_buckets[i]);
  }
  printf("o_histogram=%s, hist_overflows=%d, hist_min_sample=%"PRIu64", hist_max_sample=%"PRIu64",\n",
      o_histogram, hist_overflows, hist_min_sample, hist_max_sample);
  uint64_t average_sample = hist_sample_sum / (uint64_t)hist_num_samples;
  printf("hist_num_samples=%d, average_sample=%d,\n",
      hist_num_samples, (int)average_sample);

  printf("Percentiles: 90=%d, 99=%d, 99.9=%d, 99.99=%d, 99.999=%d\n",
      hist_percentile(90.0), hist_percentile(99.0), hist_percentile(99.9),
      hist_percentile(99.99), hist_percentile(99.999));
}  /* hist_print */


void init_sock(int sock)
{
  CPRT_EOK0(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF,
      (char*)&iface_in, sizeof(iface_in)));

  char ttl = 15;
  CPRT_EOK0(setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
      (char *)&ttl, sizeof(ttl)));
}  /* init_sock */


/* Measure the minimum and maximum duration of a timestamp. */
void jitter_loop()
{
  uint64_t ts_min_ns = 999999999;
  uint64_t ts_max_ns = 0;
  struct timespec ts1;
  struct timespec ts2;

  struct timespec start_ts;
  CPRT_GETTIME(&start_ts);

  /* Warm up the cache. */
  CPRT_GETTIME(&ts1);
  CPRT_GETTIME(&ts2);
  CPRT_GETTIME(&ts1);
  CPRT_GETTIME(&ts2);

  uint64_t ts_this_ns = 0;;
  /* Loop for 2 seconds. */
  while (ts_this_ns < 2000000000) {
    /* Two timestamps in a row measures the duration of the timestamp. */
    CPRT_GETTIME(&ts1);

    CPRT_GETTIME(&ts2);

    CPRT_DIFF_TS(ts_this_ns, ts2, ts1);
    hist_input((int)ts_this_ns);
    /* Track maximum and minimum. */
    if (ts_this_ns < ts_min_ns) ts_min_ns = ts_this_ns;
    if (ts_this_ns > ts_max_ns) ts_max_ns = ts_this_ns;

    CPRT_DIFF_TS(ts_this_ns, ts2, start_ts);
  }

  hist_print();
  printf("ts_min_ns=%"PRIu64", ts_max_ns=%"PRIu64", \n",
      ts_min_ns, ts_max_ns);
}  /* jitter_loop */


/* Return number of sock writes accomplished. */
uint64_t sock_loop(int sock, uint64_t duration_ns)
{
  struct sockaddr_in dest_sin;
  struct msghdr message_hdr;
  struct iovec message_iov;
  struct timespec ts2;

  /* Set up destination group:port. */
  memset(&dest_sin, 0, sizeof(dest_sin));
  dest_sin.sin_family = AF_INET;
  dest_sin.sin_addr.s_addr = group_in.s_addr;
  dest_sin.sin_port = htons(12000);

  /* Set up outgoing message buffer. */
  message_iov.iov_base = "um_lat_jitter";
  message_iov.iov_len = strlen(message_iov.iov_base);

  /* Set up call to sendmsg(). */
  message_hdr.msg_name = &dest_sin;
  message_hdr.msg_namelen = sizeof(dest_sin);
  message_hdr.msg_iov = &message_iov;
  message_hdr.msg_iovlen = 1;
  message_hdr.msg_control = NULL;
  message_hdr.msg_controllen = 0;
  message_hdr.msg_flags = 0;

  struct timespec start_ts;
  CPRT_GETTIME(&start_ts);

  /* Warm up the cache. */
  CPRT_GETTIME(&ts2);
  CPRT_GETTIME(&ts2);

  uint64_t ts_this_ns = 0;
  uint64_t num_sends = 0;
  /* Loop for 2 seconds. */
  while (ts_this_ns < duration_ns) {
    CPRT_EM1(sendmsg(sock, &message_hdr, 0));
    CPRT_EM1(sendmsg(sock, &message_hdr, 0));
    num_sends += 2;

    CPRT_GETTIME(&ts2);
    CPRT_DIFF_TS(ts_this_ns, ts2, start_ts);
  }

  return num_sends;
}  /* sock_loop */


int main(int argc, char **argv)
{
  int sock;
  uint64_t cpuset;
  CPRT_NET_START;

  CPRT_INITTIME();

  get_my_opts(argc, argv);

  if (hist_num_buckets > 0) {
    hist_create();
  }

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_cpu=%d, o_histogram=%s, \n",
      o_affinity_cpu, o_histogram);

  sock = socket(PF_INET,SOCK_DGRAM,0);
  ASSRT(sock != -1);
  init_sock(sock);

  if (o_affinity_cpu > -1) {
    CPRT_CPU_ZERO(&cpuset);
    CPRT_CPU_SET(o_affinity_cpu, &cpuset);
    cprt_set_affinity(cpuset);
    jitter_loop();
  }
  else {
    int max_loops = 0;
    int max_cpu = -1;
    int cpu;
    for (cpu = 0; cpu < 64; cpu++) {  /* Can't go higher than 64. */
      CPRT_CPU_ZERO(&cpuset);
      CPRT_CPU_SET(cpu, &cpuset);
      int err = cprt_try_affinity(cpuset);
      if (err) {
        break;
      }

      uint64_t num_sends = sock_loop(sock, 100000000);  /* .1 sec "warmup". */
      num_sends = sock_loop(sock, 1000000000);  /* 1 sec test. */
      printf("cpu=%d, num_sends=%"PRIu64"\n", cpu, num_sends);
      if (num_sends > max_loops) {
        max_loops = num_sends;
        max_cpu = cpu;
      }
    }  /* for */

    CPRT_CPU_ZERO(&cpuset);
    CPRT_CPU_SET(max_cpu, &cpuset);
    cprt_set_affinity(cpuset);

    jitter_loop();
  }


  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
