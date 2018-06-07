package client;

import client.forgedalliance.ForgedAlliance;
import client.ice.ICEAdapter;
import javafx.beans.property.BooleanProperty;
import javafx.beans.property.SimpleBooleanProperty;
import logging.Logger;
import net.ClientInformationMessage;

import java.util.LinkedList;

public class TestClient {

	public static boolean DEBUG_MODE = false;
	private static final int INFORMATION_INTERVAL = 2500;

	public static String username;
	public static int playerID;

	public static ForgedAlliance forgedAlliance;
	public static BooleanProperty isGameRunning = new SimpleBooleanProperty(false);

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

			ClientInformationMessage message = new ClientInformationMessage(username, playerID, System.currentTimeMillis(), TestServerAccessor.latencies, ICEAdapter.status(), Logger.collectedLog, isGameRunning.get() ? forgedAlliance.getPeers() : null);
			TestServerAccessor.latencies = new LinkedList<>();
			Logger.collectedLog = "";

			TestServerAccessor.send(message);

			if(forgedAlliance != null) {
				forgedAlliance.getPeers().forEach(p -> p.setLatencies(new LinkedList<>()));
			}

//			Logger.debug("Sent: %s", new Gson().toJson(message));

			try { Thread.sleep(INFORMATION_INTERVAL); } catch(InterruptedException e) {}
		}
	}




	public static void main(String args[]) {
		if(args.length >= 1 && args[0].equals("debug")) {
			DEBUG_MODE = true;
		} else {
			Logger.enableLogging();
		}
		Logger.init("ICE adapter testclient");

		GUI.init(args);

		GUI.showGDPRDialog();
		GUI.showUsernameDialog();

		TestServerAccessor.init();

		ICEAdapter.init();

		GUI.instance.showStage();

		new Thread(TestClient::informationThread).start();
	}

	public static void close() {

		TestServerAccessor.disconnect();
		ICEAdapter.close();

		Logger.close();


		System.exit(0);
	}
}
