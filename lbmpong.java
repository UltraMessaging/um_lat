import com.latencybusters.lbm.*;

import java.text.NumberFormat;
import java.nio.ByteBuffer;

/*
  (C) Copyright 2005,2022 Informatica LLC  Permission is granted to licensees to use
  or alter this software for any purpose, including commercial applications,
  according to the terms laid out in the Software License Agreement.

  This source code example is provided by Informatica for educational
  and evaluation purposes only.

  THE SOFTWARE IS PROVIDED "AS IS" AND INFORMATICA DISCLAIMS ALL WARRANTIES 
  EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION, ANY IMPLIED WARRANTIES OF 
  NON-INFRINGEMENT, MERCHANTABILITY OR FITNESS FOR A PARTICULAR 
  PURPOSE.  INFORMATICA DOES NOT WARRANT THAT USE OF THE SOFTWARE WILL BE 
  UNINTERRUPTED OR ERROR-FREE.  INFORMATICA SHALL NOT, UNDER ANY CIRCUMSTANCES, BE 
  LIABLE TO LICENSEE FOR LOST PROFITS, CONSEQUENTIAL, INCIDENTAL, SPECIAL OR 
  INDIRECT DAMAGES ARISING OUT OF OR RELATED TO THIS AGREEMENT OR THE 
  TRANSACTIONS CONTEMPLATED HEREUNDER, EVEN IF INFORMATICA HAS BEEN APPRISED OF 
  THE LIKELIHOOD OF SUCH DAMAGES.
*/

class lbmpong
{
	private static int msecpause = 0;
	private static int msecstartpause = 5000;
	private static int msgs = 200;
	private static int msglen = 10;
	private static boolean eventq = false;
	private static boolean sequential = true;
	private static boolean ping = false;
	private static int run_secs = 300;
	private static boolean verbose = false;
	private static boolean end_on_eos = false;
	private static boolean rtt_collect = false;
	private static boolean use_mim = false;
	private static boolean use_smx = false;
	private static int rtt_ignore = 0;
	private static String regid_offset = null;
	public static String prefix = "lbmpong";
	public static boolean joinlivestream = false;
	public static int enoregcount = 0;
	public static int ewouldblockcount = 0;
	public static int nostoreeventcount = 0;

	private static String purpose = "Purpose: Message round trip processor.";
	private static String usage =
"Usage: lbmpong [options] id\n"+ 
"Available options:\n"+ 
"  -c filename = Use LBM configuration file filename.\n"+ 
"                Multiple config files are allowed.\n"+ 
"                Example:  '-c file1.cfg -c file2.cfg'\n"+ 
"  -C = collect RTT data\n"+ 
"  -d = delay start (wait milliseconds to complete registration)\n"+ 
"  -E = exit after source ends\n"+ 
"  -e = use LBM embedded mode\n"+ 
"  -h = help\n"+ 
"  -i msgs = send and ignore msgs messages to warm up\n"+ 
"  -I = Use MIM\n"+ 
"  -j = join live stream (do not recover from stores)\n"+ 
"  -l len = use len length messages\n"+ 
"  -M msgs = stop after receiving msgs messages\n"+ 
"  -o offset = use offset to calculate Registration ID\n"+ 
"              (as source registration ID + offset)\n"+ 
"              offset of 0 forces creation of regid by store\n"+ 
"  -P msec = pause after each send msec milliseconds\n"+ 
"  -q = use an LBM event queue\n"+ 
"  -r [UM]DATA/RETR = Set transport type to LBT-R[UM], set data rate limit to\n"+ 
"                     DATA bits per second, and set retransmit rate limit to\n"+ 
"                     RETR bits per second.  For both limits, the optional\n"+ 
"                     k, m, and g suffixes may be used.  For example,\n"+ 
"                     '-R 1m/500k' is the same as '-R 1000000/500000'\n"+ 
"  -t secs = run for secs seconds\n"+ 
"  -T topic = topic name prefix (appended with '/' and id) [lbmpong]\n"+ 
"  -v = be verbose about each message (for RTT only)\n"+ 
"  id = either \"ping\" or \"pong\"\n"
;
	private static LBMContextThread ctxthread = null;

