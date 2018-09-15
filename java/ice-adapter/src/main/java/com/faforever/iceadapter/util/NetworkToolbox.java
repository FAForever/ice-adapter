package com.faforever.iceadapter.util;

import lombok.extern.slf4j.Slf4j;

import java.io.IOException;
import java.net.DatagramSocket;
import java.net.ServerSocket;
import java.net.SocketException;
import java.util.Random;

@Slf4j
public class NetworkToolbox {

    private static Random random = new Random();

    /**
     * Returns a free TCP port by generating random ports within the specified range and opening/closing a socket on them
     * @param min Minimum port, inclusive
     * @param max Maximum port, exclusive
     * @return the discovered port, -1 if no port could be found
     */
    public static int findFreeTCPPort(int min, int max) {

        for(int i = 0;i < 10000;i++) {
            int possiblePort = min + random.nextInt(max - min);
            try {
                ServerSocket serverSocket = new ServerSocket(possiblePort);
                serverSocket.close();
                return possiblePort;
            } catch(IOException e) {
                continue;
            }
        }

        log.error("Could not find a free tcp port");
        System.exit(-1);
        return -1;
    }

    /**
     * Returns a free TCP port by generating random ports within the specified range and opening/closing a socket on them
     * @param min Minimum port, inclusive
     * @param max Maximum port, exclusive
     * @return the discovered port, -1 if no port could be found
     */
    public static int findFreeUDPPort(int min, int max) {

        for(int i = 0;i < 10000;i++) {
            int possiblePort = min + random.nextInt(max - min);
            try {
                DatagramSocket socket = new DatagramSocket(possiblePort);
                socket.close();
                return possiblePort;
            } catch(SocketException e) {
                continue;
            }
        }

        log.error("Could not find a free tcp port");
        System.exit(-1);
        return -1;
    }
}
