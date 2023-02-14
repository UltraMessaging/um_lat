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

enum persist_mode_enum { STREAMING, RPP, SPP };
enum rcv_thread_enum { MAIN_CTX, XSP };
enum spin_method_enum { NO_SPIN, FD_MGT_BUSY };

/* Forward declarations. */
lbm_xsp_t *my_xsp_mapper_callback(lbm_context_t *ctx, lbm_new_transport_info_t *transp_info, void *clientd);
int my_rcv_cb(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd);


/* Command-line options and their defaults. String defaults are set
 * in "get_my_opts()".
 */
int o_affinity_src = -1;  /* -A */
int o_affinity_rcv = -1;
char *o_config = NULL;
int o_generic_src = 0;
char *o_histogram = NULL;  /* -H */
int o_linger_ms = 1000;
int o_msg_len = 0;
int o_num_msgs = 0;
char *o_persist_mode = NULL;
char *o_rcv_thread = NULL; /* -R */
int o_rate = 0;
char *o_spin_method = NULL;
char *o_warmup = NULL;
char *o_xml_config = NULL;

/* Parameters parsed out from command-line options. */
char *app_name = "um_perf";
int hist_num_buckets = 0;
int hist_ns_per_bucket = 0;
enum persist_mode_enum persist_mode = STREAMING;
enum rcv_thread_enum rcv_thread = MAIN_CTX;
enum spin_method_enum spin_method = NO_SPIN;
int warmup_loops = 0;
int warmup_rate = 0;

/* Globals. */
char *msg_buf = NULL;
perf_msg_t *perf_msg = NULL;
int global_max_tight_sends = 0;
int registration_complete = 0;
int cur_flight_size = 0;
int max_flight_size = 0;


void assrt(int assertion, char *err_message)
{
  if (! assertion) {
    fprintf(stderr, "um_lat_ping: Error, %s\nUse '-h' for help\n", err_message);
    exit(1);
  }
}  /* assrt */