	private String optArg(String[] args, int argNum)
	{
		if (argNum >= args.length)
		{
			throw new IllegalArgumentException("Option '" + args[argNum - 1] + "' requires argument.");
		}
		if (args[argNum].charAt(0) == '-')
		{
			System.err.println("Warning, arg to '" + args[argNum - 1] + "' starts with '-'");
		}
		return args[argNum];
	}  // optArg

	public static void main(String[] args)
	{
		@SuppressWarnings("unused")
		lbmpong pongapp = new lbmpong(args);
	}

	int send_rate = 0;						//	Used for lbmtrm | lbtru transports
	int retrans_rate = 0;						//
	char protocol = '\0';						//
	LBM lbm = null;

	private void process_cmdline(String[] args)
	{
		boolean error = false;
		int argNum = 0;
		while (argNum < args.length)
		{
			if (args[argNum].charAt(0) != '-')
			{
				break;  // No more options, only positional parameters left.
			}
			if (args[argNum].length() != 2)
			{
				System.err.println("Error, '" + args[argNum] + "' not a valid option (use '-h' for help)");
				System.exit(1);
			}

			char c = args[argNum].charAt(1);  // Skip leading '-'.
			argNum ++;  // Consume option.
			try
			{
				switch (c)
				{
					case 'c':
						try 
						{
							LBM.setConfiguration(optArg(args, argNum ++));
						}
						catch (LBMException ex) 
						{
							System.err.println("Error setting LBM configuration: " + ex.toString());
							System.exit(1);
						}
						break;
					case 'C':
						rtt_collect = true;
						break;
					case 'd':
						msecstartpause = Integer.parseInt(optArg(args, argNum ++));
						break;
					case 'E':
						end_on_eos = true;
						break;
					case 'e':
						sequential = false;
						break;
					case 'h':
						print_help_exit(0);
					case 'i':
						rtt_ignore = Integer.parseInt(optArg(args, argNum ++));
						break;
					case 'I':
						use_mim = true;
						break;
					case 'j':
						joinlivestream = true;
						break;
					case 'l':
						msglen = Integer.parseInt(optArg(args, argNum ++));
						break;
					case 'M':
						msgs = Integer.parseInt(optArg(args, argNum ++));
						break;
					case 'o':
						regid_offset = optArg(args, argNum ++);
						break;
					case 'P':
						msecpause = Integer.parseInt(optArg(args, argNum ++));
						break;
					case 'q':
						eventq = true;
						break;
					case 'r':
						ParseRateVars parseRateVars = lbmExampleUtil.parseRate(optArg(args, argNum ++));
						if (parseRateVars.error) {
							print_help_exit(1);
						}
						protocol = parseRateVars.protocol;
						send_rate = parseRateVars.rate;
						retrans_rate = parseRateVars.retrans;
						break;
					case 't':
						run_secs = Integer.parseInt(optArg(args, argNum ++));
						break;
					case 'T':
						prefix = optArg(args, argNum ++);
						break;
					case 'v':
						verbose = true;
						break;
					default:
						error = true;
						break;
				}
				if (error)
					break;
			}
			catch (Exception e) 
			{
				/* type conversion exception */
				System.err.println("lbmpong: error\n" + e);
				print_help_exit(1);
			}
		}
		if (error || argNum >= args.length)
		{
			/* An error occurred processing the command line - print help and exit */
			print_help_exit(1);
		}
		if (args[argNum].equalsIgnoreCase("ping"))
		{
			ping = true;
		}
		else if (!args[argNum].equalsIgnoreCase("pong"))
		{
			System.err.println("else if (!args[gopt.getOptind()].equals(\"pong\"))");
			System.err.println(LBM.version());
			System.err.println(usage);
			System.exit(1);
		}
	}
	
	private static void print_help_exit(int exit_value)
	{
		System.err.println(LBM.version());
		System.err.println(purpose);
		System.err.println(usage);
		System.exit(exit_value);
	}

