package client.ice;

import client.GUI;
import client.TestClient;
import client.nativeAccess.NativeAccess;
import com.google.gson.GsonBuilder;
import com.nbarraille.jjsonrpc.CallbackMethod;
import com.nbarraille.jjsonrpc.InvalidMethodException;
import com.nbarraille.jjsonrpc.JJsonPeer;
import com.nbarraille.jjsonrpc.TcpClient;
import data.IceStatus;
import javafx.beans.property.BooleanProperty;
import javafx.beans.property.SimpleBooleanProperty;
import javafx.scene.control.Alert;
import logging.Logger;
import util.OsUtil;
import util.Util;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.reflect.Field;
import java.net.ConnectException;
import java.nio.charset.Charset;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.*;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;

import static com.github.nocatch.NoCatch.noCatch;

public class ICEAdapter {

	private static final String LOG_LEVEL = "info";

	private static final int CONNECTION_ATTEMPTS = 20;

	private static final String COTURN_HOST = "geosearchef.de";
	private static final String COTURN_KEY = "8T2o1yfSu29vf9cJ3WjHS9Ak6zJCB6qECL2Uxlza";
//	private static final String COTURN_HOST = "vmrbg145.informatik.tu-muenchen.de";
//	private static final Str ing COTURN_KEY = "banana";

	public static int ADAPTER_PORT;//RPC Client <=> ICE
	public static int GPG_PORT;//ICE <=> Lobby
	public static int LOBBY_PORT;//forged alliance udp receive point (from other games)

	public static int EXTERNAL_ADAPTER_PORT = -1;//Indicates if an ICE adapter is already running and should be connected to

	private static Process process;
	private static TcpClient tcpClient;
	private static JJsonPeer peer;

	public static BooleanProperty connected = new SimpleBooleanProperty(false);
	public static int processID = 0;
	public static long processHandle = 0;

	private static IceAdapterCallbacks iceAdapterCallbacks = new IceAdapterCallbacks();

	public static void init() {
		Alert alert = noCatch(() -> GUI.showDialog("Starting ICE adapter...").get());
		startICEAdapter();

		try { Thread.sleep(500); } catch(InterruptedException e) {}

		connectToICEAdapter();
		configureIceServers();

		connected.set(true);

		GUI.runAndWait(alert::close);

		try { Thread.sleep(500); } catch(InterruptedException e) {}
	}


	public static void hostGame(String mapName) {
		peer.sendAsyncRequest("hostGame", Arrays.asList(mapName), null, false);
	}

	public static void joinGame(String remotePlayerLogin, long remotePlayerId) {
		peer.sendAsyncRequest("joinGame", Arrays.asList(remotePlayerLogin, remotePlayerId), null, false);
	}
	public static void connectToPeer(String remotePlayerLogin, long remotePlayerId, boolean offer) {
		peer.sendAsyncRequest("connectToPeer", Arrays.asList(remotePlayerLogin, remotePlayerId, offer), null, false);
	}

	public static void disconnectFromPeer(long remotePlayerId) {
		peer.sendAsyncRequest("disconnectFromPeer", Arrays.asList(remotePlayerId), null, false);
	}

	public static void setLobbyInitMode(String lobbyInitMode) {
		peer.sendAsyncRequest("setLobbyInitMode", Arrays.asList(lobbyInitMode), null, false);
	}

	public static void iceMsg(long remotePlayerId, Object msg) {
		peer.sendAsyncRequest("iceMsg", Arrays.asList(remotePlayerId, msg), null, false);
	}

	public static void sendToGpgNet(String header, String... chunks) {
		peer.sendAsyncRequest("sendToGpgNet", Arrays.asList(header, chunks), null, false);
	}

	public static void setIceServers(List<Map<String, Object>> iceServers) {
		peer.sendAsyncRequest("setIceServers", Arrays.asList(iceServers), null, false);
	}

