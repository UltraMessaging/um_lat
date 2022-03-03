/* um_lat_ping.c - performance measurement tool. */
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
  #include <stdlib.h>
  #include <unistd.h>
#endif

#include "lbm/lbm.h"
#include "um_lat.h"

/* Forward declarations. */
lbm_xsp_t *my_xsp_mapper_callback(lbm_context_t *ctx, lbm_new_transport_info_t *transp_info, void *clientd);
int rcv_callback(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd);


/* Command-line options and their defaults. String defaults are set
 * in "get_my_opts()".
 */
static int o_affinity_src = -1;  /* -A */
static int o_affinity_rcv = -1;
static char *o_config = NULL;
static int o_generic_src = 0;
static char *o_histogram = NULL;  /* -H */
static int o_linger_ms = 1000;
static int o_msg_len = 0;
static int o_num_msgs = 0;
static char *o_persist_mode = NULL;
static char *o_rcv_thread = NULL; /* -R */
static int o_rate = 0;
static char *o_spin_method = NULL;
static char *o_warmup = NULL;
static char *o_xml_config = NULL;

/* Parameters parsed out from command-line options. */
char *app_name;
int hist_num_buckets;
int hist_ns_per_bucket;
int warmup_loops;
int warmup_rate;

/* Globals. The code depends on the loader initializing them to all zeros. */
int msg_flags;
char *msg_buf;
perf_msg_t *perf_msg;
int global_max_tight_sends;
int registration_complete;
int cur_flight_size;
int max_flight_size;


char usage_str[] = "Usage: um_lat_ping [-h] [-A affinity_src] [-a affinity_rcv] [-c config] [-g] [-H hist_num_buckets,hist_ns_per_bucket] [-l linger_ms] [-m msg_len] [-n num_msgs] [-p persist_mode] [-R rcv_thread] [-r rate] [-s spin_method] [-w warmup_loops,warmup_rate] [-x xml_config]";

void usage(char *msg) {
  if (msg) fprintf(stderr, "%s\n", msg);
  fprintf(stderr, "%s\n", usage_str);
  CPRT_NET_CLEANUP;
  exit(1);
}

void help() {
  fprintf(stderr, "%s\n", usage_str);
  fprintf(stderr, "where:\n"
      "  -h : print help\n"
      "  -A affinity_src : CPU number (0..N-1) for send thread (-1=none) [%d]\n"
      "  -a affinity_rcv : CPU number (0..N-1) for receive thread (-1=none) [%d]\n"
      "  -c config : configuration file; can be repeated [%s]\n"
      "  -g : generic source [%d]\n"
      "  -H hist_num_buckets,hist_ns_per_bucket : send time histogram [%s]\n"
      "  -l linger_ms : linger time before source delete [%d]\n"
      "  -m msg_len : message length [%d]\n"
      "  -n num_msgs : number of messages to send [%d]\n"
      "  -p persist_mode : '' (empty)=streaming, 'r'=RPP, 's'=SPP [%s]\n"
      "  -R rcv_thread : '' (empty)=main context, 'x'=XSP [%s]\n"
      "  -r rate : messages per second to send [%d]\n"
      "  -s spin_method : '' (empty)=no spin, 'f'=fd mgt busy [%s]\n"
      "  -w warmup_loops,warmup_rate : messages to send before measurement [%s]\n"
      "  -x xml_config : XML configuration file [%s]\n"
      , o_affinity_src, o_affinity_rcv, o_config, o_generic_src, o_histogram
      , o_linger_ms, o_msg_len, o_num_msgs, o_persist_mode, o_rcv_thread, o_rate
      , o_spin_method , o_warmup, o_xml_config
  );
  CPRT_NET_CLEANUP;
  exit(0);
}