	public static void send_with_retry(LBMSource src, ByteBuffer message, int length, int flags, boolean pinger)
								throws LBMException {	
		boolean done = false;
		while (!done) {
			try {
				if (pinger) {
					message.putLong(0, System.nanoTime());
				}
				src.send(message, 0, length, flags);
				done = true;
			} catch (LBMException ex) {
				System.err.println("Error sending message: " + ex.toString());
				if ((ex instanceof UMENoRegException) || (ex instanceof LBMEWouldBlockException)) {
					if (!eventq) {
						System.err.println("Note: lbmpong can be configured to retry this send with the '-q' option.");
						throw ex;	// Not allowed to retry. Done.
					}

					// Print warning, update count, sleep, and retry send operation
					System.out.println("Retrying. The latency for this session of lbmpong will be skewed.");
					if (ex instanceof UMENoRegException)
						enoregcount++;
					else
						ewouldblockcount++;

					try {
						Thread.sleep(1000);
					} catch (InterruptedException e) {}; 
				} else {
					System.err.println("Unexpected error sending message: " + ex.toString());
					throw ex;	// Exception not handled here. Done.
				}
			}
		}
	}

	private lbmpong(String[] args)
	{	
		try
		{
			lbm = new LBM();
		}
		catch (LBMException ex)
		{
			System.err.println("Error initializing LBM: " + ex.toString());
			System.exit(1);
		}

		lbm.setLogger(new LBMLogging() {
			@Override
			public void LBMLog(int logLevel, String message) {
				// A real application should include a high-precision time stamp.
				System.out.println("appLogger: logLevel=" + logLevel + ", message=" + message);
			}
		});

		process_cmdline(args);

		if(use_mim && !eventq) {
			System.out.println("Using mim requires event queue to send from receive callback - forcing use");
			eventq = true;
		}
		if (msecpause > 0 && !eventq) {
			System.out.println("Setting pause value requires event queue - forcing use");
			eventq = true;
		}
		LBMSourceAttributes sattr = null;
		LBMContextAttributes cattr = null;
		try
		{
			sattr = new LBMSourceAttributes();
			cattr = new LBMContextAttributes();
		}
		catch (LBMException ex)
		{
			System.err.println("Error creating attributes: " + ex.toString());
			System.exit(1);
		}
		
		/* Check if protocol needs to be set to lbtrm | lbtru */
		if (protocol == 'U')
		{
			try
			{
				sattr.setProperty("transport", "LBTRU");
				cattr.setProperty("transport_lbtru_data_rate_limit", Integer.toString(send_rate));
				cattr.setProperty("transport_lbtru_retransmit_rate_limit", Integer.toString(retrans_rate));
			}
			catch (LBMRuntimeException ex)
			{
				System.err.println("Error setting LBTRU rate: " + ex.toString());
				System.exit(1);
			}														
		}		
		if (protocol == 'M') 
		{
			try
			{
				sattr.setProperty("transport", "LBTRM");
				cattr.setProperty("transport_lbtrm_data_rate_limit", Integer.toString(send_rate));
				cattr.setProperty("transport_lbtrm_retransmit_rate_limit", Integer.toString(retrans_rate));
			}
			catch (LBMRuntimeException ex)
			{
				System.err.println("Error setting LBTRM rates: " + ex.toString());
				System.exit(1);
			}
		}
		
		if (sequential)
		{
			cattr.setProperty("operational_mode", "sequential");
		}
		else
		{
			// The default for operational_mode is embedded, but set it
			// explicitly in case a configuration file was specified with
			// a different value.
			cattr.setProperty("operational_mode", "embedded");
		}
		LBMContext ctx = null;
		try
		{
			ctx = new LBMContext(cattr);
		}
		catch (LBMException ex)
		{
			System.err.println("Error creating context: " + ex.toString());
			System.exit(1);
		}
		PongLBMEventQueue evq = null;
		if (sequential)
		{
			// Run the context on a separate thread
			ctxthread = new LBMContextThread(ctx);
			ctxthread.start();
		}
		if (eventq)
		{
			if (sequential)
			{
				System.err.println("Sequential mode with event queue in use");
			}
			else
			{
				System.err.println("Embedded mode with event queue in use");
			}
			try
			{
				evq = new PongLBMEventQueue();
			}
			catch (LBMException ex)
			{
				System.err.println("Error creating event queue: " + ex.toString());
				System.exit(1);
			}
		}
		else if (sequential)
		{
			System.err.println("Sequential mode, no event queue");
		}
		else
		{
			System.err.println("Embedded mode, no event queue");
		}
		LBMSource src = null;
		PongLBMReceiver rcv = null;
		LBMTopic src_topic = null;
		LBMTopic rcv_topic = null;
		if(msglen < 8) msglen = 8;
		try
		{
			LBMReceiverAttributes rcv_attr = new LBMReceiverAttributes();
			if (regid_offset != null) {
				/* This is relevant for UME only, but enables this example to be used with persistent streams.
				 * There is no effect by doing this on non persistent streams or if an LBM only license is used
				 */
				PongRegistrationId umeregid;
				umeregid = new PongRegistrationId(regid_offset);
				rcv_attr.setRegistrationIdCallback(umeregid, null);
				System.out.println("Will use RegID offset " + regid_offset + ".");
			}
			if (joinlivestream) {
				UMERecoverySequenceNumberCallback umeseqcb = new PongSequenceAtLiveStream();
				rcv_attr.setRecoverySequenceNumberCallback(umeseqcb, null);
				System.out.println("Will join live stream (no recovery) ");
			}
			if (ping)
			{
				System.err.println("Sending " + msgs + " " + msglen
								   + " byte messages to topic " + prefix + "/ping pausing "
								   + msecpause + " msec between");
				if(!use_mim)
					src_topic = ctx.allocTopic(prefix + "/ping", sattr);
				rcv_topic = ctx.lookupTopic(prefix + "/pong",rcv_attr);
			}
			else
			{
				rcv_topic =  ctx.lookupTopic(prefix + "/ping",rcv_attr);
				if(!use_mim)
					src_topic =  ctx.allocTopic(prefix + "/pong", sattr);
			}
		}
		catch (LBMException ex)
		{
			System.err.println("Error setting up topics: " + ex.toString());
			System.exit(1);
		}
		PongSrcCB srccb = new PongSrcCB();
		try
		{
			if(!use_mim) {
				src = ctx.createSource(src_topic, srccb, null);
				use_smx = src.getAttributeValue("transport").toLowerCase().contains("smx");
			}

			if (use_smx) {
				/* Perform configuration validation */
				final int smx_header_size = 16;
				int max_payload_size =
					Integer.parseInt(src.getAttributeValue("transport_lbtsmx_datagram_max_size")) + smx_header_size;
				if (msglen > max_payload_size) {
					/* The SMX transport doesn't fragment, so payload must be within maximum size limits */
					System.out.println("Error: Message size requested is larger than configured SMX datagram size.");
					System.exit(1);
				}
			}
		}
		catch (LBMException ex)
		{
			System.err.println("Error creating source: " + ex.toString());
			System.exit(1);
		}
		try
		{
			rcv = new PongLBMReceiver(ctx,
						  rcv_topic,
						  evq,
						  src,
						  ping,
						  msecpause,
						  msgs,
						  verbose,
						  end_on_eos,rtt_collect,rtt_ignore,use_mim);
		}
		catch (LBMException ex)
		{
			System.err.println("Error creating pong receiver: " + ex.toString());
			System.exit(1);
		}
		try
		{
			Thread.sleep(msecstartpause);
		}
		catch (InterruptedException e) { }
		if (ping)
		{
			ByteBuffer message = ByteBuffer.allocateDirect(msglen);
			rcv.start();
			
			try
			{
				if(use_mim) {
					message.putLong(System.nanoTime());
					ctx.send(lbmpong.prefix + "/ping", message.array(), msglen, LBM.MSG_FLUSH);
				} else if (use_smx) {
					ByteBuffer buf = src.getMessagesBuffer();
					buf.position(src.acquireMessageBufferPosition(msglen, 0));
					buf.putLong(System.nanoTime());
					src.messageBuffersComplete();
				} else {
					send_with_retry(src, message, msglen, LBM.MSG_FLUSH, true);
				}
			}
			catch (LBMException ex)
			{
				System.err.println("Error sending message: " + ex.toString());
				System.exit(1);
			}
		}
		if (eventq)
		{
			evq.run(run_secs * 1000);
		}
		else
		{			
			try
			{
				Thread.sleep(run_secs * 1000);
			}
			catch (InterruptedException e) { }
		}
		if (ctxthread != null)
		{
			ctxthread.terminate();
		}
		if (ping)
		{
			rcv.print_latency(System.out);
			
			// print transport statistics
			try {
				print_stats(rcv, src);
			} catch (LBMException ex) {
				System.out.println("Error printing transport stats");
			}
				
			System.exit(0);
		}
						
		System.err.println("Quitting....");
	}

