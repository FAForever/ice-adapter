package client;

import client.experimental.VoiceChat;
import client.forgedalliance.ForgedAlliance;
import client.ice.ICEAdapter;
import data.IceStatus;
import javafx.beans.property.BooleanProperty;
import javafx.beans.property.SimpleBooleanProperty;
import logging.Logger;
import net.ClientInformationMessage;
import net.ScenarioOptionsMessage;

import java.util.LinkedList;
import java.util.regex.Pattern;

public class TestClient {

	public static boolean DEBUG_MODE = false;
	private static final int INFORMATION_INTERVAL = 2500;

	public static String username;
	public static int playerID;

	public static ForgedAlliance forgedAlliance;
	public static BooleanProperty isGameRunning = new SimpleBooleanProperty(false);

	public static ScenarioOptionsMessage scenarioOptions = new ScenarioOptionsMessage();

	public static void joinGame(int gpgpnetPort, int lobbyPort) {
		Logger.info("Starting ForgedAlliance.");
		forgedAlliance = new ForgedAlliance(gpgpnetPort, lobbyPort);
		isGameRunning.set(true);
	}

	public static void leaveGame() {
		Logger.info("Stopping ForgedAlliance.");
		forgedAlliance.stop();
		forgedAlliance = null;
		isGameRunning.set(false);
	}


	private static void informationThread() {
		while(true) {

			if(Logger.collectedLog.length() > 30000) {
				Logger.collectedLog = Logger.collectedLog.substring(0, 30000);
				Logger.error("Log too long. Cannot send to server");
			}

			ClientInformationMessage message;
			synchronized (TestServerAccessor.latencies) {
				IceStatus iceStatus = ICEAdapter.status();
				message = new ClientInformationMessage(username, playerID, System.currentTimeMillis(), TestServerAccessor.latencies, scenarioOptions.isUploadIceStatus() ? iceStatus : new IceStatus(), scenarioOptions.isUploadLog() ? Logger.collectedLog : "", isGameRunning.get() ? forgedAlliance.getPeers() : null);
				TestServerAccessor.latencies = new LinkedList<>();
			}
			Logger.collectedLog = "";


			TestServerAccessor.send(message);

			if(forgedAlliance != null) {
				forgedAlliance.getPeers().forEach(p -> {
					synchronized (p) {
						p.setLatencies(new LinkedList<>());
					}
				});
			}

//			Logger.debug("Sent: %s", new Gson().toJson(message));

			try { Thread.sleep(INFORMATION_INTERVAL); } catch(InterruptedException e) {}
		}
	}




	public static void main(String args[]) {
		if (args.length >= 1)
			if (args[0].equals("debug")) {
				DEBUG_MODE = true;
			} else {
				Logger.enableLogging();
			}

			if(Pattern.compile("\\d*").matcher(args[0]).matches()) {
				ICEAdapter.EXTERNAL_ADAPTER_PORT = Integer.parseInt(args[0]);
			}
		else {
			Logger.enableLogging();
		}
		Logger.init("ICE adapter testclient");

		GUI.init(args);

		GUI.showGDPRDialog();
		GUI.showUsernameDialog();

		TestServerAccessor.init();

		ICEAdapter.init();

		GUI.instance.showStage();

		VoiceChat.init();

		new Thread(TestClient::informationThread).start();
	}

	public static void close() {
		VoiceChat.close();

		TestServerAccessor.disconnect();
		ICEAdapter.close();

		Logger.close();


		System.exit(0);
	}
}
