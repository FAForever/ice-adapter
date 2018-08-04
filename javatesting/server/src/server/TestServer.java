package server;

import com.google.gson.Gson;
import common.ICEAdapterTest;
import logging.Logger;
import net.ClientInformationMessage;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.*;

import static com.github.nocatch.NoCatch.noCatch;

public class TestServer {

	public static List<Player> players = new LinkedList<>();
	public static List<Game> games = new LinkedList<>();

	public static Map<Integer, CollectedInformation> collectedData = new HashMap<>();
	private static volatile boolean running = true;

	public static void main(String args[]) {
		Logger.enableLogging();
		Logger.setLogLevel(Logger.INFO);
		Logger.init("ICE adapter testserver");

		IceTestServerConfig.init();

		new Thread(TestServer::stopListener).start();

		Webserver.init();

		ScenarioRunner.start();

		try {
			ServerSocket serverSocket = new ServerSocket(ICEAdapterTest.TEST_SERVER_PORT);

			while (running) {
				try {
					Socket socket = serverSocket.accept();

					if(players.size() >= IceTestServerConfig.INSTANCE.getMax_users()) {
						socket.close();
					} else if(running) {
						new Player(socket);
					}

				} catch(IOException e) {
					Logger.warning("Error while accepting user.");
				}
			}
		} catch (IOException e) {
			e.printStackTrace();
		}


	}

	private static void stopListener() {
		Scanner scan = new Scanner(System.in);
		scan.next();

		close();
	}

	public static void close() {
		if(! running) {
			return;
		}
		running = false;
		Logger.info("Got end");

//		synchronized (players) {
		new Thread(() -> {
			new HashSet<>(players).forEach(Player::disconnect);
		}).start();
//		}

		writeCollectedData();
		Webserver.close();
		Logger.close();
		System.exit(0);
	}


	public static void writeCollectedData() {
		Logger.info("Writing logs...");
		Path dir = Paths.get(String.valueOf(System.currentTimeMillis()));
		File dirFile = dir.toFile();
		if(! dirFile.exists()) {
			dirFile.mkdir();
		}

		Gson gson = new Gson();

		try {
			{
				FileWriter writer = new FileWriter(dir.resolve("latency.log").toFile());

				writer.write(getLatencyLog(false));

				writer.flush();
				writer.close();
			}

			for(Map.Entry<Integer, CollectedInformation> entry : collectedData.entrySet()) {
				try {
					FileWriter writer = new FileWriter(dir.resolve(entry.getKey() + ".userLog").toFile());
					writer.write("Id: " + entry.getKey() + "\n");
					writer.write("Name: " + entry.getValue().getUsername() + "\n\n");
					writer.write(entry.getValue().getLog());
					writer.flush();
					writer.close();
				} catch(Exception e) {
					System.err.println("Error while writing log.");
				}
			}

			for(Map.Entry<Integer, CollectedInformation> entry : collectedData.entrySet()) {
				try {
					FileWriter writer = new FileWriter(dir.resolve(entry.getKey() + ".iceStatusLog").toFile());
					writer.write("Id: " + entry.getKey() + "\n");
					writer.write("Name: " + entry.getValue().getUsername() + "\n\n");
					writer.write(entry.getValue().getInformationMessages().stream().map(ClientInformationMessage::getIceStatus).map(gson::toJson).reduce("", (l, r) -> l + "\n" + r));
					writer.flush();
					writer.close();
				} catch(Exception e) {
					System.err.println("Error while writing log.");
				}

			}

			for(Map.Entry<Integer, CollectedInformation> entry1 : collectedData.entrySet()) {
				for(Map.Entry<Integer, CollectedInformation> entry2 : collectedData.entrySet()) {
					if(entry1.getKey() == entry2.getKey() || entry1.getKey() > entry2.getKey()) {
						continue;
					}

					try {
						FileWriter writer = new FileWriter(dir.resolve(String.format("%d-%d.iceMsgLog", entry1.getKey(), entry2.getKey())).toFile());
						writer.write(String.format("Ids: %d <-> %d\n", entry1.getKey(), entry2.getKey()));
						writer.write(String.format("Names: %d <-> %d\n\n", entry1.getValue().getId(), entry2.getValue().getId()));

						Arrays.asList(entry1, entry2).stream()
								.flatMap(e -> e.getValue().getIceMessages().entrySet().stream())
								.filter(e -> e.getKey() == entry1.getKey() || e.getKey() == entry2.getKey())
								.flatMap(e -> e.getValue().entrySet().stream())
								.sorted(Comparator.comparingLong(Map.Entry::getKey))
								.map(Map.Entry::getValue)
								.forEach(im -> noCatch(() -> writer.write(String.format("%d -> %d:\t%s\n", im.getSrcPlayerId(), im.getDestPlayerId(), gson.toJson(im.getMsg())))));

						writer.flush();
						writer.close();
					} catch(Exception e) {
						System.err.println("Error while writing log.");
					}
				}
			}

			Logger.info("Wrote logs to " + dir.toAbsolutePath().toString());
		} catch(IOException e) {
			Logger.error("Could not write data logs.", e);
		}
	}


	public static String getLatencyLog(boolean filterNonConnected) {
		StringBuilder sb = new StringBuilder();
		for (Map.Entry<Integer, CollectedInformation> entry1 : collectedData.entrySet()) {
			for (Map.Entry<Integer, CollectedInformation> entry2 : collectedData.entrySet()) {
				try {
					if(entry1 == entry2) {
						continue;
					}

//					if(filterNonConnected && (players.stream().noneMatch(p -> p.getId() == entry1.getKey()) || players.stream().noneMatch(p -> p.getId() == entry2.getKey()))) {
//						continue;
//					}

					sb.append(String.format("%d -> %d: ", entry1.getKey(), entry2.getKey()));

					double[] pings = entry1.getValue().getInformationMessages().stream()
							.filter(im -> im.getForgedAlliancePeers() != null)
							.flatMap(im -> im.getForgedAlliancePeers().stream())
							.filter(p -> p.getRemoteId() == entry2.getKey())
							.flatMap(p -> p.getLatencies().stream())
							.mapToDouble(Integer::doubleValue).toArray();

					double min = Arrays.stream(pings).min().orElse(99999);
					double max = Arrays.stream(pings).max().orElse(99999);
					double avg = Arrays.stream(pings).average().orElse(99999);
					double avgLimited = Arrays.stream(pings).filter(p -> p < 2000).average().orElse(99999);

					double serverPing = collectedData.get(entry1.getKey()).getInformationMessages().stream().flatMap(im -> im.getLatencies().stream()).mapToDouble(Integer::doubleValue).average().orElse(99999)
							+ collectedData.get(entry2.getKey()).getInformationMessages().stream().flatMap(im -> im.getLatencies().stream()).mapToDouble(Integer::doubleValue).average().orElse(99999);

					sb.append(String.format("%.0f/%.0f(%.0f)/%.0f", min, avg, avgLimited, max));
					sb.append(String.format("\t(Server: %.0f)", serverPing));
//					writer.write("\tConnected: " + );
					sb.append("\tQuiet for: " + entry1.getValue().getInformationMessages().stream()
							.filter(im -> im.getForgedAlliancePeers().stream().anyMatch(p -> p.getRemoteId() == entry2.getKey() && im.getCurrentTimeMillis() - p.getLastPacketReceived() > 5000))
							.count()
					);
					sb.append("\n");
				} catch(Exception e) {
					Logger.error("Error while writing log.", e);
				}
				if (entry1.getKey() == entry2.getKey()) {
					continue;
				}
			}
		}

		return sb.toString();
	}

}
