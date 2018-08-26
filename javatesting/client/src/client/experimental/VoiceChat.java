package client.experimental;

import logging.Logger;

import javax.sound.sampled.*;
import java.util.Arrays;

public class VoiceChat {

	public static boolean ENABLED = false;
	public static AudioFormat format = new AudioFormat(44_100.0F, 32, 1, true, false);
	public static SourceDataLine sourceDataLine = null;
	public static TargetDataLine targetDataLine = null;

	public static void init() {
		if(ENABLED) {
			try {
				Logger.debug("Starting voice chat");
				DataLine.Info sourceInfo = new DataLine.Info(SourceDataLine.class, format);
				DataLine.Info targetInfo = new DataLine.Info(TargetDataLine.class, format);

				if(! AudioSystem.isLineSupported(sourceInfo)) {
					throw new LineUnavailableException("Source line not supported.");
				}

				if(! AudioSystem.isLineSupported(targetInfo)) {
					throw new LineUnavailableException("Target line not supported.");
				}

				sourceDataLine = (SourceDataLine) AudioSystem.getLine(sourceInfo);
				sourceDataLine.open(format);
				sourceDataLine.start();

				targetDataLine = (TargetDataLine) AudioSystem.getLine(targetInfo);
				targetDataLine.open(format);
				targetDataLine.start();

				new Thread(VoiceChat::listener).start();
				Logger.info("Started voice chat.");
			} catch(LineUnavailableException e) {
				Logger.error("Could not open audio data line.", e);
			}
		}
	}

	public static void receivedVoiceData(byte[] data) {
		sourceDataLine.write(data, 0, data.length - 1);
	}

	public static void listener() {
		while(targetDataLine.isOpen()) {
			byte[] data = new byte[targetDataLine.getBufferSize()];
			int bytesRead = targetDataLine.read(data, 0, data.length);
			receivedVoiceData(Arrays.copyOf(data, bytesRead - (bytesRead % (format.getFrameSize()))));
		}
	}


	public static void close() {
		if(ENABLED) {
			Logger.debug("Stopping voice chat.");
			if(sourceDataLine != null) {
				sourceDataLine.close();
			}
			if(targetDataLine != null) {
				targetDataLine.close();
			}
		}
	}
}
