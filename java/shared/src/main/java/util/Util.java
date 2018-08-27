package util;

import logging.Logger;

import java.io.IOException;
import java.net.DatagramSocket;
import java.net.ServerSocket;
import java.net.SocketException;

public class Util {
//	private static final Random random = new Random();

	public static int getAvaiableTCPPort() {
		try {
			ServerSocket socket = new ServerSocket(0);
			int port = socket.getLocalPort();
			socket.close();
			return port;
		} catch (IOException e) {
			Logger.error("Could not find available TCP port.");
			Logger.crash(e);
			return -1;
		}
	}

	public static int getAvaiableUDPPort() {
		try {
			DatagramSocket socket = new DatagramSocket(0);
			int port = socket.getLocalPort();
			socket.close();
			return port;
		} catch (SocketException e) {
			Logger.error("Could not find available TCP port.");
			Logger.crash(e);
			return -1;
		}
	}

//	public static int getAvaiableTCPPort() {
//		return getAvailableTCPPort(49152, 65535);
//	}
//
//	public static int getAvailableTCPPort(int min, int max) {
//		while (true) {
//			int randomPort = min + random.nextInt(max - min);
//			if (isPortAvaiable(randomPort)) {
//				return randomPort;
//			}
//		}
//	}
//
//	public static boolean isPortAvaiable(int port) {
//		ServerSocket socket = null;
//		try {
//			socket = new ServerSocket(port);
//		} catch (IOException e) {
//			return false;
//		} finally {
//			if (socket != null) {
//				try {
//					socket.close();
//				} catch (IOException e) {
//					Logger.crash("Could not close socket while searching for availabe port");
//				}
//			}
//		}
//
//		return true;
//	}


	public static void assertThat(boolean b) {
		if(! b) {
			Throwable t = new Throwable();
			Logger.error("Assertion failed!!! %s->%s:%d", t.getStackTrace()[1].getClassName(), t.getStackTrace()[1].getMethodName(), t.getStackTrace()[1].getLineNumber());
		}
	}
}
