package server;

import lombok.Data;
import net.*;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

@Data
public class Game {
	private final Player host;
	private final List<Player> players = new LinkedList<>();

	public Game(Player host) {
		this.host = host;
		players.add(host);

		host.send(new HostGameMessage("Seton's clutch"));
	}

	public synchronized void join(Player player) {
		host.send(new ConnectToPeerMessage(player.getUsername(), player.getId(), true));
		player.send(new JoinGameMessage(host.getUsername(), host.getId()));

		players.stream()
				.filter(p -> p != host)
				.forEach(p -> {
					p.send(new ConnectToPeerMessage(player.getUsername(), player.getId(), true));
					player.send(new ConnectToPeerMessage(p.getUsername(), p.getId(), false));
				});

		players.forEach(p -> {
			HolePunchingServer.connect(p, player);
		});

		players.add(player);
	}

	public synchronized void left(Player player) {
		if(! TestServer.players.contains(player)) {
			players.remove(player);
		}

		if(player == host) {
			close();
		} else {
			for (Player p : players) {
				p.send(new DisconnectFromPeerMessage(player.getId()));
				player.send(new DisconnectFromPeerMessage(p.getId()));
			}
		}

		players.remove(player);
	}

	//e.g. host left
	public synchronized void close() {
		//disconnect all players

		synchronized (TestServer.games) {
			TestServer.games.remove(this);
		}


		Iterator<Player> iter = players.iterator();
		while(iter.hasNext()) {
			Player rem = iter.next();
			iter.remove();

			rem.send(new LeaveGameMessage());

			for (Player p : players) {
				p.send(new DisconnectFromPeerMessage(rem.getId()));
				rem.send(new DisconnectFromPeerMessage(p.getId()));
			}
		}
	}
}