	private static volatile CompletableFuture<Object> status = null;
	public synchronized static IceStatus status() {
//		long time = System.currentTimeMillis();
		status = new CompletableFuture<>();
		IceStatus iceStatus = null;
		try {
			peer.sendAsyncRequest("status", Collections.emptyList(), new CallbackMethod(StatusCallback.class.getMethod("statusCallback", Object.class), STATUS_CALLBACK_INSTANCE, null), false);
			iceStatus = new GsonBuilder().serializeNulls().create().fromJson(status.get().toString().replace("{}", "null"), IceStatus.class);
		} catch (InvalidMethodException | NoSuchMethodException | InterruptedException | ExecutionException e) {
			Logger.error(e);
			e.printStackTrace();
		}
//		Logger.debug("Took %d ms.", System.currentTimeMillis() - time);
		latestIceStatus = iceStatus;

		return iceStatus;
	}
	public static IceStatus latestIceStatus = null;

	private static StatusCallback STATUS_CALLBACK_INSTANCE = new StatusCallback();
	public static class StatusCallback {
		public void statusCallback(Object res) {
			status.complete(res);
		}
	}


	private static void configureIceServers() {
		int timestamp = (int)(System.currentTimeMillis() / 1000) + 3600 * 24;
		String tokenName = String.format("%s:%s", timestamp, TestClient.username);
		byte[] secret = null;
		try {
			Mac mac = Mac.getInstance("HmacSHA1");
			mac.init(new SecretKeySpec(Charset.forName("cp1252").encode(COTURN_KEY).array(), "HmacSHA1"));
			secret = mac.doFinal(Charset.forName("cp1252").encode(tokenName).array());

		} catch(NoSuchAlgorithmException | InvalidKeyException e) { Logger.crash(e); }
		String authToken = Base64.getEncoder().encodeToString(secret);

		Map<String, Object> map = new HashMap<>();
		map.put("urls", Arrays.asList(
				String.format("turn:%s?transport=tcp", COTURN_HOST),
				String.format("turn:%s?transport=udp", COTURN_HOST),
				String.format("stun:%s", COTURN_HOST)
		));

		map.put("credential", authToken);
		map.put("credentialType", "authToken");
		map.put("username", tokenName);

		setIceServers(Arrays.asList(map));
		Logger.debug("Set ICE servers");
	}

	private static void connectToICEAdapter() {
		for (int attempt = 0; attempt < CONNECTION_ATTEMPTS; attempt++) {
			try {
				tcpClient = new TcpClient("localhost", ADAPTER_PORT, iceAdapterCallbacks);
				peer = tcpClient.getPeer();

				break;
			} catch (ConnectException e) {
				Logger.debug("Could not connect to ICE adapter (attempt %s/%s)", attempt, CONNECTION_ATTEMPTS);
				try { Thread.sleep(50); } catch(InterruptedException e2) {}
			} catch (IOException e) {
				Logger.crash(e);
			}
		}

		if(peer == null) {
			Logger.error("Could not connect to ICE adapter.");
			System.exit(45);
		}

		Logger.debug("Connected to ICE adapter via JsonRPC.");

		//Read ports
		IceStatus iceStatus = status();
		GPG_PORT = iceStatus.getOptions().getGpgnet_port();
		LOBBY_PORT = iceStatus.getLobby_port();

		Logger.info("Got generated ports from IceAdapterStatus:   LOBBY_PORT: %d   GPGNET_PORT: %d", LOBBY_PORT, GPG_PORT);
	}


