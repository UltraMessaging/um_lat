/* um_lat_pong.c - performance measurement tool. */
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
 * in "get_my_opts()". */
static int o_affinity_rcv = -1;
static char *o_config = NULL;
static int o_exit_on_eos = 0;  /* -E */
static int o_generic_src = 0;
static char *o_persist_mode = NULL;
static char *o_rcv_thread = NULL; /* -R */
static char *o_spin_method = NULL;
static char *o_xml_config = NULL;

/* Parameters parsed out from command-line options. */
char *app_name;

/* Globals. The code depends on the loader initializing them to all zeros. */
int registration_complete;
int cur_flight_size;
int max_flight_size;


char usage_str[] = "Usage: um_lat_pong [-h] [-a affinity_rcv] [-c config] [-E] [-g] [-p persist_mode] [-R rcv_thread] [-s spin_method] [-x xml_config]";

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
      "  -a affinity_rcv : CPU number (0..N-1) for receive thread (-1=none) [%d]\n"
      "  -c config : configuration file; can be repeated [%s]\n"
      "  -E : exit on EOS [%d]\n"
      "  -g : generic source [%d]\n"
      "  -p persist_mode : '' (empty)=streaming, 'r'=RPP, 's'=SPP [%s]\n"
      "  -R rcv_thread : '' (empty)=main context, 'c'=separate context, 'x'=XSP [%s]\n"
      "  -s spin_method : '' (empty)=no spin, 'f'=fd mgt busy, 'p'=proc events [%s]\n"
      "  -x xml_config : configuration file [%s]\n"
      , o_affinity_rcv, o_config, o_exit_on_eos, o_generic_src, o_persist_mode
      , o_rcv_thread, o_spin_method, o_xml_config
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
  o_persist_mode = CPRT_STRDUP("");
  o_rcv_thread = CPRT_STRDUP("");
  o_spin_method = CPRT_STRDUP("");
  o_xml_config = CPRT_STRDUP("");

  while ((opt = getopt(argc, argv, "ha:c:Egp:R:s:x:")) != EOF) {
    switch (opt) {
      case 'h': help(); break;
      case 'a': CPRT_ATOI(optarg, o_affinity_rcv); break;
      /* Allow -c to be repeated, loading each config file in succession. */
      case 'c': free(o_config);
                o_config = CPRT_STRDUP(optarg);
                E(lbm_config(o_config));  /* Allow multiple calls. */
                break;
      case 'E': o_exit_on_eos = 1; break;
      case 'g': o_generic_src = 1; break;
      case 'p': free(o_persist_mode); o_persist_mode = CPRT_STRDUP(optarg); break;
      case 'R': free(o_rcv_thread); o_rcv_thread = CPRT_STRDUP(optarg); break;
      case 's': free(o_spin_method); o_spin_method = CPRT_STRDUP(optarg); break;
      case 'x': free(o_xml_config); o_xml_config = CPRT_STRDUP(optarg); break;
      default: usage(NULL);
    }  /* switch opt */
  }  /* while getopt */

  if ((strcmp(o_persist_mode, "") != 0) && (strcmp(o_persist_mode, "r") != 0) && (strcmp(o_persist_mode, "s") != 0)) {
    usage("Error, -p value must be '', 'r', or 's'\n");
  }
  if (o_persist_mode[0] == '\0') {
    app_name = "um_perf";
  }
  else if (o_persist_mode[0] == 's') {
    app_name = "um_perf_spp";
  }
  else {
    ASSRT(o_persist_mode[0] == 'r');
    app_name = "um_perf_rpp";
  }

  if ((strcmp(o_rcv_thread, "") != 0) && (strcmp(o_rcv_thread, "c") != 0) && (strcmp(o_rcv_thread, "x") != 0)) {
    usage("Error, -R value must be '', 'c', or 'x'\n");
  }

  if ((strcmp(o_spin_method, "") != 0) && (strcmp(o_spin_method, "f") != 0) && (strcmp(o_spin_method, "p") != 0)) {
    usage("Error, -s value must be '', 'f', or 'p'\n");
  }

  if (strlen(o_xml_config) > 0) {
    /* Unlike lbm_config(), you can't load more than one XML file.
     * If user supplied -x more than once, only load last one. */
    E(lbm_config_xml_file(o_xml_config, app_name));
  }

  if (optind != argc) { usage("Unexpected positional parameter(s)"); }
}  /* get_my_opts */


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
        "ume_session_id", "0x7"));  /* Pong uses session ID 7. */
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
        "ume_session_id", "0x7"));  /* Pong uses session ID 7. */
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

  E(lbm_src_topic_attr_str_setopt(src_attr, "ume_session_id", "0x7"));

  /* Get notified for forced reclaims (should not happen). */
  lbm_ume_src_force_reclaim_func_t force_reclaim_cb_conf;
  force_reclaim_cb_conf.func = force_reclaim_cb;
  force_reclaim_cb_conf.clientd = NULL;
  E(lbm_src_topic_attr_setopt(src_attr, "ume_force_reclaim_function",
      &force_reclaim_cb_conf, sizeof(force_reclaim_cb_conf)));

  /* The "pong" program sends messages to "topic2". */
  E(lbm_src_topic_alloc(&topic_obj, ctx, "topic2", src_attr));
  if (o_generic_src) {
    E(lbm_src_create(&src, ctx, topic_obj,
        src_event_cb, NULL, NULL));
  }
  else {  /* Smart Src API. */
    E(lbm_ssrc_create(&ssrc, ctx, topic_obj,
        ssrc_event_cb, NULL, NULL));
    E(lbm_ssrc_buff_get(ssrc, &ssrc_buff, 0));
  }

  E(lbm_src_topic_attr_delete(src_attr));
}  /* create_source */


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