	/* Convert a signed byte to an unsigned int so it can be or'd */
//	private static int convert(byte b) {
//
//		if(b > 0) return (int) b;
//
//		if(b < 0) return 128 + (b & 0x7f);
//
//		return 0;
//	}
	
	public static int print_stats(LBMReceiver rcv, LBMSource src) throws LBMException
	{
		LBMReceiverStatistics rcv_stats = null;
		LBMSourceStatistics src_stats = null;
		String source_type = "";
		int nstats = 1;
		
		// Get receiver stats
		try {
			rcv_stats = rcv.getStatistics(nstats);
		} catch (LBMException ex) {
			System.err.println("Error getting receiver statistics: " + ex.toString());
			return -1;
		}
		
		if (rcv_stats == null) {
			System.err.println("Cannot print stats, because receiver stats are null");
			return -1;
		}
		
		// Get source stats
		try {
			src_stats = src.getStatistics();
		} catch (LBMException ex) {
			System.err.println("Error getting source statistics: " + ex.toString());
			return -1;
		}
		
		if (src_stats == null) {
			System.err.println("Cannot print stats, because source stats are null");
			return -1;
		}
				
		// Print transport stats
		switch(src_stats.type())
		{
			case LBM.TRANSPORT_STAT_TCP:
				break;
			case LBM.TRANSPORT_STAT_LBTRU:
			case LBM.TRANSPORT_STAT_LBTRM:
				if (rcv_stats.lost() != 0 || src_stats.retransmissionsSent() != 0) {
					source_type = (src_stats.type() == LBM.TRANSPORT_STAT_LBTRM) ? "LBT-RM" : "LBT-RU";
					System.out.println("The latency for this " + source_type + " session of lbmpong might be skewed by loss");
					System.out.println("Source loss: " + src_stats.retransmissionsSent() + "    " +
										  	 "Receiver loss: " + rcv_stats.lost());
				}
				break;
			case LBM.TRANSPORT_STAT_LBTIPC:
				break;
			case LBM.TRANSPORT_STAT_LBTSMX:
				break;
			case LBM.TRANSPORT_STAT_LBTRDMA:
				break;
		}
		System.out.flush();	
		rcv_stats.dispose();
		src_stats.dispose();

		if (enoregcount != 0) {
			System.out.println("The latency for this session of lbmpong has been skewed by registration error " + enoregcount + " time(s)");
			System.out.println("Please fix possible store misconfiguration (store name, network setup, heartbeat/timeout), or allow more time for registration -- and try again.");
		}
		if (ewouldblockcount != 0) {
			System.out.println("The latency for this session of lbmpong has been skewed by a blocking condition " + ewouldblockcount + " time(s)");
			System.out.println("Please slow down rate, use a different transport, or increase flight size -- and try again.");
		}
		if (nostoreeventcount != 0) {
			System.out.println("The latency for this session of lbmpong has been skewed by a store unresponsive condition " + nostoreeventcount + " time(s)");
			System.out.println("If not testing store failures, please fix possible store misconfiguration (store name, network setup, heartbeat/timeout) -- and try again.");
		}
		return 0;
	}				
}