	public static void startICEAdapter() {
		Logger.info("Launching ICE adapter...");

		if(EXTERNAL_ADAPTER_PORT == -1) {
			ADAPTER_PORT = Util.getAvaiableTCPPort();
		} else {
			ADAPTER_PORT = EXTERNAL_ADAPTER_PORT;
		}

		//Ports are now being generated by ICE adapter
//		GPG_PORT = Util.getAvaiableTCPPort();
//		LOBBY_PORT = Util.getAvaiableTCPPort();

//		if (ADAPTER_PORT == GPG_PORT) {
//			Logger.crash(new RuntimeException("ADAPTER_PORT = GPG_PORT, you got very unlucky, this is NO error, just try again!"));
//		}

		if(EXTERNAL_ADAPTER_PORT == -1) {
			String command[] = new String[]{
//					(System.getProperty("os.name").contains("Windows") ? "faf-ice-adapter.exe" : "./faf-ice-adapter"),
					"java",
					"-jar",
					"faf-ice-adapter.jar",
					"--id", String.valueOf(TestClient.playerID),
					"--login", TestClient.username,
					"--rpc-port", String.valueOf(ADAPTER_PORT),
//				"--gpgnet-port", String.valueOf(GPG_PORT), retrieved afterwards
//				"--lobby-port", String.valueOf(LOBBY_PORT),
					"--log-level", LOG_LEVEL,
//					"--log-directory", "iceAdapterLogs/"
			};

			ProcessBuilder processBuilder = new ProcessBuilder(command);
//			processBuilder.inheritIO();


            Logger.debug("Command: %s", Arrays.stream(command).reduce("", (l, r) -> l + " " + r));
			try {
				process = processBuilder.start();
			} catch (IOException e) {
				Logger.error("Could not start ICE adapter", e);
				System.exit(10);
			}

            try {
                BufferedWriter iceAdapterLogWriter = new BufferedWriter(new FileWriter(new File(String.format("faf-ice-adapter_%s.log", TestClient.username))));
                OsUtil.gobbleLines(process.getInputStream(), s -> noCatch(() -> iceAdapterLogWriter.write(s + "\n")));
                OsUtil.gobbleLines(process.getErrorStream(), s -> noCatch(() -> iceAdapterLogWriter.write(s + "\n")));

                Thread t = new Thread(() -> {
                    while(true) {
                        noCatch(() -> Thread.sleep(5000));
                        noCatch(() -> iceAdapterLogWriter.flush());
                    }
                });
                t.setDaemon(true);
                t.start();
            } catch (IOException e) {
                Logger.warning("Could not open output writer for ice adapter log.");
            }

			if (!process.isAlive()) {
				Logger.error("ICE Adapter not running");
				System.exit(11);
			}

			//Read pid
			if (process.getClass().getName().equals("java.lang.Win32Process") || process.getClass().getName().equals("java.lang.ProcessImpl")) {
				try {
					Field f = process.getClass().getDeclaredField("handle");
					f.setAccessible(true);
					processHandle = f.getLong(process);
				} catch (Throwable e) {
					e.printStackTrace();
				}
			} else if(process.getClass().getName().equals("java.lang.UNIXProcess")) {
				/* get the PID on unix/linux systems */
				try {
					Field f = process.getClass().getDeclaredField("pid");
					f.setAccessible(true);
					processID = f.getInt(process);
					Logger.debug("ICEAdapter PID: %d", processID);
				} catch (Throwable e) {
					e.printStackTrace();
				}
			}

//			IceAdapter.main(Arrays.copyOfRange(command, 1, command.length));
		}

		Logger.info("Launched ICE adapter.");
	}


	public static void sigStop() {
		if(processID != 0) {

//			try {
//				Runtime.getRuntime().exec("kill -SIGSTOP " + processID);
			NativeAccess.sendSignal(processID, NativeAccess.SIGSTOP);
			Logger.warning("Sent SIGSTOP to %d", processID);
//			} catch (IOException e) {
//				Logger.error("Could not sigstop process %d", processID);
//			}
		}

		if(processHandle != 0) {
			NativeAccess.suspendWin32(processHandle);
		}
	}

	public static void sigCont() {
		if(processID != 0) {
//			try {
//				Runtime.getRuntime().exec("kill -SIGCONT " + processID);
			NativeAccess.sendSignal(processID, NativeAccess.SIGCONT);
			Logger.warning("Sent SIGCONT to %d", processID);
//			} catch (IOException e) {
//				Logger.error("Could not sigcont process %d", processID);
//			}
		}

		if(processHandle != 0) {
			NativeAccess.resumeWin32(processHandle);
		}
	}

	public static void sigKill() {
		if(processID != 0) {
//			try {
//				Runtime.getRuntime().exec("kill -9" + processID);
			NativeAccess.sendSignal(processID, NativeAccess.SIGKILL);
			Logger.warning("Sent SIGKILL to %d", processID);
//			} catch (IOException e) {
//				Logger.error("Could not sigkill process %d", processID);
//			}
		}
	}

	public static void close() {
		Logger.debug("Closing ICE adapter...");
		if (peer != null && peer.isAlive()) {
			peer.sendAsyncRequest("quit", Collections.emptyList(), null, false);
			try {
				Thread.sleep(500);
			} catch (InterruptedException e) {
			}
		}

		if (process != null && process.isAlive()) {
			Logger.debug("ICE adapter running, killing...");
			process.destroyForcibly();
		}

		connected.set(false);
	}
}
