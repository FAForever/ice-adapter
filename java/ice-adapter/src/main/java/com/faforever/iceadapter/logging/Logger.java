package com.faforever.iceadapter.logging;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Date;

public class Logger {
	public static final int ERROR = 0;
	public static final int WARNING = 1;
	public static final int INFO = 2;
	public static final int DEBUG = 3;

	public static String collectedLog = "";

	public static boolean loggingEnabled = false;
	public static int logLevel = DEBUG;


	static FileWriter fileWriter;
	private static long lastFlush = 0;

    public synchronized static void init(String programTitle, File file) {
		if (fileWriter != null)
			return;

		try {
			Date date = new Date();
			System.out.println(programTitle + "\n");
			System.out.println("Logging started:   " + date.toString());

            initFileWriter(programTitle, date, file);
		} catch (IOException e) {
			System.err.println("Could not initialize logging!");
		}
	}

    private synchronized static void initFileWriter(String programTitle, Date date, File file) throws IOException {
		if (loggingEnabled) {
            fileWriter = new FileWriter(file);
			fileWriter.write(programTitle + "\n\n");
			fileWriter.write("Logging started:   " + date.toString() + "\n");
			fileWriter.flush();
		}
	}

	public synchronized static void close() {
		try {
			if (fileWriter != null) {
				fileWriter.write("Logging stopped");
				fileWriter.flush();
				fileWriter.close();
			}
		} catch (IOException e) {
			System.err.println("Could not close log file!");
		}

//		System.out.println("Logging stopped");
	}

	public synchronized static void crash(String string) {
		error(string);
		crash(new RuntimeException(string));
	}

	public synchronized static void crash(String string, Exception e) {
		error(string);
		crash(e);
	}

	public synchronized static void crash(Exception e) {
		if (logLevel >= ERROR) {
			Date date = new Date();
			String logString = "[CRASH] " + (date.getHours() < 10 ? "0" + date.getHours() : date.getHours()) + ":" + (date.getMinutes() < 10 ? "0" + date.getMinutes() : date.getMinutes()) + ":" + (date.getSeconds() < 10 ? "0" + date.getSeconds() : date.getSeconds()) + " :   " + e.getCause() + "  " + e.getMessage() + "\n";
			System.err.print(logString);

			writeToLog(logString);
			for (StackTraceElement element : e.getStackTrace()) {
				logString = "[STACKTRACE]:\t\t" + element.toString() + "\n";
				System.err.print(logString);
				writeToLog(logString);
			}
			close();
		}

		System.exit(-1);
	}

	public synchronized static void debug(String string, Object... args) {
		if (args.length > 0) {
			string = String.format(string, args);
		}

		if (logLevel >= DEBUG) {
			Date date = new Date();
			String logString = "[DEBUG] " + (date.getHours() < 10 ? "0" + date.getHours() : date.getHours()) + ":" + (date.getMinutes() < 10 ? "0" + date.getMinutes() : date.getMinutes()) + ":" + (date.getSeconds() < 10 ? "0" + date.getSeconds() : date.getSeconds()) + ":\t\t" + string + "\n";
			System.out.print(logString);
			writeToLog(logString);
		}
	}

	public synchronized static void info(String string, Object... args) {
		if (args.length > 0) {
			string = String.format(string, args);
		}
		if (logLevel >= INFO) {
			Date date = new Date();
			String logString = "[INFO] " + (date.getHours() < 10 ? "0" + date.getHours() : date.getHours()) + ":" + (date.getMinutes() < 10 ? "0" + date.getMinutes() : date.getMinutes()) + ":" + (date.getSeconds() < 10 ? "0" + date.getSeconds() : date.getSeconds()) + ":\t\t" + string + "\n";
			System.out.print(logString);
			writeToLog(logString);
		}
	}

	public synchronized static void warning(String string, Object... args) {
		if (args.length > 0) {
			string = String.format(string, args);
		}

		if (logLevel >= WARNING) {
			Date date = new Date();
			String logString = "[WARNING] " + (date.getHours() < 10 ? "0" + date.getHours() : date.getHours()) + ":" + (date.getMinutes() < 10 ? "0" + date.getMinutes() : date.getMinutes()) + ":" + (date.getSeconds() < 10 ? "0" + date.getSeconds() : date.getSeconds()) + ":\t\t" + string + "\n";
			System.out.print(logString);
			writeToLog(logString);
		}
	}

	public synchronized static void error(String string, Object... args) {
		if (args.length > 0) {
			string = String.format(string, args);
		}

		if (logLevel >= ERROR) {
			Date date = new Date();
			String logString = "[ERROR] " + (date.getHours() < 10 ? "0" + date.getHours() : date.getHours()) + ":" + (date.getMinutes() < 10 ? "0" + date.getMinutes() : date.getMinutes()) + ":" + (date.getSeconds() < 10 ? "0" + date.getSeconds() : date.getSeconds()) + ":\t\t" + string + "\n";
			System.err.print(logString);
			writeToLog(logString);
		}
	}

	public synchronized static void error(Exception e) {
		if (logLevel >= ERROR) {
			Date date = new Date();
			String logString = "[ERROR] " + (date.getHours() < 10 ? "0" + date.getHours() : date.getHours()) + ":" + (date.getMinutes() < 10 ? "0" + date.getMinutes() : date.getMinutes()) + ":" + (date.getSeconds() < 10 ? "0" + date.getSeconds() : date.getSeconds()) + ":\t\t" + e.getMessage() + "\n";
			System.err.print(logString);
			writeToLog(logString);

			for (StackTraceElement element : e.getStackTrace()) {
				logString = "[STACKTRACE]:\t\t" + element.toString() + "\n";
				System.err.print(logString);
				writeToLog(logString);
			}
		}
	}

	public synchronized static void error(String string, Exception e) {
		error(string);
		error(e);
	}

	private synchronized static void writeToLog(String logString) {
		collectedLog = collectedLog.concat(logString);

		if (loggingEnabled) {
			try {
				fileWriter.write(logString);
				if (System.currentTimeMillis() - lastFlush > 2000) {
					fileWriter.flush();
					lastFlush = System.currentTimeMillis();
				}
			} catch (IOException e) {
				System.err.println("Could not write to log file!");
			}
		}
	}

	public synchronized static void enableLogging() {
		loggingEnabled = true;
	}

	public synchronized static void disableLogging() {
		loggingEnabled = false;
	}

	public static void setLogLevel(int level) {
		logLevel = level;
	}
}
