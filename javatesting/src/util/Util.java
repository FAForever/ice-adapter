package util;

import logging.Logger;

import java.io.IOException;
import java.net.ServerSocket;
import java.util.Random;

public class Util {
	private static final Random random = new Random();

	public static int getAvaiableTCPPort() {
		return getAvailableTCPPort(49152, 65535);
	}

	public static int getAvailableTCPPort(int min, int max) {
		while (true) {
			int randomPort = min + random.nextInt(max - min);
			if (isPortAvaiable(randomPort)) {
				return randomPort;
			}
		}
	}

	public static boolean isPortAvaiable(int port) {
		ServerSocket socket = null;
		try {
			socket = new ServerSocket(port);
		} catch (IOException e) {
			return false;
		} finally {
			if (socket != null) {
				try {
					socket.close();
				} catch (IOException e) {
					e.printStackTrace();
				}
			}
			return true;
		}
	}


	public static void assertThat(boolean b) {
		if(! b) {
			Throwable t = new Throwable();
			Logger.error("Assertion failed!!! %s->%s:%d", t.getStackTrace()[1].getClassName(), t.getStackTrace()[1].getMethodName(), t.getStackTrace()[1].getLineNumber());
		}
	}
}
