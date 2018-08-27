package server;

import net.HolePunchingMessage;

import java.util.HashMap;
import java.util.Map;

public class HolePunchingServer {

    public static volatile int port = 51239;
    public static Map<Integer, Integer> ports = new HashMap<>();

    public static synchronized int onPlayerConnected(Player player) {
        int newPort = port++;
        ports.put(player.getId(), newPort);
        return newPort;
    }

    public static synchronized void connect(Player player1, Player player2) {
        player1.send(new HolePunchingMessage(player2.getId(), player2.getSocket().getInetAddress().getHostAddress(), ports.get(player2.getId())));
        player2.send(new HolePunchingMessage(player1.getId(), player1.getSocket().getInetAddress().getHostAddress(), ports.get(player1.getId())));
    }
}
