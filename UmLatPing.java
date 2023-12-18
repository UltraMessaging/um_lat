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
import java.util.concurrent.atomic.AtomicInteger;
import com.latencybusters.lbm.*;

// The application class supplies the onReceive, onSourceEvent,
// onTransportMapping, and LBMLog callbacks directly.
class UmLatPing implements LBMSourceEventCallback, LBMReceiverCallback, LBMTransportMappingCallback, LBMLogging {
  enum PersistMode { STREAMING, RPP, SPP };
  enum RcvThread { MAIN_CTX, XSP };
  enum SpinMethod { NO_SPIN, FD_MGT_BUSY };

  // Command-line options.
  String optConfig = "";
  boolean optGenericSrc = false;
  String optHistogram = "0,0";  // -H
  int optLingerMs = 1000;
  int optMsgLen = 0;
  int optNumMsgs = 0;
  String optPersistMode = "";
  String optRcvThread = "";  // -R
  int optRate = 0;
  boolean optSequential = false;  // -S
  String optSpinMethod = "";
  String optWarmup = "0,0";
  String optXmlConfig = "";

  // Parameters parsed out from command-line options.
  String appName = "um_perf";
  int histNumBuckets = 0;
  int histNsPerBucket = 0;
  PersistMode persistMode = PersistMode.STREAMING;
  RcvThread rcvThread = RcvThread.MAIN_CTX;
  SpinMethod spinMethod = SpinMethod.NO_SPIN;
  int warmupLoops = 0;
  int warmupRate = 0;

  // Globals.
  LBM lbm = null;
  int globalMaxTightSends = 0;
  int registrationComplete = 0;
  AtomicInteger curFlightSize = new AtomicInteger(0);
  int maxFlightSize = 0;


  public static void main(String[] args) throws Exception {
    // The body of the program is in the "run" method.
    UmLatPing application = new UmLatPing();
    application.run(args);
  }  // main


  public void LBMLog(int logLevel, String message) {
    // A real application should include a high-precision time stamp and
    // be thread safe.
    System.out.println("AppLogger: logLevel=" + logLevel + ", message=" + message);
  }  // LBMLog


  private void assrt(boolean assertion, String errMessage) {
    if (! assertion) {
      System.err.println("UmLatPing: ERROR: '" + errMessage + "' not true");
      new Exception().printStackTrace();
      System.exit(1);
    }
  }  // assrt

  private void fatalError(String errMessage) {
    System.err.println("UmLatPing: ERROR: '" + errMessage + "' not true");
    new Exception().printStackTrace();
    System.exit(1);
  }  // fatalError


  // Used when parsing options that have arguments.
  private String optArg(String[] args, int argNum) {                   
    if (argNum >= args.length)
    {               
      throw new IllegalArgumentException("Option '" + args[argNum - 1] + "' requires argument.");
    }               
    return args[argNum];
  }  // optArg


