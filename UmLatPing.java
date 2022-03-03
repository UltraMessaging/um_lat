/* UmLatPing.java - performance measurement tool. */
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

import java.util.*;
import java.nio.*;
import java.nio.charset.StandardCharsets;
import com.latencybusters.lbm.*;

class UmLatPing {
  // Command-line options.
  int o_affinity_src;
  int o_affinity_rcv;


  public static void main(String[] args) throws Exception {
    // The body of the program is in the "run" method.
    UmLatPing application = new UmLatPing();
    application.run(args);
  }  // main

  private void run(String[] args) throws Exception {
    getMyOpts(args);
  }  // run


  // Used when parsing options that have arguments.
  private String optArg(String[] args, int argNum) {                   
    if (argNum >= args.length)
    {               
      throw new IllegalArgumentException("Option '" + args[argNum - 1] + "' requires argument.");
    }               
    return args[argNum];
  }  // optArg

  // Process command-line options.
  private void getMyOpts(String[] args) throws Exception {
    int argNum = 0;
    while (argNum < args.length) {
      if (! args[argNum].startsWith("-")) {
        break;  // No more options, only positional parameters left.
      }

      String opt = args[argNum ++];  // Consume option.
      try {
        switch (opt) {
          case "-h":
            help();
            break;
          case "-A":
            o_affinity_src = Integer.parseInt(optArg(args, argNum ++));
            break;
          case "-a":
            o_affinity_rcv = Integer.parseInt(optArg(args, argNum ++));
            break;
          default:
            System.err.println("UmLatPing: error: unrecognized option '" +
                opt + "'\nUse '-h' for help");
        }  // switch
      } catch (Exception e) {
        System.err.println("UmLatPing: error processing option '" +
            opt + "':\n" + e + "\nUse '-h' for help");
        System.exit(1);
      }  // try
    }  // while argNum

    // This program doesn't have positional parameters.
    if (argNum < args.length) {
      System.err.println("UmLatPing: error, unexpected positional parameter: "
          + args[argNum]);
      System.exit(1);
    }
  }  // getMyOpts

  private void help() {
    System.out.println("Usage: UmLatPing [-h] [-A affinity_src] [-a affinity_rcv] [-c config] [-g] [-H hist_num_buckets,hist_ns_per_bucket] [-l linger_ms] [-m msg_len] [-n num_msgs] [-p persist_mode] [-R rcv_thread] [-r rate] [-s spin_method] [-w warmup_loops,warmup_rate] [-x xml_config]\n");
    System.out.println("Where:\n" +
      "-h : print help\n" +
      "  -A affinity_src : CPU number (0..N-1) for send thread (-1=none)\n" +
      "  -a affinity_rcv : CPU number (0..N-1) for receive thread (-1=none)\n" +
      "  -c config : configuration file; can be repeated\n" +
      "  -g : generic source\n"  +
      "  -H hist_num_buckets,hist_ns_per_bucket : send time histogram\n" +
      "  -l linger_ms : linger time before source delete\n" +
      "  -m msg_len : message length\n" +
      "  -n num_msgs : number of messages to send\n" +
      "  -p persist_mode : '' (empty)=streaming, 'r'=RPP, 's'=SPP\n" +
      "  -R rcv_thread : '' (empty)=main context, 'x'=XSP\n" +
      "  -r rate : messages per second to send\n" +
      "  -s spin_method : '' (empty)=no spin, 'f'=fd mgt busy\n" +
      "  -w warmup_loops,warmup_rate : messages to send before measurement\n" +
      "  -x xml_config : XML configuration file\n");
    System.exit(0);
  }  // help


}  // class UmLatPing