void help() {
  fprintf(stderr, "Usage: um_lat_ping [-h] [-A affinity_src] [-a affinity_rcv] [-c config]\n  [-g] -H hist_num_buckets,hist_ns_per_bucket [-l linger_ms] -m msg_len\n  -n num_msgs [-p persist_mode] [-R rcv_thread] -r rate [-s spin_method]\n  [-w warmup_loops,warmup_rate] [-x xml_config]\n");
  fprintf(stderr, "Where (those marked with 'R' are required):\n"
      "  -h : print help\n"
      "  -A affinity_src : CPU number (0..N-1) for send thread (-1=none)\n"
      "  -a affinity_rcv : CPU number (0..N-1) for receive thread (-1=none)\n"
      "  -c config : configuration file; can be repeated\n"
      "  -g : generic source\n"
      "R -H hist_num_buckets,hist_ns_per_bucket : round-trip time histogram\n"
      "  -l linger_ms : linger time before source delete\n"
      "R -m msg_len : message length\n"
      "R -n num_msgs : number of messages to send\n"
      "  -p persist_mode : '' (empty)=streaming, 'r'=RPP, 's'=SPP\n"
      "  -R rcv_thread : '' (empty)=main context, 'x'=XSP\n"
      "R -r rate : messages per second to send\n"
      "  -s spin_method : '' (empty)=no spin, 'f'=fd mgt busy\n"
      "  -w warmup_loops,warmup_rate : messages to send before measurement\n"
      "  -x xml_config : XML configuration file\n");
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

  while ((opt = cprt_getopt(argc, argv, "hA:a:c:gH:l:L:m:n:p:R:r:s:w:x:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'A': CPRT_ATOI(cprt_optarg, o_affinity_src); break;
      case 'a': CPRT_ATOI(cprt_optarg, o_affinity_rcv); break;
      /* Allow -c to be repeated, loading each config file in succession. */
      case 'c':
        free(o_config);
        o_config = CPRT_STRDUP(cprt_optarg);
        E(lbm_config(o_config));
        break;
      case 'g': o_generic_src = 1; break;
      case 'H': {
        free(o_histogram);
        o_histogram = CPRT_STRDUP(cprt_optarg);
        char *work_str = CPRT_STRDUP(o_histogram);
        char *strtok_context;
        char *hist_num_buckets_str = CPRT_STRTOK(work_str, ",", &strtok_context);
        assrt(hist_num_buckets_str != NULL, "-H value must be 'hist_num_buckets,hist_ns_per_bucket'");
        CPRT_ATOI(hist_num_buckets_str, hist_num_buckets);

        char *hist_ns_per_bucket_str = CPRT_STRTOK(NULL, ",", &strtok_context);
        assrt(hist_num_buckets_str != NULL, "-H value must be 'hist_num_buckets,hist_ns_per_bucket'");
        CPRT_ATOI(hist_ns_per_bucket_str, hist_ns_per_bucket);

        assrt((CPRT_STRTOK(NULL, ",", &strtok_context)) == NULL, "-H value must be 'hist_num_buckets,hist_ns_per_bucket'");
        free(work_str);
        break;
      }
      case 'l': CPRT_ATOI(cprt_optarg, o_linger_ms); break;
      case 'm': CPRT_ATOI(cprt_optarg, o_msg_len); break;
      case 'n': CPRT_ATOI(cprt_optarg, o_num_msgs); break;
      case 'p':
        free(o_persist_mode);
        o_persist_mode = CPRT_STRDUP(cprt_optarg);
        if (strcasecmp(o_persist_mode, "") == 0) {
          app_name = "um_perf";
          persist_mode = STREAMING;
        } else if (strcasecmp(o_persist_mode, "r") == 0) {
          app_name = "um_perf_rpp";
          persist_mode = RPP;
        } else if (strcasecmp(o_persist_mode, "s") == 0) {
          app_name = "um_perf_spp";
          persist_mode = SPP;
        } else {
          assrt(0, "-p value must be '', 'r', or 's'");
        }
        break;
      case 'R':
        free(o_rcv_thread);
        o_rcv_thread = CPRT_STRDUP(cprt_optarg);
        if (strcasecmp(o_rcv_thread, "") == 0) {
          rcv_thread = MAIN_CTX;
        } else if (strcasecmp(o_rcv_thread, "x") == 0) {
          rcv_thread = XSP;
        } else {
          assrt(0, "Error, -R value must be '' or 'x'\n");
        }
        break;
      case 'r': CPRT_ATOI(cprt_optarg, o_rate); break;
      case 's':
        free(o_spin_method);
        o_spin_method = CPRT_STRDUP(cprt_optarg);
        if (strcasecmp(o_spin_method, "") == 0) {
          spin_method = NO_SPIN;
        } else if (strcasecmp(o_spin_method, "f") == 0) {
          spin_method = FD_MGT_BUSY;
        } else {
          assrt(0, "Error, -s value must be '' or 'f'\n");
        }
        break;
      case 'w': {
        free(o_warmup);
        o_warmup = CPRT_STRDUP(cprt_optarg);
        char *work_str = CPRT_STRDUP(o_warmup);
        char *strtok_context;
        char *warmup_loops_str = CPRT_STRTOK(work_str, ",", &strtok_context);
        assrt(warmup_loops_str != NULL, "-w value must be 'warmup_loops,warmup_rate'");
        CPRT_ATOI(warmup_loops_str, warmup_loops);

        char *warmup_rate_str = CPRT_STRTOK(NULL, ",", &strtok_context);
        assrt(warmup_rate_str != NULL, "-w value must be 'warmup_loops,warmup_rate'");
        CPRT_ATOI(warmup_rate_str, warmup_rate);

        assrt((CPRT_STRTOK(NULL, ",", &strtok_context)) == NULL, "-w value must be 'warmup_loops,warmup_rate' (3)");
        if (warmup_loops > 0) {
          assrt(warmup_rate > 0, "warmup_rate must be > 0");
        }
        free(work_str);
        break;
      }
      case 'x':
        free(o_xml_config);
        o_xml_config = CPRT_STRDUP(cprt_optarg);
        /* Don't read it now since app_name might not be set yet. */
        break;
      default:
        fprintf(stderr, "um_lat_ping: error: unrecognized option '%c'\nUse '-h' for help\n", opt);
        exit(1);
    }  /* switch opt */
  }  /* while getopt */

  if (cprt_optind != argc) { assrt(0, "Unexpected positional parameter(s)"); }

  /* Must supply certain required "options". */
  assrt(o_rate > 0, "Must supply '-r rate'");
  assrt(o_num_msgs > 0, "Must supply '-n num_msgs'");
  assrt(o_msg_len >= sizeof(perf_msg_t), "Must supply '-m msg_len'");
  assrt(hist_num_buckets > 0, "-H value hist_num_buckets must be > 0");
  assrt(hist_ns_per_bucket > 0, "-H value hist_ns_per_bucket must be > 0");

  /* Waited to read xml config (if any) so that app_name is set up right. */
  if (strlen(o_xml_config) > 0) {
    E(lbm_config_xml_file(o_xml_config, app_name));
  }
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

  return -1;
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
      break;
    case LBM_SRC_EVENT_SEQUENCE_NUMBER_INFO:
      break;
    case LBM_SRC_EVENT_FLIGHT_SIZE_NOTIFICATION:
      break;
    case LBM_SRC_EVENT_UME_MESSAGE_RECLAIMED_EX:
    {
      lbm_src_event_ume_ack_ex_info_t *ack_info = (lbm_src_event_ume_ack_ex_info_t *)extra_data;
      if (ack_info->flags & LBM_SRC_EVENT_UME_MESSAGE_RECLAIMED_EX_FLAG_FORCED) {
        fprintf(stderr, "Forced reclaim (should not happen), sqn=%u\n", ack_info->sequence_number);
        __sync_fetch_and_sub(&cur_flight_size, 1);
        ASSRT(cur_flight_size >= 0);  /* Die if negative. */
      }
      break;
    }
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
int my_ssrc_event_cb(lbm_ssrc_t *ssrc, int event, void *extra_data, void *client_data)
{
  handle_src_event(event, extra_data, client_data);

  return 0;
}  /* my_ssrc_event_cb */

/* Callback for UM source events. */
int my_src_event_cb(lbm_src_t *src, int event, void *extra_data, void *client_data)
{
  handle_src_event(event, extra_data, client_data);

  return 0;
}  /* my_src_event_cb */


lbm_context_t *my_ctx = NULL;
lbm_xsp_t *my_xsp = NULL;

void create_context()
{
  /* Create UM context. */
  lbm_context_attr_t *ctx_attr;
  E(lbm_context_attr_create(&ctx_attr));

  lbm_transport_mapping_func_t mapping_func;
  if (rcv_thread == XSP) {
    /* Set all receive transport sessions to my_xsp. */
    mapping_func.mapping_func = my_xsp_mapper_callback;
    mapping_func.clientd = NULL;
    E(lbm_context_attr_setopt(ctx_attr,
        "transport_mapping_function", &mapping_func, sizeof(mapping_func)));
  }
  else {  /* rcv_thread not xsp. */
    /* Main context will host receivers; set desired options. */
    E(lbm_context_attr_str_setopt(ctx_attr,
        "ume_session_id", "0x6"));  /* Ping uses session ID 6. */

    if (spin_method == FD_MGT_BUSY) {
      E(lbm_context_attr_str_setopt(ctx_attr,
          "file_descriptor_management_behavior", "busy_wait"));
    }
  }

  /* Context thread inherits the initial CPU set of the process. */
  E(lbm_context_create(&my_ctx, ctx_attr, NULL, NULL));
  E(lbm_context_attr_delete(ctx_attr));

  if (rcv_thread == XSP) {
    /* Xsp in use; create a fresh context attr (can't re-use parent's). */
    E(lbm_context_attr_create(&ctx_attr));
    /* Main context will host receivers; set desired options. */
    E(lbm_context_attr_str_setopt(ctx_attr,
        "ume_session_id", "0x6"));  /* Ping uses session ID 6. */
    if (spin_method == FD_MGT_BUSY) {
      E(lbm_context_attr_str_setopt(ctx_attr,
          "file_descriptor_management_behavior", "busy_wait"));
    }

    E(lbm_xsp_create(&my_xsp, my_ctx, ctx_attr, NULL));
    E(lbm_context_attr_delete(ctx_attr));
  }

}  /* create_context */


lbm_src_t *my_src = NULL;  /* Used if o_generic_src is 1. */
lbm_ssrc_t *my_ssrc = NULL;  /* Used if o_generic_src is 0. */
char *my_ssrc_buff = NULL;

void create_source(lbm_context_t *ctx)
{
  lbm_src_topic_attr_t *src_attr;
  lbm_topic_t *topic_obj;

  /* Set some options in code. */
  E(lbm_src_topic_attr_create(&src_attr));

  /* The "ping" program sends messages to "topic1". */
  E(lbm_src_topic_alloc(&topic_obj, ctx, "topic1", src_attr));
  if (o_generic_src) {
    E(lbm_src_create(&my_src, ctx, topic_obj,
        my_src_event_cb, NULL, NULL));
    perf_msg = (perf_msg_t *)msg_buf;  /* Set up perf_msg once. */
  }
  else {  /* Smart Src API. */
    E(lbm_ssrc_create(&my_ssrc, ctx, topic_obj,
        my_ssrc_event_cb, NULL, NULL));
    E(lbm_ssrc_buff_get(my_ssrc, &my_ssrc_buff, 0));
    /* Set up perf_msg before each send. */
  }

  E(lbm_src_topic_attr_delete(src_attr));
}  /* create_source */


void delete_source()
{
  if (o_generic_src) {  /* If using smart src API */
    E(lbm_src_delete(my_src));
  }
  else {
    E(lbm_ssrc_buff_put(my_ssrc, my_ssrc_buff));
    E(lbm_ssrc_delete(my_ssrc));
  }
}  /* delete_source */


lbm_rcv_t *my_rcv;

void create_receiver(lbm_context_t *ctx)
{
  lbm_rcv_topic_attr_t *rcv_attr;
  E(lbm_rcv_topic_attr_create(&rcv_attr));

  /* Receive reflected messages from pong. */
  lbm_topic_t *topic_obj;
  E(lbm_rcv_topic_lookup(&topic_obj, ctx, "topic2", rcv_attr));
  E(lbm_rcv_create(&my_rcv, ctx, topic_obj, my_rcv_cb, NULL, NULL));
}  /* create_receiver */


uint64_t num_rcv_msgs;
uint64_t num_rx_msgs;
uint64_t num_unrec_loss;

/* UM callback for receiver events, including received messages. */
int my_rcv_cb(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
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

    num_rcv_msgs++;

    if (perf_msg->send_ts.tv_sec != 0) {
      uint64_t ns_rtt;
      CPRT_DIFF_TS(ns_rtt, rcv_ts, perf_msg->send_ts);
      hist_input((int)ns_rtt);
    }

    /* Keep track of recovered messages. */
    if ((msg->flags & LBM_MSG_FLAG_RETRANSMIT) != 0) {
      num_rx_msgs++;
    }
    break;
  }

  default:
    printf("rcv event %d, topic_name='%s', source=%s, \n", msg->type, msg->topic_name, msg->source); fflush(stdout);
  }  /* switch msg->type */

  return 0;
}  /* my_rcv_cb */