class PongLBMEventQueue extends LBMEventQueue implements LBMEventQueueCallback
{
	/**
	 * 
	 */
	private static final long serialVersionUID = 1L;

	public PongLBMEventQueue() throws LBMException
	{
		super();
		addMonitor(this);
	}
	
	public void monitor(Object cbArg, int evtype, int evq_size, long evq_delay)
	{	
		System.err.println("Event Queue Monitor: Type: " + evtype +
			", Size: " + evq_size +
			", Delay: " + evq_delay + " usecs.");
	}
}

class PongLBMReceiver extends LBMReceiver
{
	/**
	 * 
	 */
	private static final long serialVersionUID = 1L;
	LBMContext _ctx;
	int _msg_count = 0;
	int _msgs = 200;
	int _msecpause = 0;
	boolean _ping = false;
	boolean _verbose = false;
	boolean _end_on_eos = false;
	LBMEventQueue _evq = null;
	LBMSource _src = null;
	@SuppressWarnings("unused")
	private long _start_time = 0;
	int rtt_ignore = 0;
	boolean use_mim = false;
	boolean use_smx = false;

	private final ByteBuffer _msgbuf;

	public PongLBMReceiver(LBMContext ctx, LBMTopic topic, LBMEventQueue evq, LBMSource src, 
						   boolean ping, int msecpause, int msgs, boolean verbose, boolean end_on_eos, 
						   boolean rtt_collect, int ignore, boolean mim) throws LBMException
	{
		super(ctx, topic, evq);
		_ctx = ctx;
		_msgs = msgs;
		_verbose = verbose;
		_msecpause = msecpause;
		_ping = ping;
		_evq = evq;
		_end_on_eos = end_on_eos;
		_src = src;
		use_smx = src.getAttributeValue("transport").toLowerCase().contains("smx");
		_msgbuf = (use_smx) ? _src.getMessagesBuffer() : null;
		//timer = new PongLBMTimer(this, ctx, 1000, evq);
		if(rtt_collect) rtt_data = new long[msgs];
			rtt_ignore = ignore;
		use_mim = mim;
	}

