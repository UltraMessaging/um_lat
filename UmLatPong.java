/* UmLatPong.java - performance measurement tool. */
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
class UmLatPong implements LBMSourceEventCallback, LBMReceiverCallback, LBMTransportMappingCallback, LBMLogging {
  enum PersistMode { STREAMING, RPP, SPP };
  enum RcvThread { MAIN_CTX, XSP };
  enum SpinMethod { NO_SPIN, FD_MGT_BUSY };

  // Constants
  final int maxMsgLength = 1472;

  // Command-line options.
  String optConfig = "";
  boolean optExitOnEos = false;  // -E
  boolean optGenericSrc = false;
  String optPersistMode = "";
  String optRcvThread = "";  // -R
  boolean optSequential = false;  // -S
  String optSpinMethod = "";
  String optXmlConfig = "";

  // Parameters parsed out from command-line options.
  String appName = "um_perf";
  PersistMode persistMode = PersistMode.STREAMING;
  RcvThread rcvThread = RcvThread.MAIN_CTX;
  SpinMethod spinMethod = SpinMethod.NO_SPIN;

  // Globals.
  LBM lbm = null;
  int globalMaxTightSends = 0;
  int registrationComplete = 0;
  AtomicInteger curFlightSize = new AtomicInteger(0);
  int maxFlightSize = 0;


  public static void main(String[] args) throws Exception {
    // The body of the program is in the "run" method.
    UmLatPong application = new UmLatPong();
    application.run(args);
  }  // main


  public void LBMLog(int logLevel, String message) {
    // A real application should include a high-precision time stamp.
    System.out.println("AppLogger: logLevel=" + logLevel + ", message=" + message);
  }  // LBMLog


  private void assrt(boolean assertion, String errMessage) {
    if (! assertion) {
      System.err.println("UmLatPong: Error, " + errMessage + "\nUse '-h' for help");
      System.exit(1);
    }
  }  // assrt


  // Used when parsing options that have arguments.
  private String optArg(String[] args, int argNum) {                   
    if (argNum >= args.length)
    {               
      throw new IllegalArgumentException("Option '" + args[argNum - 1] + "' requires argument.");
    }               
    return args[argNum];
  }  // optArg


