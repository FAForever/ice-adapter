package client;

import client.ice.ICEAdapter;
import com.google.gson.Gson;
import common.ICEAdapterTest;
import javafx.beans.property.BooleanProperty;
import javafx.beans.property.SimpleBooleanProperty;
import javafx.scene.control.Alert;
import logging.Logger;
import net.*;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;
import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.CompletableFuture;

import static com.github.nocatch.NoCatch.noCatch;

public class TestServerAccessor {

	public static final String TEST_SERVER_ADDRESS = "localhost";
	private static Gson gson = new Gson();
	public static BooleanProperty connected = new SimpleBooleanProperty(false);

	private static Socket socket;
	private static DataInputStream in;
	private static DataOutputStream out;

	public static Queue<Integer> latencies = new LinkedList<>();

	public static void init() {
		connect();
		new Thread(TestServerAccessor::echoThread).start();
	}


	private static void listener() {
		CompletableFuture f = new CompletableFuture();
		ICEAdapter.connected.addListener(s -> f.complete(s));
		f.join();

		try {
			while(true) {
				String messageClass = in.readUTF();
				Object message = gson.fromJson(in.readUTF(), Class.forName(messageClass));

				if(message instanceof IceMessage) {
					ICEAdapter.iceMsg(((IceMessage)message).getSrcPlayerId(), ((IceMessage)message).getMsg());
				}

				if(message instanceof HostGameMessage) {
					if(TestClient.forgedAlliance != null) {
						TestClient.leaveGame();
					}

					ICEAdapter.hostGame(((HostGameMessage)message).getMapName());
					TestClient.joinGame(ICEAdapter.GPG_PORT, ICEAdapter.LOBBY_PORT);
				}

				if(message instanceof JoinGameMessage) {
					if(TestClient.forgedAlliance != null) {
						TestClient.leaveGame();
					}

					ICEAdapter.joinGame(((JoinGameMessage)message).getRemotePlayerLogin(), ((JoinGameMessage)message).getRemotePlayerId());
					TestClient.joinGame(ICEAdapter.GPG_PORT, ICEAdapter.LOBBY_PORT);
				}

				if(message instanceof ConnectToPeerMessage) {
					ICEAdapter.connectToPeer(((ConnectToPeerMessage)message).getRemotePlayerLogin(), ((ConnectToPeerMessage)message).getRemotePlayerId(), ((ConnectToPeerMessage)message).isOffer());
				}

				if(message instanceof DisconnectFromPeerMessage) {
					ICEAdapter.disconnectFromPeer(((DisconnectFromPeerMessage)message).getRemotePlayerId());
				}

				if(message instanceof LeaveGameMessage) {
					TestClient.leaveGame();
				}


				if(message instanceof EchoResponse) {
					latencies.add((int) (System.currentTimeMillis() - ((EchoResponse) message).getTimestamp()));
					if(latencies.size() > 10) {
						latencies.remove();
					}
				}

			}
		} catch(IOException | ClassNotFoundException e) {
			TestClient.close();
		}
	}

	public static void send(Object message) {
		synchronized (out) {
			try {
				out.writeUTF(message.getClass().getName());
				out.writeUTF(gson.toJson(message));
			} catch(IOException e) {
				Logger.error("Error while sending to server", e);
				System.exit(123);
			}
		}
	}

	private static void echoThread() {
		while(connected.get()) {
			send(new EchoRequest(System.currentTimeMillis()));
			try { Thread.sleep(1000); } catch(InterruptedException e) {}
		}
	}

	private static void connect() {
		Alert alert = noCatch(() -> GUI.showDialog("Connecting...").get());
		Logger.info("Connecting to %s:%d...", TEST_SERVER_ADDRESS, ICEAdapterTest.TEST_SERVER_PORT);

		try {
			socket = new Socket(TEST_SERVER_ADDRESS, ICEAdapterTest.TEST_SERVER_PORT);
			in = new DataInputStream(socket.getInputStream());
			out = new DataOutputStream(socket.getOutputStream());

			out.writeUTF(TestClient.username);
			if(! in.readBoolean()) {
				Logger.error("could not lock in username");
				System.exit(3);
			}

			TestClient.playerID = in.readInt();

			Logger.info("Got username: %s(%d)", TestClient.username, TestClient.playerID);

		} catch (IOException e) {
			Logger.error("Could not connect to test server", e);
			noCatch(() -> GUI.showDialog("Could not connect to test server.").get()).setOnCloseRequest(ev -> {
				System.exit(-1);
			});
			try { Thread.sleep(3000); } catch (InterruptedException e1) {}
			System.exit(-1);
		}


		connected.set(true);
		Logger.info("Connected to %s:%d", TEST_SERVER_ADDRESS, ICEAdapterTest.TEST_SERVER_PORT);

		new Thread(TestServerAccessor::listener).start();

		GUI.runAndWait(alert::close);
	}

	public static void disconnect() {
		Logger.debug("Disconnecting from test server...");
		try {
			socket.close();
		} catch (IOException e) {
			Logger.error(e);
		}
		connected.set(false);
	}
}
