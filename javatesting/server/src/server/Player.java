package server;

import com.google.gson.Gson;
import logging.Logger;
import lombok.Getter;
import net.ClientInformationMessage;
import net.EchoRequest;
import net.EchoResponse;
import net.IceMessage;

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

		//Todo:
		if(TestServer.games.isEmpty()) {
			TestServer.games.add(new Game(this));
		} else {
			TestServer.games.get(0).join(this);
		}
	}

	private void login() {
		try {
			in = new DataInputStream(socket.getInputStream());
			out = new DataOutputStream(socket.getOutputStream());

			this.username = in.readUTF();

			if(players.stream().map(Player::getUsername).anyMatch(this.username::equals)) {
				out.writeBoolean(false);
				out.flush();
				disconnect();
				return;
			}

			out.writeBoolean(true);

			synchronized (players) {
				players.add(this);
			}

			this.id = PLAYER_ID_FACTORY++;
			out.writeInt(id);

			connected = true;

		} catch(IOException e) {
			Logger.error("Error while logging in player. ", e);
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
					synchronized (players) {
						players.stream()
								.filter(p -> p.getId() == ((IceMessage)message).getDestPlayerId())
								.findFirst().ifPresent(p -> p.send(message));
					}
				}

				if(message instanceof ClientInformationMessage) {
					Logger.debug("Client information message: %s", gson.toJson(message));
				}

				if(message instanceof EchoRequest) {
					send(new EchoResponse(((EchoRequest)message).getTimestamp()));
				}
			}
		} catch(IOException | ClassNotFoundException e) {
			disconnect();
		}
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
}
