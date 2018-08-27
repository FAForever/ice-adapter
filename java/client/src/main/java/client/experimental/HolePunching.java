package client.experimental;

import client.TestClient;
import logging.Logger;

import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.SocketException;
import java.util.AbstractMap;
import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class HolePunching {
    private static int localPort;
    private static DatagramSocket datagramSocket;

    private static HashMap<Integer, Map.Entry<InetAddress, Integer>> peers = new HashMap();
    public static HashMap<Integer, Long> latencies = new HashMap();

    public static void init(int port) {
        localPort = port;
        try {
            datagramSocket = new DatagramSocket(port);
        } catch(SocketException e) {
            Logger.error("Could not create udp socket for hole punching test.", e);
        }

        new Thread(HolePunching::listener).start();
        new Thread(HolePunching::holePunching).start();
    }

    private static final Pattern messagePattern = Pattern.compile("(req|res)(\\d*):(\\d*)");
    private static void listener() {

        byte[] buffer = new byte[1024];
        DatagramPacket packet;
        while(true) {
            try {
                packet = new DatagramPacket(buffer, 0, buffer.length);
                datagramSocket.receive(packet);
                String message = new String(buffer, 0, packet.getLength());
                Matcher matcher = messagePattern.matcher(message);
                if(! matcher.find()) {
                    continue;
                }
                int fromId = Integer.parseInt(matcher.group(2));
                long startTime = Long.parseLong(matcher.group(3));
//                System.out.println(message);
                if(matcher.group(1).equals("res")) {
                    latencies.put(fromId, System.currentTimeMillis() - startTime);
                } else if(peers.containsKey(fromId)) {
                    byte[] responseBuffer = ("res" + String.valueOf(TestClient.playerID) + ":" + startTime).getBytes();
                    DatagramPacket responsePacket = new DatagramPacket(responseBuffer, 0, responseBuffer.length);
                    responsePacket.setAddress(peers.get(fromId).getKey());
                    responsePacket.setPort(peers.get(fromId).getValue());
                    try {
                        datagramSocket.send(responsePacket);
                    } catch (IOException e2) {
                        e2.printStackTrace();
                    }
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    private static void holePunching() {
        while(true) {
            synchronized (peers) {
//                latencies.clear();

                byte[] buffer = ("req" + String.valueOf(TestClient.playerID) + ":" + System.currentTimeMillis()).getBytes();

                peers.entrySet().stream()
                        .map(e -> e.getValue())
                        .forEach(e -> {
                            DatagramPacket packet = new DatagramPacket(buffer, 0, buffer.length);
                            packet.setAddress(e.getKey());
                            packet.setPort(e.getValue());
                            try {
                                datagramSocket.send(packet);
                            } catch (IOException e2) {
                                e2.printStackTrace();
                            }
                        });
            }
            try { Thread.sleep(300); } catch(InterruptedException e) {}
        }
    }

    public static void addPeer(int id, InetAddress address, int port) {
        synchronized (peers) {
            peers.put(id, new AbstractMap.SimpleImmutableEntry<>(address, port));
        }
    }
}