	public void start()
	{
		_start_time = System.currentTimeMillis();
	}
 
	protected int onReceive(LBMMessage msg)
	{
		long t = (_ping) ? System.nanoTime() : 0L;
		long s = 0;
		switch (msg.type())
		{
			case LBM.MSG_DATA:
				if(rtt_ignore == 0) {
					_msg_count++;
				}
				ByteBuffer message = msg.getMessagesBuffer();
				if (message == null) {
					message = msg.dataBuffer();
				}
				s = message.getLong();
				if (_ping)
				{
					calc_latency(t,s);
					if (rtt_ignore == 0 && _msg_count == _msgs)
					{
						rtt_avg = curr_nsec/_msg_count;

						print_rtt_data();

						print_latency(System.out);
						try {
							lbmpong.print_stats(this, _src);
						}
						catch (LBMException ex) {
							System.out.println("Error printing transport stats");
						}
						
						System.exit(0);
					}
					if (_msecpause > 0)
					{
						// This would not normally be a good
						// thing in a callback on the context
						// thread.
						try
						{
							Thread.sleep(_msecpause);
						}
						catch (InterruptedException e) { }
					}

					/*  Next Message */
					if (!use_smx)
                                       		message.putLong(0, System.nanoTime());
				}
				try
				{
					if(use_mim)
						_ctx.send(_ping ? lbmpong.prefix + "/ping" : lbmpong.prefix + "/pong",
							message.array(), message.array().length, LBM.MSG_FLUSH | LBM.SRC_NONBLOCK);
					else if (use_smx){
	                                	_msgbuf.position(_src.acquireMessageBufferPosition((int)msg.dataLength(), 0));
						if (_ping)
							_msgbuf.putLong(System.nanoTime());
						else
                                        		_msgbuf.putLong(s);
						_src.messageBuffersComplete();
					} else
						lbmpong.send_with_retry(_src, message, (int)msg.dataLength(), LBM.MSG_FLUSH | LBM.SRC_NONBLOCK, _ping);
				}
				catch (LBMException ex)
				{
					System.err.println("Error sending message: " + ex.toString());
				}
				if(_ping) {
					if(_verbose) {
						System.out.println(_msg_count + " curr " + t + " sent " + s + " latency " + (t - s) + " ns");
					}
				}
				if(rtt_ignore > 0) rtt_ignore--;
				break;
			case LBM.MSG_BOS:
				System.err.println("[" + msg.topicName() + "][" + msg.source() + "], Beginning of Transport Session");
				break;
			case LBM.MSG_EOS:
				System.err.println("[" + msg.topicName() + "][" + msg.source() + "], End of Transport Session");
				if (_end_on_eos)
				{
					end();
				}
				break;
			case LBM.MSG_UNRECOVERABLE_LOSS:
				if (_verbose)
					System.err.println("[" + msg.topicName() + "][" + msg.source() + "][" + msg.sequenceNumber() + "], LOST");
				/* Any kind of loss makes this test invalid */
				System.out.println("Unrecoverable loss occurred.  Quitting...");	
				System.exit(1);
				break;
			case LBM.MSG_UNRECOVERABLE_LOSS_BURST:
				System.err.println("[" + msg.topicName() + "][" + msg.source() + "], LOST BURST");
				/* Any kind of loss makes this test invalid */
				System.out.println("Unrecoverable loss occurred.  Quitting...");	
				System.exit(1);
				break;
			case LBM.MSG_UME_REGISTRATION_SUCCESS_EX:
			case LBM.MSG_UME_REGISTRATION_COMPLETE_EX:
				/* Provided to enable quiet usage of lbmstrm with UME */
				break;
			default:
				System.out.println("Unhandled receiver event [" + msg.type() + "] from source [" +  msg.source() + "] with topic [" + msg.topicName() + "]. Refer to https://ultramessaging.github.io/currdoc/doc/java_example/index.html#unhandledjavaevents for a detailed description.");
				break;
		}
		msg.dispose();
		return 0;
	}