/* Process command-line options. */
void get_my_opts(int argc, char **argv)
{
  int opt;  /* Loop variable for getopt(). */

  /* Set defaults for string options. */
  o_config = CPRT_STRDUP("");
  o_histogram = CPRT_STRDUP("0,0");
  o_persist_mode = CPRT_STRDUP("");
  o_rcv_thread = CPRT_STRDUP("");
  o_spin_method = CPRT_STRDUP("");
  o_warmup = CPRT_STRDUP("0,0");
  o_xml_config = CPRT_STRDUP("");

  while ((opt = getopt(argc, argv, "hA:a:c:gH:l:L:m:n:p:R:r:s:w:x:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'A': CPRT_ATOI(optarg, o_affinity_src); break;
      case 'a': CPRT_ATOI(optarg, o_affinity_rcv); break;
      /* Allow -c to be repeated, loading each config file in succession. */
      case 'c': free(o_config);
                o_config = CPRT_STRDUP(optarg);
                E(lbm_config(o_config));
                break;
      case 'g': o_generic_src = 1; break;
      case 'H': free(o_histogram); o_histogram = CPRT_STRDUP(optarg); break;
      case 'l': CPRT_ATOI(optarg, o_linger_ms); break;
      case 'm': CPRT_ATOI(optarg, o_msg_len); break;
      case 'n': CPRT_ATOI(optarg, o_num_msgs); break;
      case 'p': free(o_persist_mode); o_persist_mode = CPRT_STRDUP(optarg); break;
      case 'R': free(o_rcv_thread); o_rcv_thread = CPRT_STRDUP(optarg); break;
      case 'r': CPRT_ATOI(optarg, o_rate); break;
      case 's': free(o_spin_method); o_spin_method = CPRT_STRDUP(optarg); break;
      case 'w': free(o_warmup); o_warmup = CPRT_STRDUP(optarg); break;
      case 'x': free(o_xml_config); o_xml_config = CPRT_STRDUP(optarg); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  /* Must supply certain required "options". */
  ASSRT(o_rate > 0);
  ASSRT(o_num_msgs > 0);
  ASSRT(o_msg_len >= sizeof(perf_msg_t));

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

  if ((strcmp(o_persist_mode, "") != 0) && (strcmp(o_persist_mode, "r") != 0) && (strcmp(o_persist_mode, "s") != 0)) {
    usage("Error, -p value must be '', 'r', or 's'\n");
  }
  if (o_persist_mode[0] == '\0') {
    app_name = "um_perf";
  }
  else if (o_persist_mode[0] == 'r') {
    app_name = "um_perf_rpp";
  }
  else {
    ASSRT(o_persist_mode[0] == 's');
    app_name = "um_perf_spp";
  }

  if ((strcmp(o_rcv_thread, "") != 0) && (strcmp(o_rcv_thread, "x") != 0)) {
    usage("Error, -R value must be '' or 'x'\n");
  }

  if ((strcmp(o_spin_method, "") != 0) && (strcmp(o_spin_method, "f") != 0)) {
    usage("Error, -s value must be '' or 'f'\n");
  }

  /* Parse the warmup option: "warmup_loops,warmup_rate". */
  work_str = CPRT_STRDUP(o_warmup);
  char *warmup_loops_str = CPRT_STRTOK(work_str, ",", &strtok_context);
  ASSRT(warmup_loops_str != NULL);
  CPRT_ATOI(warmup_loops_str, warmup_loops);

  char *warmup_rate_str = CPRT_STRTOK(NULL, ",", &strtok_context);
  ASSRT(warmup_rate_str != NULL);
  CPRT_ATOI(warmup_rate_str, warmup_rate);

  ASSRT(CPRT_STRTOK(NULL, ",", &strtok_context) == NULL);
  free(work_str);
  if (warmup_loops > 0) { ASSRT(warmup_rate > 0); }

  if (strlen(o_xml_config) > 0) {
    /* Unlike lbm_config(), you can't load more than one XML file.
     * If user supplied -x more than once, only load last one. */
    E(lbm_config_xml_file(o_xml_config, app_name));
  }

  if (optind != argc) { usage("Unexpected positional parameter(s)"); }
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


/* Process source event. */
int handle_src_event(int event, void *extra_data, void *client_data)
{
  switch (event) {
    case LBM_SRC_EVENT_CONNECT:
      break;
    case LBM_SRC_EVENT_DISCONNECT:
      break;
    case LBM_SRC_EVENT_WAKEUP:
      break;
    case LBM_SRC_EVENT_UME_REGISTRATION_ERROR:
      break;
    case LBM_SRC_EVENT_UME_STORE_UNRESPONSIVE:
      break;
    case LBM_SRC_EVENT_UME_REGISTRATION_SUCCESS_EX:
      break;
    case LBM_SRC_EVENT_UME_REGISTRATION_COMPLETE_EX:
      registration_complete++;
      break;
    case LBM_SRC_EVENT_UME_MESSAGE_STABLE_EX:
      __sync_fetch_and_sub(&cur_flight_size, 1);
      ASSRT(cur_flight_size >= 0);  /* Die if negative. */
      break;
    case LBM_SRC_EVENT_SEQUENCE_NUMBER_INFO:
      break;
    case LBM_SRC_EVENT_FLIGHT_SIZE_NOTIFICATION:
      break;
    case LBM_SRC_EVENT_UME_MESSAGE_RECLAIMED_EX:
      break;
    case LBM_SRC_EVENT_UME_DEREGISTRATION_SUCCESS_EX:
      break;
    case LBM_SRC_EVENT_UME_DEREGISTRATION_COMPLETE_EX:
      break;
    case LBM_SRC_EVENT_UME_MESSAGE_NOT_STABLE:
      break;
    default:
      fprintf(stderr, "handle_src_event: unexpected event %d\n", event);
  }

  return 0;
}  /* handle_src_event */

/* Callback for UM smart source events. */
int ssrc_event_cb(lbm_ssrc_t *ssrc, int event, void *extra_data, void *client_data)
{
  handle_src_event(event, extra_data, client_data);

  return 0;
}  /* ssrc_event_cb */

/* Callback for UM source events. */
int src_event_cb(lbm_src_t *src, int event, void *extra_data, void *client_data)
{
  handle_src_event(event, extra_data, client_data);

  return 0;
}  /* src_event_cb */


/* UM callback for force reclaiom events. */
int force_reclaim_cb(const char *topic_str, lbm_uint_t seqnum, void *clientd)
{
  fprintf(stderr, "force_reclaim_cb: topic_str='%s', seqnum=%d, cur_flight_size=%d, max_flight_size=%d,\n",
      topic_str, seqnum, cur_flight_size, max_flight_size);

  __sync_fetch_and_sub(&cur_flight_size, 1);  /* Adjust flight size for reclaim. */
  ASSRT(cur_flight_size >= 0);  /* Die if negative. */

  return 0;
}  /* force_reclaim_cb */


lbm_context_t *ctx = NULL;
lbm_xsp_t *my_xsp = NULL;

void create_context()
{
  /* Create UM context. */
  lbm_context_attr_t *ctx_attr;
  E(lbm_context_attr_create(&ctx_attr));

  lbm_transport_mapping_func_t mapping_func;
  if (o_rcv_thread[0] == 'x') {
    /* Set all receive transport sessions to my_xsp. */
    mapping_func.mapping_func = my_xsp_mapper_callback;
    mapping_func.clientd = &my_xsp;
    E(lbm_context_attr_setopt(ctx_attr,
        "transport_mapping_function", &mapping_func, sizeof(mapping_func)));
  }
  else {  /* rcv_thread not xsp. */
    /* Main context will host receivers; set desired options. */
    E(lbm_context_attr_str_setopt(ctx_attr,
        "ume_session_id", "0x6"));  /* Ping uses session ID 6. */
    if (o_spin_method[0] == 'f') {
      E(lbm_context_attr_str_setopt(ctx_attr,
          "file_descriptor_management_behavior", "busy_wait"));
    }
  }

  /* Context thread inherits the initial CPU set of the process. */
  E(lbm_context_create(&ctx, ctx_attr, NULL, NULL));
  E(lbm_context_attr_delete(ctx_attr));

  if (o_rcv_thread[0] == 'x') {
    /* Xsp in use; create a fresh context attr (can't re-use parent's). */
    E(lbm_context_attr_create(&ctx_attr));
    /* Main context will host receivers; set desired options. */
    E(lbm_context_attr_str_setopt(ctx_attr,
        "ume_session_id", "0x6"));  /* Ping uses session ID 6. */
    if (o_spin_method[0] == 'f') {
      E(lbm_context_attr_str_setopt(ctx_attr,
          "file_descriptor_management_behavior", "busy_wait"));
    }

    E(lbm_xsp_create(&my_xsp, ctx, ctx_attr, NULL));
    E(lbm_context_attr_delete(ctx_attr));
  }

}  /* create_context */


lbm_src_t *src;  /* Used if o_generic_src is 1. */
lbm_ssrc_t *ssrc;  /* Used if o_generic_src is 0. */
char *ssrc_buff;

void create_source(lbm_context_t *ctx)
{
  lbm_src_topic_attr_t *src_attr;
  lbm_topic_t *topic_obj;

  /* Set some options in code. */
  E(lbm_src_topic_attr_create(&src_attr));

  /* Get notified for forced reclaims (should not happen). */
  lbm_ume_src_force_reclaim_func_t force_reclaim_cb_conf;
  force_reclaim_cb_conf.func = force_reclaim_cb;
  force_reclaim_cb_conf.clientd = NULL;
  E(lbm_src_topic_attr_setopt(src_attr, "ume_force_reclaim_function",
      &force_reclaim_cb_conf, sizeof(force_reclaim_cb_conf)));

  /* The "ping" program sends messages to "topic1". */
  E(lbm_src_topic_alloc(&topic_obj, ctx, "topic1", src_attr));
  if (o_generic_src) {
    E(lbm_src_create(&src, ctx, topic_obj,
        src_event_cb, NULL, NULL));
    perf_msg = (perf_msg_t *)msg_buf;  /* Set up perf_msg once. */
  }
  else {  /* Smart Src API. */
    E(lbm_ssrc_create(&ssrc, ctx, topic_obj,
        ssrc_event_cb, NULL, NULL));
    E(lbm_ssrc_buff_get(ssrc, &ssrc_buff, 0));
    /* Set up perf_msg before each send. */
  }

  E(lbm_src_topic_attr_delete(src_attr));
}  /* create_source */


void delete_source()
{
  if (o_generic_src) {  /* If using smart src API */
    E(lbm_src_delete(src));
  }
  else {
    E(lbm_ssrc_buff_put(ssrc, ssrc_buff));
    E(lbm_ssrc_delete(ssrc));
  }
}  /* delete_source */


lbm_rcv_t *rcv;

void create_receiver(lbm_context_t *ctx)
{
  lbm_rcv_topic_attr_t *rcv_attr;
  E(lbm_rcv_topic_attr_create(&rcv_attr));

  /* Receive reflected messages from pong. */
  lbm_topic_t *topic_obj;
  E(lbm_rcv_topic_lookup(&topic_obj, ctx, "topic2", rcv_attr));
  E(lbm_rcv_create(&rcv, ctx, topic_obj, rcv_callback, NULL, NULL));
}  /* create_receiver */


static uint64_t num_rcv_msgs;
static uint64_t num_rx_msgs;
static uint64_t num_unrec_loss;
/* UM callback for receiver events, including received messages. */
int rcv_callback(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  struct timespec rcv_ts;
  CPRT_GETTIME(&rcv_ts);

  switch (msg->type) {
  case LBM_MSG_BOS:
    /* Assume receive thread is calling this; pin the time-critical thread
     * to the requested CPU. */
    if (o_affinity_rcv > -1) {
      uint64_t cpuset;
      CPRT_CPU_ZERO(&cpuset);
      CPRT_CPU_SET(o_affinity_rcv, &cpuset);
      cprt_set_affinity(cpuset);
    }

    num_rcv_msgs = 0;
    num_rx_msgs = 0;
    num_unrec_loss = 0;
    printf("rcv event BOS, topic_name='%s', source=%s, \n",
      msg->topic_name, msg->source);
    fflush(stdout);
    break;

  case LBM_MSG_EOS:
    printf("rcv event EOS, '%s', %s, num_rcv_msgs=%"PRIu64", num_rx_msgs=%"PRIu64", num_unrec_loss=%"PRIu64", \n",
        msg->topic_name, msg->source, num_rcv_msgs, num_rx_msgs, num_unrec_loss);
    fflush(stdout);
    break;

  case LBM_MSG_UME_REGISTRATION_ERROR:
  {
    printf("rcv event LBM_MSG_UME_REGISTRATION_ERROR, '%s', %s, msg='%s'\n",
        msg->topic_name, msg->source, (char *)msg->data);
    break;
  }

  case LBM_MSG_UNRECOVERABLE_LOSS:
  {
    num_unrec_loss++;
    break;
  }

  case LBM_MSG_DATA:
  {
    perf_msg_t *perf_msg = (perf_msg_t *)msg->data;
    uint64_t ns_rtt;
    CPRT_DIFF_TS(ns_rtt, rcv_ts, perf_msg->send_ts);

    num_rcv_msgs++;
    if (perf_msg->flags & FLAGS_TIMESTAMP) {
if (((int)ns_rtt) < 0) { printf("ns_rtt=%"PRIu64"\n", ns_rtt); fflush(stdout);} /*???*/
      hist_input((int)ns_rtt);
    }

    if ((msg->flags & LBM_MSG_FLAG_RETRANSMIT) == LBM_MSG_FLAG_RETRANSMIT) {
      num_rx_msgs++;
    }
    break;
  }

  default:
    printf("rcv event %d, topic_name='%s', source=%s, \n", msg->type, msg->topic_name, msg->source); fflush(stdout);
  }  /* switch msg->type */

  return 0;
}  /* rcv_callback */


lbm_xsp_t *my_xsp_mapper_callback(lbm_context_t *ctx, lbm_new_transport_info_t *transp_info, void *clientd)
{
  lbm_xsp_t **my_xsp = (lbm_xsp_t **)clientd;
  return *my_xsp;
}  /* my_xsp_mapper_callback */


int send_loop(int num_sends, uint64_t sends_per_sec)
{
  struct timespec cur_ts;
  struct timespec start_ts;
  uint64_t num_sent;
  int lbm_send_flags, max_tight_sends;
  static lbm_ssrc_send_ex_info_t ssrc_exinfo;

  /* Set up local variable so that test is fast. */
  if (o_generic_src) {
    lbm_send_flags = LBM_SRC_NONBLOCK;
  }
  else {  /* Smart Src API. */
    memset(&ssrc_exinfo, 0, sizeof(ssrc_exinfo));
    lbm_send_flags = 0;
  }

  max_tight_sends = 0;

  /* Send messages evenly-spaced using busy looping. Based on algorithm:
   * http://www.geeky-boy.com/catchup/html/ */
  CPRT_GETTIME(&start_ts);
  cur_ts = start_ts;
  num_sent = 0;
  do {  /* while num_sent < num_sends */
    uint64_t ns_so_far;
    CPRT_DIFF_TS(ns_so_far, cur_ts, start_ts);
    /* The +1 is because we want to send, then pause. */
    uint64_t should_have_sent = (ns_so_far * sends_per_sec)/1000000000 + 1;
    if (should_have_sent > num_sends) {
      should_have_sent = num_sends;
    }
    if (should_have_sent - num_sent > max_tight_sends) {
      max_tight_sends = should_have_sent - num_sent;
    }

    /* If we are behind where we should be, get caught up. */
    while (num_sent < should_have_sent) {
      if (o_generic_src) {
        /* Construct message. */
        perf_msg->msg_num = num_sent;
        CPRT_GETTIME(&(perf_msg->send_ts));
        perf_msg->flags = msg_flags;

        /* Send message. */
        int e = lbm_src_send(src, (void *)perf_msg, o_msg_len, lbm_send_flags);
        if (e == -1) {
          printf("num_sent=%"PRIu64", global_max_tight_sends=%d, max_flight_size=%d\n",
              num_sent, global_max_tight_sends, max_flight_size);
        }
        E(e);  /* If error, print message and fail. */
      }
      else {  /* Smart Src API. */
        perf_msg = (perf_msg_t *)ssrc_buff;
        /* Construct message in shared memory buffer. */
        perf_msg->msg_num = num_sent;
        CPRT_GETTIME(&(perf_msg->send_ts));
        perf_msg->flags = msg_flags;

        /* Send message and get next buffer from shared memory. */
        int e = lbm_ssrc_send_ex(ssrc, (char *)perf_msg, o_msg_len, lbm_send_flags, &ssrc_exinfo);
        if (e == -1) {
          printf("num_sent=%"PRIu64", global_max_tight_sends=%d, max_flight_size=%d\n",
              num_sent, global_max_tight_sends, max_flight_size);
        }
        E(e);  /* If error, print message and fail. */
      }

      int cur = __sync_fetch_and_add(&cur_flight_size, 1);
      if (cur > max_flight_size) {
        max_flight_size = cur;
      }

      num_sent++;
    }  /* while num_sent < should_have_sent */
    CPRT_GETTIME(&cur_ts);
  } while (num_sent < num_sends);

  global_max_tight_sends = max_tight_sends;

  return num_sent;
}  /* send_loop */


int main(int argc, char **argv)
{
  struct timespec start_ts;  /* struct timespec is used by clock_gettime(). */
  struct timespec end_ts;
  uint64_t duration_ns;
  int actual_sends;
  double result_rate;
  CPRT_NET_START;

  CPRT_INITTIME();

  get_my_opts(argc, argv);

  hist_create();

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_src=%d, o_affinity_rcv=%d, o_config=%s, o_generic_src=%d, o_histogram=%s, o_linger_ms=%d, o_msg_len=%d, o_num_msgs=%d, o_persist_mode='%s', o_rcv_thread='%s', o_rate=%d, o_spin_method='%s', o_warmup=%s, xml_config=%s, \n",
      o_affinity_src, o_affinity_rcv, o_config, o_generic_src, o_histogram, o_linger_ms, o_msg_len, o_num_msgs, o_persist_mode, o_rcv_thread, o_rate, o_spin_method, o_warmup, o_xml_config);

  msg_buf = (char *)malloc(o_msg_len);

  create_context();

  if (o_affinity_src > -1) {
    uint64_t cpuset;
    CPRT_CPU_ZERO(&cpuset);
    CPRT_CPU_SET(o_affinity_src, &cpuset);
    cprt_set_affinity(cpuset);
  }

  create_source(ctx);

  create_receiver(ctx);

  /* Ready to send "warmup" messages which should not be accumulated
   * in the histogram.
   */
  msg_flags = 0;

  if (strlen(o_persist_mode) > 0) {
    /* Wait for registration complete. */
    while (registration_complete < 1) {
      CPRT_SLEEP_SEC(1);
      if (registration_complete < 1) {
        printf("Waiting for %d store registrations.\n",
            1 - registration_complete);
      }
    }

    /* Wait for receiver(s) to register and are ready to receive messages.
     * There is a proper algorithm for this, but it adds unnecessary complexity
     * and obscures the perf test algorithms. Sleep for simplicity. */
    CPRT_SLEEP_SEC(5);
  }
  else {  /* strlen(o_persist_mode) == 0 : Streaming (not persistence). */
    if (warmup_loops > 0) {
      /* Without persistence, need to initiate data on src.
       * NOTE: this message will NOT be received (head loss)
       * because topic resolution hasn't completed. */
      send_loop(1, 999999999);
      warmup_loops -= 1;
    }
    /* Wait for topic resolution to complete. */
    CPRT_SLEEP_SEC(1);
  }

  if (warmup_loops > 0) {
    /* Warmup loops to get CPU caches loaded. */
    send_loop(warmup_loops, warmup_rate);
  }

  /* No longer warming up, subsequent messages should be accumulated. */
  msg_flags = FLAGS_TIMESTAMP;

  /* Measure overall send rate by timing the main send loop. */
  if (hist_buckets != NULL) {
    hist_init();  /* Zero out data from warmup period. */
  }
  CPRT_GETTIME(&start_ts);
  actual_sends = send_loop(o_num_msgs, o_rate);
  CPRT_GETTIME(&end_ts);
  CPRT_DIFF_TS(duration_ns, end_ts, start_ts);

  if (o_linger_ms > 0) {
    CPRT_SLEEP_MS(o_linger_ms);
  }

  ASSRT(num_rcv_msgs > 0);

  result_rate = (double)(duration_ns);
  result_rate /= (double)1000000000;
  /* Don't count initial message. */
  result_rate = (double)(actual_sends - 1) / result_rate;

  hist_print();

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("actual_sends=%d, duration_ns=%"PRIu64", result_rate=%f, global_max_tight_sends=%d, max_flight_size=%d\n",
      actual_sends, duration_ns, result_rate, global_max_tight_sends,
      max_flight_size);

  printf("Rcv: num_rcv_msgs=%"PRIu64", num_rx_msgs=%"PRIu64", num_unrec_loss=%"PRIu64", \n",
      num_rcv_msgs, num_rx_msgs, num_unrec_loss);

  if (strlen(o_persist_mode) > 0) {
    /* Wait for Store to get caught up. */
    int num_checks = 0;
    while (cur_flight_size > 0) {
      num_checks++;
      if (num_checks > 3) {
        printf("Giving up.\n");
        break;
      }
      printf("Waiting for flight size %d to clear.\n", cur_flight_size);
      CPRT_SLEEP_SEC(num_checks);  /* Sleep longer each check. */
    }
  }

  delete_source();

  E(lbm_rcv_delete(rcv));

  if (o_rcv_thread[0] == 'x') {
    E(lbm_xsp_delete(my_xsp));
  }

  E(lbm_context_delete(ctx));

  free(msg_buf);

  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
