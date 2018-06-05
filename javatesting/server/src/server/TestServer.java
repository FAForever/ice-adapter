package server;

import common.ICEAdapterTest;
import logging.Logger;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.LinkedList;
import java.util.List;

public class TestServer {

	public static List<Player> players = new LinkedList<>();
	public static List<Game> games = new LinkedList<>();

	public static void main(String args[]) {
		Logger.enableLogging();
		Logger.init("ICE adapter testserver");

		try {
			ServerSocket serverSocket = new ServerSocket(ICEAdapterTest.TEST_SERVER_PORT);

			while (true) {
				Socket socket = serverSocket.accept();

				new Player(socket);
			}
		} catch (IOException e) {
			e.printStackTrace();
		}


	}

}