	long curr_nsec;
	long min_nsec,max_nsec;
	long min_nsec_idx,max_nsec_idx;

	int datanum = 0;
	long rtt_data[];
	long rtt_median,rtt_avg,rtt_stddev;

	public void calc_latency(long curr,long sent)
	{
		long diff = curr - sent;
		if (rtt_ignore == 0) {
			curr_nsec += diff;
			if (diff < min_nsec || min_nsec == 0) { min_nsec = diff; min_nsec_idx = datanum; }
			if (diff > max_nsec || max_nsec == 0) { max_nsec = diff; max_nsec_idx = datanum; }
			if (rtt_data != null) rtt_data[datanum] = diff;
			datanum++;
		}
	}
	
	long calc_med() {
		int r;
		boolean changed;
		long t;
		long msgs = _msg_count;

		/* sort the result set */
		do {
			changed = false;
		
			for(r = 0;r < msgs - 1;r++) {
				if(rtt_data[r] > rtt_data[r + 1]) {
					t = rtt_data[r];
					rtt_data[r] = rtt_data[r + 1];
					rtt_data[r + 1] = t;
					changed = true;
				}
			}
		} while(changed);

		if((msgs & 1) == 1) {
			/* Odd number of data elements - take middle */
			return rtt_data[(int)(msgs / 2) + 1];
		} else {
			/* Even number of data element avg the two middle ones */
			return (rtt_data[(int)(msgs / 2)] + rtt_data[((int)msgs / 2) + 1]) / 2;
		}
	}

	long calc_stddev(long mean) {
        	int r;
        	long sum;

        	/* Subtract the mean from the data points, square them and sum them */
        	sum = 0;
        	for(r = 0;r < _msg_count;r++) {
                	rtt_data[r] -= mean;
                	rtt_data[r] *= rtt_data[r];
                	sum += rtt_data[r];
        	}

        	sum /= (_msg_count - 1);

        	return (long)(Math.sqrt(sum));
	}

	public void print_rtt_data() {
		if(rtt_data != null) {
			int r;
			NumberFormat nf = NumberFormat.getInstance();
			nf.setMaximumFractionDigits(4);

			for(r = 0;r < _msg_count;r++)
				System.err.println("RTT " + nf.format(rtt_data[r]) + " nsec, msg " + r);

			/* Calculate median and stddev */
			rtt_median = calc_med();
			rtt_stddev = calc_stddev(rtt_avg);

			print_latency(System.err);
		}
	}