/* The ssrc_exinfo global is initialized to zeros. */
static lbm_ssrc_send_ex_info_t ssrc_exinfo;
/* UM callback for receiver events, including received messages. */
int rcv_callback(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
  static uint64_t num_rcv_msgs;
  static uint64_t num_rx_msgs;
  static uint64_t num_unrec_loss;
  static uint64_t num_sent;

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
    num_sent = 0;
    printf("rcv event BOS, topic_name='%s', source=%s, \n",
      msg->topic_name, msg->source);
    fflush(stdout);
    break;

  case LBM_MSG_EOS:
    printf("rcv event EOS, '%s', %s, num_rcv_msgs=%"PRIu64", num_rx_msgs=%"PRIu64", num_unrec_loss=%"PRIu64", max_flight_size=%d\n",
        msg->topic_name, msg->source, num_rcv_msgs, num_rx_msgs, num_unrec_loss, max_flight_size);
    fflush(stdout);

    if (o_exit_on_eos) {
      CPRT_NET_CLEANUP;
      exit(0);
    }
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
    num_rcv_msgs++;
    if ((msg->flags & LBM_MSG_FLAG_RETRANSMIT) == LBM_MSG_FLAG_RETRANSMIT) {
      num_rx_msgs++;
    }

    if (o_generic_src) {
      /* Send message. */
      int e = lbm_src_send(src, (void *)msg->data, msg->len, LBM_SRC_NONBLOCK);
      if (e == -1) {
        printf("num_sent=%"PRIu64", max_flight_size=%d\n", num_sent, max_flight_size);
      }
      E(e);  /* If error, print message and fail. */
    }
    else {  /* Smart Src API. */
      memcpy(ssrc_buff, msg->data, msg->len);
      /* Send message and get next buffer from shared memory. */
      int e = lbm_ssrc_send_ex(ssrc, ssrc_buff, msg->len, 0, &ssrc_exinfo);
      if (e == -1) {
        printf("num_sent=%"PRIu64", max_flight_size=%d\n", num_sent, max_flight_size);
      }
      E(e);  /* If error, print message and fail. */
    }
    num_sent++;
    int cur = __sync_fetch_and_add(&cur_flight_size, 1);
    if (cur > max_flight_size) {
      max_flight_size = cur;
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


int main(int argc, char **argv)
{
  lbm_rcv_topic_attr_t *rcv_attr;
  lbm_topic_t *topic_obj;
  lbm_rcv_t *rcv_obj;
  CPRT_NET_START;

  get_my_opts(argc, argv);

  printf("o_affinity_rcv=%d, o_config=%s, o_exit_on_eos=%d, o_generic_src=%d, o_persist_mode='%s', o_rcv_thread='%s', o_spin_method='%s', o_xml_config=%s, \n",
      o_affinity_rcv, o_config, o_exit_on_eos, o_generic_src, o_persist_mode, o_rcv_thread, o_spin_method, o_xml_config);

  create_context();

  /* Set some options in code. */
  E(lbm_rcv_topic_attr_create(&rcv_attr));

  E(lbm_rcv_topic_lookup(&topic_obj, ctx, "topic1", rcv_attr));
  E(lbm_rcv_create(&rcv_obj, ctx, topic_obj, rcv_callback, NULL, NULL));

  create_source(ctx);

  /* The subscriber must be "kill"ed externally. */
  CPRT_SLEEP_SEC(2000000000);  /* 23+ centuries. */

  /* Should delete UM objects, but this tool never exits. */

  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