  private void help() {
    System.out.println("Usage: UmLatPing [-h] [-c config] [-g] -H hist_num_buckets,hist_ns_per_bucket [-l linger_ms] -m msg_len -n num_msgs [-p persist_mode] [-R rcv_thread] -r rate [-S] [-s spin_method] [-w warmup_loops,warmup_rate] [-x xml_config]\n");
    System.out.println("Where (those marked with 'R' are required):\n" +
      "  -h : print help\n" +
      "  -c config : configuration file; can be repeated\n" +
      "  -g : generic source\n"  +
      "R -H hist_num_buckets,hist_ns_per_bucket : send time histogram\n" +
      "  -l linger_ms : linger time before source delete\n" +
      "R -m msg_len : message length\n" +
      "R -n num_msgs : number of messages to send\n" +
      "  -p persist_mode : '' (empty)=streaming, 'r'=RPP, 's'=SPP\n" +
      "  -R rcv_thread : '' (empty)=main context, 'x'=XSP\n" +
      "R -r rate : messages per second to send\n" +
      "  -S : sequential context and XSP (if enabled with -R)\n" +
      "  -s spin_method : '' (empty)=no spin, 'f'=fd mgt busy\n" +
      "  -w warmup_loops,warmup_rate : messages to send before measurement\n" +
      "  -x xml_config : XML configuration file\n");
    System.exit(0);
  }  // help


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
          case "-h": help(); break;
          case "-c":
            optConfig = optArg(args, argNum ++);
            LBM.setConfiguration(optConfig);
            break;
          case "-g": optGenericSrc = true; break;
          case "-H":
            optHistogram = optArg(args, argNum ++);
            String[] values = optHistogram.split(",");
            assrt(values.length == 2, "values.length == 2");
            histNumBuckets = Integer.parseInt(values[0]);
            histNsPerBucket = Integer.parseInt(values[1]);
            break;
          case "-l": optLingerMs = Integer.parseInt(optArg(args, argNum ++)); break;
          case "-m": optMsgLen = Integer.parseInt(optArg(args, argNum ++)); break;
          case "-n": optNumMsgs = Integer.parseInt(optArg(args, argNum ++)); break;
          case "-p":
            optPersistMode = optArg(args, argNum ++);
            if (optPersistMode.equalsIgnoreCase("")) {
              appName = "um_perf";
              persistMode = PersistMode.STREAMING;
            } else if (optPersistMode.equalsIgnoreCase("r")) {
              appName = "um_perf_rpp";
              persistMode = PersistMode.RPP;
            } else if (optPersistMode.equalsIgnoreCase("s")) {
              appName = "um_perf_spp";
              persistMode = PersistMode.SPP;
            } else {
              fatalError("-p value must be '', 'r', or 's'");
            }
            break;
          case "-R":
            optRcvThread = optArg(args, argNum ++);
            if (optRcvThread.equalsIgnoreCase("")) {
              rcvThread = RcvThread.MAIN_CTX;
            } else if (optRcvThread.equalsIgnoreCase("x")) {
              rcvThread = RcvThread.XSP;
            } else {
              fatalError("-R value must be '' or 'x'");
            }
            break;
          case "-r": optRate = Integer.parseInt(optArg(args, argNum ++)); break;
          case "-S": optSequential = true; break;
          case "-s":
            optSpinMethod = optArg(args, argNum ++);
            if (optSpinMethod.equalsIgnoreCase("")) {
              spinMethod = SpinMethod.NO_SPIN;
            } else if (optSpinMethod.equalsIgnoreCase("f")) {
              spinMethod = SpinMethod.FD_MGT_BUSY;
            } else {
              fatalError("-s value must be '' or 'f'");
            }
            break;
          case "-w":
            optWarmup = optArg(args, argNum ++);
            values = optWarmup.split(",");
            assrt(values.length == 2, "values.length == 2");
            warmupLoops = Integer.parseInt(values[0]);
            warmupRate = Integer.parseInt(values[1]);
            if (warmupLoops > 0) {
              assrt(warmupRate > 0, "warmupRate > 0");
            }
            break;
          case "-x":
            optXmlConfig = optArg(args, argNum ++);
            // Don't read it now since app_name might not be set yet.
            break;
          default:
            System.err.println("unrecognized option '" + opt + "'");
            System.exit(1);
        }  // switch
      } catch (Exception e) {
        fatalError("Exception processing option '" + opt + "':\n" + e);
      }  // try
    }  // while argNum

    assrt(argNum == args.length, "argNum == args.length");  // No further command-line parameters allowed.

    // Make sure required options are supplied.
    assrt(optRate > 0, "optRate > 0");
    assrt(optNumMsgs > 0, "optNumMsgs > 0");
    assrt(optMsgLen > 0, "optMsgLen > 0");
    assrt(optHistogram.length() > 0, "optHistogram.length() > 0");
    assrt(histNumBuckets > 0, "histNumBuckets > 0");
    assrt(histNsPerBucket > 0, "histNsPerBucket > 0");

    // Waited to read xml config (if any) so that app_name is set up right.
    if (optXmlConfig.length() > 0) {
      LBM.setConfigurationXml(optXmlConfig, appName);
    }
  }  // getMyOpts


  long histBuckets[];
  long histMinSample = 999999999;
  long histMaxSample = 0;
  int histOverflows = 0;
  int histNumSamples = 0;
  long histSampleSum = 0;

  private void histInit() {
    histMinSample = 999999999;
    histMaxSample = 0;
    histOverflows = 0;
    histNumSamples = 0;
    histSampleSum = 0;

    int i;
    for (i = 0; i < histNumBuckets; i++) {
      histBuckets[i] = 0;
    }
  }  // histInit

  private void histCreate() {
    histBuckets = new long[histNumBuckets];

    histInit();
  }  // histCreate

  private void histInput(long inSample) {
    histNumSamples++;
    histSampleSum += inSample;

    if (inSample > histMaxSample) {
      histMaxSample = inSample;
    }
    if (inSample < histMinSample) {
      histMinSample = inSample;
    }

    int bucket = (int)(inSample / histNsPerBucket);
    if (bucket >= histNumBuckets) {
      histOverflows++;
    }
    else {
      histBuckets[bucket]++;
    }
  }  // histInput

  // Get the latency (in ns) which "percentile" percent of samples are below.
  // Returns -1 if not calculable (i.e. too many overflows).
  private int histPercentile(double percentile) {
    int i;
    int neededSamples = (int)((double)histNumSamples * (double)percentile / 100.0);
    int foundSamples = 0;

    for (i = 0; i < histNumBuckets; i++) {
      foundSamples += histBuckets[i];
      if (foundSamples > neededSamples) {
        return (i+1) * histNsPerBucket;
      }
    }

    return -1;
  }  // histPercentile

  private void histPrint() {
    int i;
    for (i = 0; i < histNumBuckets; i++) {
      System.out.println(histBuckets[i]);
    }
    System.out.println("opt_histogram=" + optHistogram + ", hist_overflows=" + histOverflows +
        ", hist_min_sample=" + histMinSample + ", hist_max_sample=" + histMaxSample + ",");
    long averageSample = histSampleSum / histNumSamples;
    System.out.println("hist_num_samples=" + histNumSamples + ", average_sample=" + averageSample + ",");

    System.out.println("Percentiles: 90=" + histPercentile(90.0) + ", 99=" + histPercentile(99.0) +
        ", 99.9=" + histPercentile(99.9) + ", 99.99=" + histPercentile(99.99) +
        ", 99.999=" + histPercentile(99.999));
    System.out.flush();  // typically not needed, but not guaranteed.
  }  // histPrint


  public int onSourceEvent(Object arg, LBMSourceEvent sourceEvent) {
    switch (sourceEvent.type()) {
      case LBM.SRC_EVENT_CONNECT:
        break;
      case LBM.SRC_EVENT_DISCONNECT:
        break;
      case LBM.SRC_EVENT_WAKEUP:
        break;
      case LBM.SRC_EVENT_UME_REGISTRATION_ERROR:
        break;
      case LBM.SRC_EVENT_UME_STORE_UNRESPONSIVE:
        break;
      case LBM.SRC_EVENT_UME_REGISTRATION_SUCCESS_EX:
        break;
      case LBM.SRC_EVENT_UME_REGISTRATION_COMPLETE_EX:
        registrationComplete++;
        break;
      case LBM.SRC_EVENT_UME_MESSAGE_STABLE_EX:  // Message stable, flight size shrinks.
        assrt(curFlightSize.decrementAndGet() >= 0, "curFlightSize.decrementAndGet() >= 0");
        break;
      case LBM.SRC_EVENT_SEQUENCE_NUMBER_INFO:
        break;
      case LBM.SRC_EVENT_FLIGHT_SIZE_NOTIFICATION:
        break;
      case LBM.SRC_EVENT_UME_MESSAGE_RECLAIMED_EX:
        UMESourceEventAckInfo reclaiminfo = sourceEvent.ackInfo();
        if ((reclaiminfo.flags() & LBM.SRC_EVENT_UME_MESSAGE_RECLAIMED_EX_FLAG_FORCED) != 0) {
          System.err.println("Forced reclaim (should not happen): sqn=" + reclaiminfo.sequenceNumber());
          // Forced reclaim also shrinks flight size.
          assrt(curFlightSize.decrementAndGet() >= 0, "curFlightSize.decrementAndGet() >= 0");
        }
        break;
      case LBM.SRC_EVENT_UME_DEREGISTRATION_SUCCESS_EX:
        break;
      case LBM.SRC_EVENT_UME_DEREGISTRATION_COMPLETE_EX:
        break;
      case LBM.SRC_EVENT_UME_MESSAGE_NOT_STABLE:
        break;
      default:
        System.err.println("handleSrcEvent: ERROR, unexpected event: " + sourceEvent.type());
    }  // switch

    sourceEvent.dispose();
    return 0;
  }  // handleSourceEvent


  LBMContext myCtx = null;
  LBMXSP myXsp = null;
  private static LBMContextThread myCtxThread = null;
  private static LBMXspThread myXspCtxThread = null;

  private void createContext() throws Exception {
    LBMContextAttributes ctxAttr = new LBMContextAttributes();
    if (optSequential) {
      ctxAttr.setProperty("operational_mode", "sequential");
    }

    if (rcvThread == RcvThread.XSP) {
      // Set up callback "onTransportMapping()" when transports are joined.
      ctxAttr.setTransportMappingCallback(this, null);
    }
    else {  // rcvThread not XSP. */
      // Main context will host receivers; set desired options.
      ctxAttr.setProperty("ume_session_id", "0x6");  // Ping uses session ID 6.

      if (spinMethod == SpinMethod.FD_MGT_BUSY) {
        ctxAttr.setProperty("file_descriptor_management_behavior", "busy_wait");
      }
    }

    myCtx = new LBMContext(ctxAttr);

    if (optSequential) {
      myCtxThread = new LBMContextThread(myCtx);
      myCtxThread.start();
    }

    if (rcvThread == RcvThread.XSP) {
      // Xsp in use; create a fresh context attr (can't re-use parent's).
      LBMContextAttributes xspCtxAttr = new LBMContextAttributes();
      // Main context will host receivers; set desired options.
      xspCtxAttr.setProperty("ume_session_id", "0x6");  // Ping uses session ID 6.

      if (spinMethod == SpinMethod.FD_MGT_BUSY) {
        xspCtxAttr.setProperty("file_descriptor_management_behavior", "busy_wait");
      }
      myXsp = new LBMXSP(myCtx, xspCtxAttr, null);

      if (optSequential) {
        myXspCtxThread = new LBMXspThread(myXsp);
        myXspCtxThread.start();
      }
    }
  }  // createContext


  LBMSource mySrc = null;
  LBMSSource mySSrc = null;
  ByteBuffer mySSrcBuffer = null;

  private void createSource(LBMContext ctx) throws Exception {
    LBMSourceAttributes srcAttr = new LBMSourceAttributes();
    // The "ping" program sends messages to "topic1".
    LBMTopic srcTopic = ctx.allocTopic("topic1", srcAttr);;
    if (optGenericSrc) {
      // Set up callback "onSourceEvent()" for UM to deliver source events.
      mySrc = ctx.createSource(srcTopic, this, null);
      mySSrcBuffer = ByteBuffer.allocateDirect(optMsgLen);
    }
    else {  // Smart Src API.
      mySSrc = new LBMSSource(myCtx, srcTopic);
      mySSrcBuffer = mySSrc.buffGet();
    }
  }  // createSource

  private void deleteSource() throws Exception {
    if (optGenericSrc) {
      mySrc.close();
    }
    else {  // Smart Src API.
      mySSrc.close();
    }
  }  // deleteSource


  LBMReceiver myRcv = null;

  private void createReceiver(LBMContext ctx) throws Exception {
    LBMReceiverAttributes rcvAttr = new LBMReceiverAttributes();
    // The "pong" program sends messages to "topic2", which we receive.
    LBMTopic rcvTopic = ctx.lookupTopic("topic2", rcvAttr);
    // Set up callback "onReceive()" for UM to deliver messages and other events.
    myRcv = ctx.createReceiver(rcvTopic, this, null);
  }  // createReceiver


  long numRcvMsgs;
  long numRxMsgs;
  long numUnrecLoss;

  public int onReceive(Object cbArg, LBMMessage msg) {
    long rcvNs = System.nanoTime();

    switch (msg.type()) {
      case LBM.MSG_BOS:
        // In a perfect world, here's where we would set affinity.
        numRcvMsgs = 0;
        numRxMsgs = 0;
        numUnrecLoss = 0;
        System.out.println("rcv event BOS, topic_name='" + msg.topicName() +
            "', source=" + msg.source() + ",");
        System.out.flush();  // typically not needed, but not guaranteed.
        break;

      case LBM.MSG_EOS:
        System.out.println("rcv event EOS, '" + msg.topicName() +
            "', " + msg.source() + ", num_rcv_msgs=" + numRcvMsgs +
            ", num_rx_msgs=" + numRxMsgs + ", num_unrec_loss=" + numUnrecLoss + ",");
        System.out.flush();  // typically not needed, but not guaranteed.
        break;

      case LBM.MSG_UME_REGISTRATION_ERROR:
        System.out.println("rcv event LBM_MSG_UME_REGISTRATION_ERROR, '" +
            msg.topicName() + "', " + msg.source() + ", msg='" + msg.dataString());
        System.out.flush();  // typically not needed, but not guaranteed.
        break;

      case LBM.MSG_UNRECOVERABLE_LOSS:
        numUnrecLoss++;
        break;

      case LBM.MSG_DATA:
        ByteBuffer message = msg.getMessagesBuffer();  // For SMX.
        if (message == null) {
          message = msg.dataBuffer();  // For non-SMX.
        }

        numRcvMsgs++;

        long sentNs = message.getLong();  // Extract timestamp.
        if (sentNs != 0) {
          histInput(rcvNs - sentNs);
        }

        // Keep track of recovered messages.
        if ((msg.flags() & LBM.MSG_FLAG_RETRANSMIT) != 0) {
          numRxMsgs++;
        }
        break;

      default:
        System.out.println("rcv event " + msg.type() +
            ", topic_name='" + msg.topicName() +
            "', source=" + msg.source() + ",");
        System.out.flush();  // typically not needed, but not guaranteed.
    }  // switch

    msg.dispose();
    return 0;
  }  // onReceive


  public LBMXSP onTransportMapping(LBMContext context, LBMNewTransportInfo newTransportInfo,
      Object cbArg) {
    return myXsp;
  }  // onTransportMapping


  private int sendLoop(int numSends, long sendsPerSec, boolean sendTimestamp) {
    int msgSendFlags = 0;
    if (optGenericSrc) {
      msgSendFlags = LBM.SRC_NONBLOCK;
    }

    int maxTightSends = 0;

    // Send messages evenly-spaced using busy looping. Based on algorithm:
    // http://www.geeky-boy.com/catchup/html/
    long startNs = System.nanoTime();
    long curNs = startNs;
    int numSent = 0;
    do {  // while numSent < numSends
      long nsSoFar = curNs - startNs;
      // The +1 is because we want to send, then pause.
      int shouldHaveSent = (int)((nsSoFar * sendsPerSec) / 1000000000 + 1);
      if (shouldHaveSent > numSends) {
        shouldHaveSent = numSends;  // Don't send more than requested.
      }
      if ((shouldHaveSent - numSent) > maxTightSends) {
        maxTightSends = (int)(shouldHaveSent - numSent);
      }

      // If we are behind where we should be, get caught up.
      while (numSent < shouldHaveSent) {
        // Construct message.
        mySSrcBuffer.position(0);
        if (sendTimestamp) {
          mySSrcBuffer.putLong(System.nanoTime());
        }
        else {
          mySSrcBuffer.putLong(0);
        }

        // Send message.
        try {
          if (optGenericSrc) {
            mySrc.send(mySSrcBuffer, 0, optMsgLen, msgSendFlags);
          }
          else {  // Smart Src API.
            mySSrc.send(mySSrcBuffer, 0, optMsgLen, msgSendFlags, null);
          }
        } catch (Exception e) {
          System.err.println("ERROR sending: " + e.toString());
          System.exit(1);
        }

        int cur = curFlightSize.incrementAndGet();
        if (cur > maxFlightSize) {
          maxFlightSize = cur;
        }

        numSent++;
      }  // while numSent < shouldHaveSent

      curNs = System.nanoTime();
    } while (numSent < numSends);

    globalMaxTightSends = maxTightSends;

    return numSent;
  }  // sendLoop

  private void run(String[] args) throws Exception {
    lbm = new LBM();
    // Set up callback "LBMLog()" for UM log messages.
    lbm.setLogger(this);

    getMyOpts(args);
    histCreate();

    System.out.println("opt_config=" + optConfig +
        ", opt_generic_src=" + optGenericSrc + ", opt_histogram=" + optHistogram +
        ", opt_linger_ms=" + optLingerMs + ", opt_msg_len=" + optMsgLen +
        ", opt_num_msgs=" + optNumMsgs + ", opt_persist_mode=" + optPersistMode +
        ", opt_rcv_thread=" + optRcvThread + ", opt_rate=" + optRate +
        ", opt_sequential=" + optSequential +
        ", opt_spin_method=" + optSpinMethod + ", opt_warmup=" + optWarmup +
        ", opt_xml_config=" + optXmlConfig + ",");
    System.out.println("app_name='" + appName +
        "', hist_num_buckets=" + histNumBuckets +
        ", hist_ns_per_bucket=" + histNsPerBucket +
        ", persist_mode=" + persistMode +
        ", spinMethod=" + spinMethod +
        ", warmupLoops=" + warmupLoops +
        ", warmupRate=" + warmupRate + ",");
    System.out.flush();  // typically not needed, but not guaranteed.

    createContext();

    createSource(myCtx);

    createReceiver(myCtx);

    // Ready to send "warmup" messages which should not be accumulated
    // in the histogram.
    if (persistMode != PersistMode.STREAMING) {
      // Wait for registration complete.
      while (registrationComplete < 1) {
        Thread.sleep(1000);
        if (registrationComplete < 1) {
          System.out.println("Waiting for " + (1 - registrationComplete));
          System.out.flush();  // typically not needed, but not guaranteed.
        }
      }

      // Wait for receiver(s) to register and are ready to receive messages.
      // There is a proper algorithm for this, but it adds unnecessary
      // complexity and obscures the perf test algorithms. Sleep for simplicity.
      Thread.sleep(5000);
    }
    else {  // Streaming.
      if (warmupLoops > 0) {
        // Without persistence, need to initiate data on src.
        // NOTE: this message will NOT be received (head loss)
        // because topic resolution hasn't completed.
        sendLoop(1, 999999999, false);
        warmupLoops--;
      }
      // Wait for topic resolution to complete.
      Thread.sleep(1000);
    }

    if (warmupLoops > 0) {
      sendLoop(warmupLoops, warmupRate, false);
      Thread.sleep(optLingerMs);
    }

    // Measure overall send rate by timing the main send loop.
    histInit();  // Zero out data from warmup period.
    numRcvMsgs = 0;  // Starting over.
    numRxMsgs = 0;
    numUnrecLoss = 0;

    long startNs = System.nanoTime();
    int actualSends = sendLoop(optNumMsgs, optRate, true);
    long endNs = System.nanoTime();
    long durationNs = endNs - startNs;

    Thread.sleep(optLingerMs);

    assrt(numRcvMsgs > 0, "numRcvMsgs > 0");

    double resultRate = durationNs;
    resultRate /= 1000000000;
    // Don't count initial message.
    resultRate = (double)(actualSends - 1) / resultRate;

    histPrint();

    // Leave "comma space" at end of line to make parsing output easier.
    System.out.println("actual_sends=" + actualSends +
        ", duration_ns=" + durationNs + ", result_rate=" + resultRate +
        ", global_max_tight_sends=" + globalMaxTightSends +
        ", max_flight_size=" + maxFlightSize +
        ", ");
    System.out.println("Rcv: num_rcv_msgs=" + numRcvMsgs +
        ", num_rx_msgs=" + numRxMsgs + ", num_unrec_loss=" + numUnrecLoss +
        ", ");
    System.out.flush();  // typically not needed, but not guaranteed.

    if (persistMode != PersistMode.STREAMING) {
      // Wait for Store to get caught up.
      int numChecks = 0;
      while (curFlightSize.get() > 0) {
        numChecks++;
        if (numChecks > 3) {
          System.out.println("Giving up.");
          break;
        }
        System.out.println("Waiting for flight size " +
            curFlightSize + " to clear.");
        Thread.sleep(numChecks * 1000);
      }
    }
    assrt(numUnrecLoss == 0, "numUnrecLoss == 0");
    assrt(numRcvMsgs == actualSends, "numRcvMsgs == actualSends");

    deleteSource();

    myRcv.close();

    if (rcvThread == RcvThread.XSP) {
      if (optSequential) {
        myXspCtxThread.terminate();
      }
      myXsp.close();
    }

    if (optSequential) {
      myCtxThread.terminate();
    }
    myCtx.close();
  }  // run

}  // class UmLatPing


// Approximate clone of LBMContextThread class, modified for XSP.
class LBMXspThread extends Thread {
  private  LBMXSP _xsp = null;
  private long _msec = 1000;
  private volatile boolean _continueThread = true;
  private volatile boolean _running = false;
  private final static long DefaultMSec = 1000;

  public LBMXspThread(LBMXSP xsp) {   
    this(xsp, DefaultMSec);
  }

  public LBMXspThread(LBMXSP xsp, long msec) {
    super();
    _xsp = xsp;
    _msec = msec;
  }  // LBMXspThread

  public void terminate() throws Exception {
    _continueThread = false;

    _xsp.unblockProcessEvents();

    while (_running) {
      Thread.sleep(250);
    }
  }  // terminate

  public void run() {
    _running = true;
    while (_continueThread) {
      try {
        _xsp.processEvents(_msec);
      } catch (Exception e) {
        System.err.println("ERROR processing events: " + e.toString());
        System.exit(1);
      }
    }

    _running = false;
  }  // run
}  // LBMXspThread