	public void print_latency(java.io.PrintStream fstr)
	{
		long latency = 0;

		NumberFormat nf = NumberFormat.getInstance();
		nf.setMaximumFractionDigits(4);
		if(rtt_data != null) {
			fstr.println("min/max msg = " + nf.format(min_nsec_idx) + "/"
					+ nf.format(max_nsec_idx)
					+ " median/stddev "
					+ nf.format(rtt_median) + "/"
					+ nf.format(rtt_stddev) +" msec"); 
		}
		latency = rtt_avg / 2;
		fstr.println("Elapsed time " + curr_nsec + " nsecs "
				   + _msg_count + " messages (RTTs). "
				   + "min/avg/max " + nf.format(min_nsec)
				   + "/" + nf.format(rtt_avg) 
				   + "/" + nf.format(max_nsec)
				   + " nsec RTT");
		fstr.println("        " + nf.format(latency) + " nsec latency one-way");
	}
	
	private void end()
	{
		if (_evq != null)
		{
			_evq.stop();
		}
		else
		{
			System.exit(0);
		}
	}

//	private long ba2l(byte[] b, int offset)
//	{
//		long value = 0;
//		for (int i = 0; i < 4; i++)
//		{
//			int shift = (3-i) * 8;
//			value += (b[i+offset] & 0x000000ff) << shift;
//		}
//		return value;
//	}
}

class PongRegistrationId implements UMERegistrationIdExCallback {
	private long _regid_offset;

	public PongRegistrationId(String regid_offset) {
		try {
			_regid_offset = Long.parseLong(regid_offset);
		} catch (Exception ex) {
			System.err
					.println("Can't convert registration ID offset to a long: "
							+ ex.toString());
			System.exit(1);
		}
	}

	public long setRegistrationId(Object cbArg, UMERegistrationIdExCallbackInfo cbInfo) {
		long regid = (_regid_offset == 0 ? 0 : cbInfo.sourceRegistrationId() + _regid_offset);
		if (regid < 0) {
			System.out.println("Would have requested registration ID [" + regid + "], but negative registration IDs are invalid.");
			regid = 0;
		}

		System.out.println("Store " + cbInfo.storeIndex() + ": " + cbInfo.store() + "["
				+ cbInfo.source() + "][" + cbInfo.sourceRegistrationId() + "] Flags " + cbInfo.flags()
				+ ". Requesting regid: " + regid);
		return regid;
	}
}

class PongSequenceAtLiveStream implements UMERecoverySequenceNumberCallback {
	public int setRecoverySequenceNumberInfo(java.lang.Object cbArg,
				UMERecoverySequenceNumberCallbackInfo cbInfo) {
		try {
			// If the live stream is ahead of the recovery point by max integer,
			// we will assume that it is a fresh restart
			long low = cbInfo.lowSequenceNumber();
			long high = cbInfo.highSequenceNumber();

			if (high - low < Integer.MAX_VALUE) {
				cbInfo.setLowSequenceNumber(high + 1);
			}
			return 0;
		} catch(LBMEInvalException ex) {
			System.err.println("Live Stream Failure: " + ex.toString());
			System.exit(1);
		}
		return -1;
	}
}

class PongSrcCB implements LBMSourceEventCallback
{
	public boolean blocked = false;

	public int onSourceEvent(Object arg, LBMSourceEvent sourceEvent)
	{
		String clientname;

		switch (sourceEvent.type())
		{
		case LBM.SRC_EVENT_CONNECT:
			clientname = sourceEvent.dataString();
			System.out.println("Receiver connect " + clientname);
			break;
		case LBM.SRC_EVENT_DISCONNECT:
			clientname = sourceEvent.dataString();
			System.out.println("Receiver disconnect " + clientname);
			break;
		case LBM.SRC_EVENT_WAKEUP:
			blocked = false;
			break;
		case LBM.SRC_EVENT_UME_REGISTRATION_SUCCESS_EX:
		case LBM.SRC_EVENT_UME_REGISTRATION_COMPLETE_EX:
		case LBM.SRC_EVENT_UME_DELIVERY_CONFIRMATION_EX:
			/* Provided to enable quiet usage of lbmstrm with UME */
			break;
		case LBM.SRC_EVENT_UME_STORE_UNRESPONSIVE:
			lbmpong.nostoreeventcount++;
			break;
		default:
			System.out.println("Unhandled source event [" + sourceEvent.type() + "]. Refer to https://ultramessaging.github.io/currdoc/doc/java_example/index.html#unhandledjavaevents for a detailed description.");
			break;
		}
		System.out.flush();	
		return 0;
	}
}