  private void help() {
    System.out.println("Usage: UmLatPong [-h] [-c config] [-g] [-p persist_mode] [-R rcv_thread] [-S] [-s spin_method] [-x xml_config]\n");
    System.out.println("Where (those marked with 'R' are required):\n" +
      "  -h : print help\n" +
      "  -c config : configuration file; can be repeated\n" +
      "  -g : generic source\n"  +
      "  -p persist_mode : '' (empty)=streaming, 'r'=RPP, 's'=SPP\n" +
      "  -R rcv_thread : '' (empty)=main context, 'x'=XSP\n" +
      "  -S : sequential context and XSP (if enabled with -R)\n" +
      "  -s spin_method : '' (empty)=no spin, 'f'=fd mgt busy\n" +
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
              assrt(false, "-p value must be '', 'r', or 's'");
            }
            break;
          case "-R":
            optRcvThread = optArg(args, argNum ++);
            if (optRcvThread.equalsIgnoreCase("")) {
              rcvThread = RcvThread.MAIN_CTX;
            } else if (optRcvThread.equalsIgnoreCase("x")) {
              rcvThread = RcvThread.XSP;
            } else {
              assrt(false, "-R value must be '' or 'x'");
            }
            break;
          case "-S": optSequential = true; break;
          case "-s":
            optSpinMethod = optArg(args, argNum ++);
            if (optSpinMethod.equalsIgnoreCase("")) {
              spinMethod = SpinMethod.NO_SPIN;
            } else if (optSpinMethod.equalsIgnoreCase("f")) {
              spinMethod = SpinMethod.FD_MGT_BUSY;
            } else {
              assrt(false, "-s value must be '' or 'f'");
            }
            break;
          case "-x":
            optXmlConfig = optArg(args, argNum ++);
            // Don't read it now since app_name might not be set yet.
            break;
          default:
            assrt(false, "unrecognized option '" + opt + "'");
        }  // switch
      } catch (Exception e) {
        assrt(false, "Exception processing option '" + opt + "':\n" + e);
      }  // try
    }  // while argNum

    // This program doesn't have positional parameters.
    assrt(argNum == args.length, "Unexpected parameter");

    // Waited to read xml config (if any) so that app_name is set up right.
    if (optXmlConfig.length() > 0) {
      LBM.setConfigurationXml(optXmlConfig, appName);
    }
  }  // getMyOpts


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
        assrt(curFlightSize.decrementAndGet() >= 0, "Negative flight size");
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
          assrt(curFlightSize.decrementAndGet() >= 0, "Negative flight size");
        }
        break;
      case LBM.SRC_EVENT_UME_DEREGISTRATION_SUCCESS_EX:
        break;
      case LBM.SRC_EVENT_UME_DEREGISTRATION_COMPLETE_EX:
        break;
      case LBM.SRC_EVENT_UME_MESSAGE_NOT_STABLE:
        break;
      default:
        System.err.println("handleSrcEvent: unexpected event: " + sourceEvent.type());
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
      ctxAttr.setProperty("ume_session_id", "0x7");  // Pong uses session ID 7.

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
      xspCtxAttr.setProperty("ume_session_id", "0x7");  // Pong uses session ID 7.

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
  LBMSSourceSendExInfo mySSrcExInfo = null;

  private void createSource(LBMContext ctx) throws Exception {
    LBMSourceAttributes srcAttr = new LBMSourceAttributes();
    // The "pong" program sends messages to "topic2".
    LBMTopic srcTopic = ctx.allocTopic("topic2", srcAttr);;
    if (optGenericSrc) {
      // Set up callback "onSourceEvent()" for UM to deliver source events.
      mySrc = ctx.createSource(srcTopic, this, null);
    }
    else {  // Smart Src API.
      mySSrc = new LBMSSource(myCtx, srcTopic);
      // Going to use a user-supplied buffer.
      mySSrcBuffer = mySSrc.buffGet();
      mySSrcExInfo = new LBMSSourceSendExInfo();
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
    // The "ping" program sends messages to "topic1", which we receive.
    LBMTopic rcvTopic = ctx.lookupTopic("topic1", rcvAttr);
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
        System.out.println("rcv event BOS, topicName='" + msg.topicName() +
            "', source=" + msg.source() + ",");
        System.out.flush();  // typically not needed, but not guaranteed.
        break;

      case LBM.MSG_EOS:
        System.out.println("rcv event EOS, '" + msg.topicName() +
            "', " + msg.source() + ", numRcvMsgs=" + numRcvMsgs +
            ", numRxMsgs=" + numRxMsgs + ", numUnrecLoss=" + numUnrecLoss + ",");
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
        int msgLength = (int)msg.dataLength();
        assrt(msgLength <= maxMsgLength, "Received message length > " + maxMsgLength);

        ByteBuffer message = msg.getMessagesBuffer();  // If SMX.
        if (message == null) {
          message = msg.dataBuffer();  // Not SMX.
        }

        numRcvMsgs++;

        try {
          if (optGenericSrc) {
            mySrc.send(message, 0, msgLength, 0);
          }
          else {  // Smart Source
            // User-supplied buffer supplied via exInfo.
            mySSrcExInfo.setUserSuppliedBuffer(message);
            mySSrc.send(mySSrcBuffer, 0, msgLength, 0, mySSrcExInfo);
          }
        }
        catch (Exception e) {
          System.err.println("Error sending: " + e.toString());
          System.exit(1);
        }

        // Keep track of recovered messages.
        if ((msg.flags() & LBM.MSG_FLAG_RETRANSMIT) != 0) {
          numRxMsgs++;
        }
        break;

      default:
        System.out.println("rcv event " + msg.type() +
            ", topicName='" + msg.topicName() +
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


  private void run(String[] args) throws Exception {
    lbm = new LBM();
    // Set up callback "LBMLog()" for UM log messages.
    lbm.setLogger(this);

    getMyOpts(args);

    System.out.println("optConfig=" + optConfig +
        ", optGenericSrc=" + optGenericSrc +
        ", optPersistMode=" + optPersistMode +
        ", optRcvThread=" + optRcvThread +
        ", optSequential=" + optSequential + ", optSpinMethod=" + optSpinMethod +
        ", optXmlConfig=" + optXmlConfig + ",");
    System.out.println("appName='" + appName +
        ", persistMode=" + persistMode +
        ", spinMethod=" + spinMethod);
    System.out.flush();  // typically not needed, but not guaranteed.

    createContext();

    createSource(myCtx);

    createReceiver(myCtx);

    /* The subscriber must be "kill"ed externally. */
    Thread.sleep(2000000000 * 1000);  // 23+ centuries.
  }  // run

}  // class UmLatPong


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
        System.err.println("Error processing events: " + e.toString());
        System.exit(1);
      }
    }

    _running = false;
  }  // run
}  // LBMXspThread
