package client;

import client.experimental.HolePunching;
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
import java.net.InetAddress;
import java.net.Socket;
import java.util.ConcurrentModificationException;
import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.CompletableFuture;

import static com.github.nocatch.NoCatch.noCatch;

public class TestServerAccessor {

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

				if(message instanceof IceAdapterSignalMessage) {
					String signal = ((IceAdapterSignalMessage)message).getSignal();
					switch (signal) {
						case "stop": ICEAdapter.sigStop();break;
						case "cont": ICEAdapter.sigCont();break;
						case "kill": ICEAdapter.sigKill();break;
						default: Logger.warning("Unrecognized signal: %s", signal);
					}
				}

				if(message instanceof EchoResponse) {
					latencies.add((int) (System.currentTimeMillis() - ((EchoResponse) message).getTimestamp()));
					if(latencies.size() > 50) {
						latencies.remove();
					}
				}

				if(message instanceof HolePunchingMessage) {
					HolePunchingMessage msg = (HolePunchingMessage) message;
					HolePunching.addPeer(msg.getId(), InetAddress.getByName(msg.getAddress()), msg.getPort());
				}

				if(message instanceof ScenarioOptionsMessage) {
					TestClient.scenarioOptions = (ScenarioOptionsMessage) message;
				}

			}
		} catch(IOException | ClassNotFoundException e) {
			TestClient.close();
		}
	}

	public static void send(Object message) {
		synchronized (out) {
			try {
                String json = null;
                try {
                    json = gson.toJson(message);
                } catch (ConcurrentModificationException e) {
                    Logger.warning("ConcurrentModification exception during serialization of status");
                }
                if (json != null) {
//					System.out.println(json);
                    out.writeUTF(message.getClass().getName());
                    out.writeUTF(json);//TODO: catch more concurrent modification exceptions
                }
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
		alert.setOnCloseRequest(event -> System.exit(0));
		Logger.info("Connecting to %s:%d...", ICEAdapterTest.TEST_SERVER_ADDRESS, ICEAdapterTest.TEST_SERVER_PORT);

		try {
			socket = new Socket(ICEAdapterTest.TEST_SERVER_ADDRESS, ICEAdapterTest.TEST_SERVER_PORT);
			in = new DataInputStream(socket.getInputStream());
			out = new DataOutputStream(socket.getOutputStream());

			if(in.readInt() != ICEAdapterTest.VERSION) {
				Logger.error("Wrong version: %d", ICEAdapterTest.VERSION);
				socket.close();
				GUI.runAndWait(alert::close);
				alert = noCatch(() -> GUI.showDialog("Please download the newest version.").get());
				try { Thread.sleep(3000); } catch(InterruptedException e) {}
				System.exit(59);
			}

			out.writeUTF(TestClient.username);
			if(! in.readBoolean()) {
				Logger.error("could not lock in username");
				System.exit(3);
			}

			TestClient.playerID = in.readInt();
			GUI.instance.scenario.set(in.readUTF());
			HolePunching.init(in.readInt());

			Logger.info("Got username: %s(%d)", TestClient.username, TestClient.playerID);

		} catch (IOException e) {
			Logger.error("Could not connect to test server");
			alert.setOnCloseRequest(null);
			GUI.runAndWait(alert::close);
			alert = noCatch(() -> GUI.showDialog("Could not reach test server.\nPlease wait till the test starts.\nWill retry in 10 seconds.").get());
			alert.setOnCloseRequest(ev -> {
				System.exit(-1);
			});
			try { Thread.sleep(10000); } catch (InterruptedException e1) {}
			alert.setOnCloseRequest(null);
			GUI.runAndWait(alert::close);

			connect();
			return;
		}


		connected.set(true);
		Logger.info("Connected to %s:%d", ICEAdapterTest.TEST_SERVER_ADDRESS, ICEAdapterTest.TEST_SERVER_PORT);

		new Thread(TestServerAccessor::listener).start();

		alert.setOnCloseRequest(null);
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
