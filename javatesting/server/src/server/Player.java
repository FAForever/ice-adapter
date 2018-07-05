package server;

import com.google.gson.Gson;
import com.google.gson.JsonSyntaxException;
import common.ICEAdapterTest;
import logging.Logger;
import lombok.Getter;
import net.*;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.Socket;

import static server.TestServer.games;
import static server.TestServer.players;

@Getter
public class Player {
	private static volatile int PLAYER_ID_FACTORY = 0;

	private Gson gson = new Gson();

	private boolean connected = false;
	private int id;
	private String username;
	private Socket socket;
	private DataInputStream in;
	private DataOutputStream out;

	public Player(Socket socket) {
		this.socket = socket;

		login();
		if(connected == true) {
			new Thread(this::listener).start();
		}

		ScenarioRunner.scenario.onPlayerConnect(this);
	}

	private void login() {
		try {
			in = new DataInputStream(socket.getInputStream());
			out = new DataOutputStream(socket.getOutputStream());

			out.writeInt(ICEAdapterTest.VERSION);

			this.username = in.readUTF();

			if(username.toLowerCase().contains("hitler")) {//@moonbearonmeth: nope
				return;
			}

			if(players.stream().map(Player::getUsername).anyMatch(this.username::equals)) {
				out.writeBoolean(false);
				out.flush();
				disconnect();
				return;
			}

			out.writeBoolean(true);

			this.id = PLAYER_ID_FACTORY++;
			out.writeInt(id);
			out.writeUTF(ScenarioRunner.scenario.getDescription());

			synchronized (players) {
				players.add(this);
			}

			TestServer.collectedData.put(this.id, new CollectedInformation(id, username));
			connected = true;

		} catch(IOException e) {
			Logger.error("Error while logging in player. ", e);

			synchronized (players) {
				if(players.contains(this)) {
					players.remove(this);
					TestServer.collectedData.remove(this.id);
				}
			}

			disconnect();
			return;
		}

		Logger.info("Player connected: %d, %s, %s", id, username, socket.getInetAddress().getHostAddress());
	}

	private void listener() {
		try {
			while(connected) {
				String messageClass = in.readUTF();
				Object message = gson.fromJson(in.readUTF(), Class.forName(messageClass));

				if(message instanceof IceMessage) {
					Logger.debug("IceMessage: %s", gson.toJson(message));

					if(filterIceMessage((IceMessage)message)) {
						Logger.info("Filtered IceMessage");
						continue;
					}

					TestServer.collectedData.get(this.id).getIceMessages().put(System.currentTimeMillis(), (IceMessage) message);

					synchronized (players) {
						players.stream()
								.filter(p -> p.getId() == ((IceMessage)message).getDestPlayerId())
								.findFirst().ifPresent(p -> p.send(message));
					}
				}

				if(message instanceof ClientInformationMessage) {
					Logger.debug("Client information message: %s", gson.toJson(message));
					TestServer.collectedData.get(this.id).getInformationMessages().add((ClientInformationMessage) message);
				}

				if(message instanceof EchoRequest) {
					send(new EchoResponse(((EchoRequest)message).getTimestamp()));
				}
			}
		} catch(IOException | ClassNotFoundException | JsonSyntaxException e) {
			Logger.warning("Disconnecting client due to error while reading data from socket. %s", e.getClass().getName());
			disconnect();
		}
	}

	public boolean filterIceMessage(IceMessage iceMessage) {
		if(! IceTestServerConfig.INSTANCE.isHost() &&
				((iceMessage.getMsg().toString()).contains("typ host"))) {
			return true;
		}

		if(! IceTestServerConfig.INSTANCE.isStun() &&
				(iceMessage.getMsg().toString().contains("typ srflx") || iceMessage.getMsg().toString().contains("typ prflx"))) {
			return true;
		}

		if(! IceTestServerConfig.INSTANCE.isTurn() &&
				(iceMessage.getMsg().toString().contains("typ relay"))) {
			return true;
		}

		return false;
	}

	public void send(Object message) {
		synchronized (out) {
			try {
				out.writeUTF(message.getClass().getName());
				out.writeUTF(gson.toJson(message));
			} catch(IOException e) {
				Logger.error("Error while sending to client", e);
			}
		}
	}

	public void disconnect() {
		synchronized (TestServer.players) {
			TestServer.players.remove(this);
		}

		synchronized (TestServer.games) {
			for (Game g : games) {
				synchronized (players) {
					if(g.getHost() == this) {//TODO
						TestServer.close();
					}

					if (g.getPlayers().contains(this)) {
						g.left(this);
					}
				}
			}
		}

		try {
			in.close();
			out.close();
			socket.close();
		} catch(IOException e) {Logger.error("Error during disconnect.", e);}

		Logger.info("Disconnected player: %d, %s", id, username);
	}

	public void sigStop() {
		this.send(new IceAdapterSignalMessage("stop"));
	}

	public void sigCont() {
		this.send(new IceAdapterSignalMessage("cont"));
	}

	public void sigKill() {
		this.send(new IceAdapterSignalMessage("kill"));
	}
}
