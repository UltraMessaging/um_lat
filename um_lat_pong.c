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

enum persist_mode_enum { STREAMING, RPP, SPP };
enum rcv_thread_enum { MAIN_CTX, XSP };
enum spin_method_enum { NO_SPIN, FD_MGT_BUSY };

/* Forward declarations. */
lbm_xsp_t *my_xsp_mapper_callback(lbm_context_t *ctx, lbm_new_transport_info_t *transp_info, void *clientd);
int my_rcv_cb(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd);

/* Command-line options and their defaults. String defaults are set
 * in "get_my_opts()". */
int o_affinity_rcv = -1;
char *o_config = NULL;
int o_exit_on_eos = 0;  /* -E */
int o_generic_src = 0;
char *o_persist_mode = NULL;
char *o_rcv_thread = NULL; /* -R */
char *o_spin_method = NULL;
char *o_xml_config = NULL;

/* Parameters parsed out from command-line options. */
char *app_name = "um_perf";
enum persist_mode_enum persist_mode = STREAMING;
enum rcv_thread_enum rcv_thread = MAIN_CTX;
enum spin_method_enum spin_method = NO_SPIN;

/* Globals. */
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
  fprintf(stderr, "Usage: um_lat_pong [-h] [-a affinity_rcv] [-c config] [-E] [-g]\n  [-p persist_mode] [-R rcv_thread] [-s spin_method] [-x xml_config]\n");
  fprintf(stderr, "Where:\n"
      "  -h : print help\n"
      "  -a affinity_rcv : CPU number (0..N-1) for receive thread (-1=none)\n"
      "  -c config : configuration file; can be repeated\n"
      "  -E : exit on EOS\n"
      "  -g : generic source\n"
      "  -p persist_mode : '' (empty)=streaming, 'r'=RPP, 's'=SPP\n"
      "  -R rcv_thread : '' (empty)=main context, 'x'=XSP\n"
      "  -s spin_method : '' (empty)=no spin, 'f'=fd mgt busy\n"
      "  -x xml_config : configuration file\n");
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
      case 'c':
        free(o_config);
        o_config = CPRT_STRDUP(optarg);
        E(lbm_config(o_config));  /* Allow multiple calls. */
        break;
      case 'E': o_exit_on_eos = 1; break;
      case 'g': o_generic_src = 1; break;
      case 'p':
        free(o_persist_mode);
        o_persist_mode = CPRT_STRDUP(optarg);
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
      case 'R':
        free(o_rcv_thread);
        o_rcv_thread = CPRT_STRDUP(optarg);
        if (strcasecmp(o_rcv_thread, "") == 0) {
          rcv_thread = MAIN_CTX;
        } else if (strcasecmp(o_rcv_thread, "x") == 0) {
          rcv_thread = XSP;
        } else {
          assrt(0, "Error, -R value must be '' or 'x'\n");
        }
        break;
      case 's':
        free(o_spin_method);
        o_spin_method = CPRT_STRDUP(optarg);
        if (strcasecmp(o_spin_method, "") == 0) {
          spin_method = NO_SPIN;
        } else if (strcasecmp(o_spin_method, "f") == 0) {
          spin_method = FD_MGT_BUSY;
        } else {
          assrt(0, "Error, -s value must be '' or 'f'\n");
        }
        break;
      case 'x':
        free(o_xml_config);
        o_xml_config = CPRT_STRDUP(optarg);
        /* Don't read it now since app_name might not be set yet. */
        break;
      default:
        fprintf(stderr, "um_lat_pong: error: unrecognized option '%c'\nUse '-h' for help\n", opt);
    }  /* switch opt */
  }  /* while getopt */

  if (optind != argc) { assrt(0, "Unexpected positional parameter(s)"); }

  /* Waited to read xml config (if any) so that app_name is set up right. */
  if (strlen(o_xml_config) > 0) {
    E(lbm_config_xml_file(o_xml_config, app_name));
  }

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
        "ume_session_id", "0x7"));  /* Pong uses session ID 7. */
    if (o_spin_method[0] == 'f') {
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
        "ume_session_id", "0x7"));  /* Pong uses session ID 7. */
    if (o_spin_method[0] == 'f') {
      E(lbm_context_attr_str_setopt(ctx_attr,
          "file_descriptor_management_behavior", "busy_wait"));
    }

    E(lbm_xsp_create(&my_xsp, my_ctx, ctx_attr, NULL));
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


lbm_rcv_t *my_rcv;

void create_receiver(lbm_context_t *ctx)
{
  lbm_rcv_topic_attr_t *rcv_attr;
  E(lbm_rcv_topic_attr_create(&rcv_attr));

  /* Receive messages from ping. */
  lbm_topic_t *topic_obj;
  E(lbm_rcv_topic_lookup(&topic_obj, my_ctx, "topic1", rcv_attr));
  E(lbm_rcv_create(&my_rcv, my_ctx, topic_obj, my_rcv_cb, NULL, NULL));
}  /* create_receiver */


uint64_t num_rcv_msgs;
uint64_t num_rx_msgs;
uint64_t num_unrec_loss;
uint64_t num_sent;

/* UM callback for receiver events, including received messages. */
int my_rcv_cb(lbm_rcv_t *rcv, lbm_msg_t *msg, void *clientd)
{
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
      int e = lbm_ssrc_send_ex(ssrc, ssrc_buff, msg->len, 0, NULL);
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
}  /* my_rcv_cb */


lbm_xsp_t *my_xsp_mapper_callback(lbm_context_t *ctx, lbm_new_transport_info_t *transp_info, void *clientd)
{
  return my_xsp;
}  /* my_xsp_mapper_callback */


int my_logger_cb(int level, const char *message, void *clientd)
{
  /* A real application should include a high-precision time stamp and
   * be thread safe. */
  printf("LOG Level %d: %s\n", level, message);
  return 0;
}  // my_logger_cb


int main(int argc, char **argv)
{
  CPRT_NET_START;

  /* Set up callback for UM log messages. */
  E(lbm_log(my_logger_cb, NULL));

  get_my_opts(argc, argv);

  printf("o_affinity_rcv=%d, o_config=%s, o_exit_on_eos=%d, o_generic_src=%d, o_persist_mode='%s', o_rcv_thread='%s', o_spin_method='%s', o_xml_config=%s, \n",
      o_affinity_rcv, o_config, o_exit_on_eos, o_generic_src, o_persist_mode,
      o_rcv_thread, o_spin_method, o_xml_config);
  printf("app_name='%s', persist_mode=%d, spin_method=%d, \n",
      app_name, persist_mode, spin_method);

  create_context();

  create_source(my_ctx);

  create_receiver(my_ctx);

  /* The subscriber must be "kill"ed externally. */
  CPRT_SLEEP_SEC(2000000000);  /* 23+ centuries. */

  /* Should delete UM objects, but this tool never exits. */

  CPRT_NET_CLEANUP;
  return 0;
}  /* main */