lbm_xsp_t *my_xsp_mapper_callback(lbm_context_t *ctx, lbm_new_transport_info_t *transp_info, void *clientd)
{
  return my_xsp;
}  /* my_xsp_mapper_callback */


int send_loop(int num_sends, uint64_t sends_per_sec, int send_timestamp)
{
  struct timespec cur_ts;
  struct timespec start_ts;

  int msg_send_flags = 0;
  if (o_generic_src) {
    msg_send_flags = LBM_SRC_NONBLOCK;
  }

  int max_tight_sends = 0;

  /* Send messages evenly-spaced using busy looping. Based on algorithm:
   * http://www.geeky-boy.com/catchup/html/ */
  CPRT_GETTIME(&start_ts);
  cur_ts = start_ts;
  uint64_t num_sent = 0;
  do {  /* while num_sent < num_sends */
    uint64_t ns_so_far;
    CPRT_DIFF_TS(ns_so_far, cur_ts, start_ts);
    /* The +1 is because we want to send, then pause. */
    int should_have_sent = (int)((ns_so_far * sends_per_sec)/1000000000 + 1);
    if (should_have_sent > num_sends) {
      should_have_sent = num_sends;  // Don't send more than requested.
    }
    if ((should_have_sent - num_sent) > max_tight_sends) {
      max_tight_sends = should_have_sent - num_sent;
    }

    /* If we are behind where we should be, get caught up. */
    while (num_sent < should_have_sent) {
      if (o_generic_src) {
        /* Construct message. */
        if (send_timestamp) {
          CPRT_GETTIME(&(perf_msg->send_ts));
        }
        else {
          perf_msg->send_ts.tv_sec = 0;
          perf_msg->send_ts.tv_nsec = 0;
        }

        /* Send message. */
        int e = lbm_src_send(my_src, (void *)perf_msg, o_msg_len, msg_send_flags);
        if (e == -1) {
          printf("num_sent=%"PRIu64", global_max_tight_sends=%d, max_flight_size=%d\n",
              num_sent, global_max_tight_sends, max_flight_size);
        }
        E(e);  /* If error, print message and fail. */
      }
      else {  /* Smart Src API. */
        perf_msg = (perf_msg_t *)my_ssrc_buff;
        /* Construct message in shared memory buffer. */
        if (send_timestamp) {
          CPRT_GETTIME(&(perf_msg->send_ts));
        }
        else {
          perf_msg->send_ts.tv_sec = 0;
          perf_msg->send_ts.tv_nsec = 0;
        }

        /* Send message and get next buffer from shared memory. */
        int e = lbm_ssrc_send_ex(my_ssrc, (char *)perf_msg, o_msg_len, msg_send_flags, NULL);
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


int my_logger_cb(int level, const char *message, void *clientd)
{
  /* A real application should include a high-precision time stamp and
   * be thread safe. */
  printf("LOG Level %d: %s\n", level, message);
  return 0;
}  // my_logger_cb


int main(int argc, char **argv)
{
  struct timespec start_ts;  /* struct timespec is used by clock_gettime(). */
  struct timespec end_ts;
  uint64_t duration_ns;
  int actual_sends;
  double result_rate;
  CPRT_NET_START;

  /* Set up callback for UM log messages. */
  E(lbm_log(my_logger_cb, NULL));

  CPRT_INITTIME();

  get_my_opts(argc, argv);

  hist_create();

  /* Leave "comma space" at end of line to make parsing output easier. */
  printf("o_affinity_src=%d, o_affinity_rcv=%d, o_config=%s, o_generic_src=%d, o_histogram=%s, o_linger_ms=%d, o_msg_len=%d, o_num_msgs=%d, o_persist_mode='%s', o_rcv_thread='%s', o_rate=%d, o_spin_method='%s', o_warmup=%s, o_xml_config=%s, \n",
      o_affinity_src, o_affinity_rcv, o_config, o_generic_src, o_histogram,
      o_linger_ms, o_msg_len, o_num_msgs, o_persist_mode, o_rcv_thread, o_rate,
      o_spin_method, o_warmup, o_xml_config);
  printf("app_name='%s', hist_num_buckets=%d, hist_ns_per_bucket=%d, persist_mode=%d, spin_method=%d, warmup_loops=%d, warmup_rate=%d, \n",
      app_name, hist_num_buckets, hist_ns_per_bucket, persist_mode, spin_method,
      warmup_loops, warmup_rate);

  msg_buf = (char *)malloc(o_msg_len);  // Not used by SmartSource.

  create_context();

  if (o_affinity_src > -1) {
    uint64_t cpuset;
    CPRT_CPU_ZERO(&cpuset);
    CPRT_CPU_SET(o_affinity_src, &cpuset);
    cprt_set_affinity(cpuset);
  }

  create_source(my_ctx);

  create_receiver(my_ctx);

  /* Ready to send "warmup" messages which should not be accumulated
   * in the histogram.
   */

  if (persist_mode != STREAMING) {
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
  else {  /* Streaming. */
    if (warmup_loops > 0) {
      /* Without persistence, need to initiate data on src.
       * NOTE: this message will NOT be received (head loss)
       * because topic resolution hasn't completed. */
      send_loop(1, 999999999, 0);
      warmup_loops--;
    }
    /* Wait for topic resolution to complete. */
    CPRT_SLEEP_SEC(1);
  }

  if (warmup_loops > 0) {
    /* Warmup loops to get CPU caches loaded. */
    send_loop(warmup_loops, warmup_rate, 0);
  }

  /* Measure overall send rate by timing the main send loop. */
  hist_init();  /* Zero out data from warmup period. */

  CPRT_GETTIME(&start_ts);
  actual_sends = send_loop(o_num_msgs, o_rate, 1);
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

  if (persist_mode != STREAMING) {
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

  E(lbm_rcv_delete(my_rcv));

  if (rcv_thread == XSP) {
    E(lbm_xsp_delete(my_xsp));
  }

  E(lbm_context_delete(my_ctx));

  free(msg_buf);

  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
