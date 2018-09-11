package client.forgedalliance;

import client.GUI;
import client.TestClient;
import common.ICEAdapterTest;
import data.ForgedAlliancePeer;
import javafx.geometry.Insets;
import javafx.scene.layout.Background;
import javafx.scene.layout.BackgroundFill;
import javafx.scene.layout.CornerRadii;
import javafx.scene.paint.Color;
import logging.Logger;
import lombok.Getter;

import java.io.*;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.concurrent.atomic.AtomicLong;

import static util.Util.assertThat;

@Getter
public class ForgedAlliance {

	private static final int ECHO_INTERVAL = 100;
	private static final int CONNECTION_REQ_INTERVAL = 1000;

	private static final String CONNECTION_ACK = "conAck";
	private static final String CONNECTION_REQ = "conReq";
	private static final String ECHO_REQ = "echoReq";
	private static final String ECHO_RES = "echoRes";

	private Random random = new Random();

	private boolean running = true;

	private int gpgnetPort;//gpgnet to connect to
	private int lobbyPort;//incoming messages

	private DatagramSocket lobbySocket;

	private Socket gpgnetSocket;
	private FaDataInputStream gpgnetIn;
	private FaDataOutputStream gpgnetOut;

	public List<ForgedAlliancePeer> peers = new ArrayList<>();

	public ForgedAlliance(int gpgnetPort, int lobbyPort) {
		this.gpgnetPort = gpgnetPort;
		this.lobbyPort = lobbyPort;

		try {
			gpgnetSocket = new Socket("localhost", gpgnetPort);
			lobbySocket = new DatagramSocket(lobbyPort);

			gpgnetIn = new FaDataInputStream(gpgnetSocket.getInputStream());
			gpgnetOut = new FaDataOutputStream(gpgnetSocket.getOutputStream());

			new Thread(this::gpgpnetListener).start();
			new Thread(this::lobbyListener).start();

			new Thread(this::echoThread).start();

			synchronized (gpgnetOut) {
				gpgnetOut.writeMessage("GameState", "Idle");
			}

		} catch(IOException e) {
            Logger.error("Could not start lobby server. (GPGNET or lobbySocket failed)", e);
		}
	}

	private void echoThread() {
		while(running) {

			if(System.currentTimeMillis() - lastMetricsUpdate >= 1000) {
				updateMetrics();
			}

			synchronized (peers) {
				peers.stream()
						.filter(ForgedAlliancePeer::isConnected)
						.forEach(peer -> {
							try {
								ByteArrayOutputStream data = new ByteArrayOutputStream();
								DataOutputStream packetOut = new DataOutputStream(data);

								packetOut.writeUTF(ECHO_REQ);
								packetOut.writeInt(TestClient.playerID);//src
								packetOut.writeInt(peer.remoteId);//target
								packetOut.writeInt(peer.echoRequestsSent++);
								packetOut.writeLong(System.currentTimeMillis());
								int randomBytes;
								if(ICEAdapterTest.TEST_DATA_RANDOM_SIZE) {
									randomBytes = 25 + random.nextInt(50);
								} else {
									randomBytes = 60;//FA sends 2 packets per tick, one ~15 bytes, one 30-70 bytes
								}
								byte[] randomData = new byte[randomBytes];
								random.nextBytes(randomData);
								packetOut.writeInt(randomBytes);
								packetOut.write(randomData, 0, randomData.length);

								sendLobby(peer.remoteAddress, peer.remotePort, data);
							} catch(IOException e) {
								Logger.warning("Error while sending to peer: %d", peer.remoteId);
							}
						});

				peers.stream()
						.filter(p -> !p.isConnected())
						.forEach(peer -> {
							try {
								if (peer.offerer == ForgedAlliancePeer.Offerer.LOCAL) {
									if ((System.currentTimeMillis() - peer.lastConnectionRequestSent) >= CONNECTION_REQ_INTERVAL) {
										peer.lastConnectionRequestSent = System.currentTimeMillis();

										ByteArrayOutputStream data = new ByteArrayOutputStream();
										DataOutputStream packetOut = new DataOutputStream(data);

										packetOut.writeUTF(CONNECTION_REQ);
										packetOut.writeInt(TestClient.playerID);
										packetOut.writeUTF(TestClient.username);

										sendLobby(peer.remoteAddress, peer.remotePort, data);

										Logger.debug("<FA> Sent CONNECTION_REQ to %s(%d) at %s:%d", peer.remoteUsername, peer.remoteId, peer.remoteAddress, peer.remotePort);
									}
								} else {
//									Logger.error("<FA> Awaiting CONNECTION_REQ");
										//TODO: log this case?
								}
							} catch(IOException e) {
								Logger.warning("Error while sending to peer: %d", peer.remoteId);
							}
						});

				//Data
				peers.stream()
						.filter(p -> System.currentTimeMillis() - p.lastPacketReceived > 10000)
						.forEach(p -> p.addLatency((int)(System.currentTimeMillis() - p.lastPacketReceived)));
			}

			try { Thread.sleep(ECHO_INTERVAL); } catch(InterruptedException e) {}
		}
	}

	private long lastMetricsUpdate = System.currentTimeMillis();
	public float bytesPerSecondIn = 0;
	public float bytesPerSecondOut = 0;
	private void updateMetrics() {
		int d = (int)(System.currentTimeMillis() - lastMetricsUpdate);
		lastMetricsUpdate += d;

		bytesPerSecondIn = (float)bytesReceived.getAndSet(0) / ((float)d / 1000f);
		bytesPerSecondOut = (float)bytesSent.getAndSet(0) / ((float)d / 1000f);
	}

	private volatile AtomicLong bytesReceived = new AtomicLong(0);
	private void lobbyListener() {
		try {
			byte[] buffer = new byte[4096];
			while(running) {
				DatagramPacket packet = new DatagramPacket(buffer, buffer.length);
				lobbySocket.receive(packet);

				bytesReceived.addAndGet(packet.getLength());

//				Logger.debug("<FA> Received: %s", new String(packet.getData()));

				DataInputStream packetIn = new DataInputStream(new ByteArrayInputStream(packet.getData()));
				String command = packetIn.readUTF();


				if(command.equals(CONNECTION_REQ)) {
					int remoteId = packetIn.readInt();
					String remoteUsername = packetIn.readUTF();

					ForgedAlliancePeer peer;
					synchronized (peers) {
						if(peers.stream().noneMatch(p -> p.remoteId == remoteId)) {
							peer = new ForgedAlliancePeer(packet.getAddress().getHostAddress(), packet.getPort(), remoteId, remoteUsername, ForgedAlliancePeer.Offerer.REMOTE);
							peers.add(peer);
						} else {
							peer = peers.stream().filter(p -> p.remoteId == remoteId).findAny().get();
						}
						peer.setConnected(true);
					}

					ByteArrayOutputStream data = new ByteArrayOutputStream();
					DataOutputStream packetOut = new DataOutputStream(data);

					packetOut.writeUTF(CONNECTION_ACK);
					packetOut.writeInt(TestClient.playerID);
					packetOut.writeUTF(TestClient.username);

					sendLobby(peer.remoteAddress, peer.remotePort, data);

					Logger.debug("<FA> Sent CONNECTION_ACK to %s(%d) at %s:%d", peer.remoteUsername, peer.remoteId, peer.remoteAddress, peer.remotePort);
				}

				if(command.equals(CONNECTION_ACK)) {
					int remoteId = packetIn.readInt();
					String remoteUsername = packetIn.readUTF();

					synchronized (peers) {
						peers.stream().filter(p -> p.remoteId == remoteId).findAny().ifPresent(p -> p.setConnected(true));
					}

					Logger.debug("<FA> Got CONNECTION_ACK from %s(%d) at %s:%d", remoteUsername, remoteId, packet.getAddress().getHostAddress(), packet.getPort());
				}

				if(command.equals(ECHO_REQ)) {
					int remoteId = packetIn.readInt();
					int localId = packetIn.readInt();
					int echoReqId = packetIn.readInt();
					long echoReqTime = packetIn.readLong();
					int randomBytes = packetIn.readInt();
					byte[] randomData = new byte[randomBytes];
					packetIn.read(randomData, 0, randomBytes);

					assertThat(localId == TestClient.playerID);



					//Construct response
					ByteArrayOutputStream data = new ByteArrayOutputStream();
					DataOutputStream packetOut = new DataOutputStream(data);

					packetOut.writeUTF(ECHO_RES);
					packetOut.writeInt(TestClient.playerID);//src
					packetOut.writeInt(remoteId);//target
					packetOut.writeInt(echoReqId);
					packetOut.writeLong(echoReqTime);
					packetOut.writeInt(randomBytes);
					packetOut.write(randomData, 0, randomData.length);

					sendLobby(packet.getAddress().getHostAddress(), packet.getPort(), data);//TODO return to peer address instead of src address?
				}

				if(command.equals(ECHO_RES)) {

					int remoteId = packetIn.readInt();
					int localId = packetIn.readInt();
					int echoReqId = packetIn.readInt();
					long echoReqTime = packetIn.readLong();
					int randomBytes = packetIn.readInt();
					byte[] randomData = new byte[randomBytes];
					packetIn.read(randomData, 0, randomBytes);

					assertThat(localId == TestClient.playerID);
					int latency = (int) (System.currentTimeMillis() - echoReqTime);


					if(echoReqId > 5 && latency < 2000 /*TODO HOW?*/) {//block first connecting pings
						synchronized (peers) {
							peers.stream().filter(p -> p.remoteId == remoteId).findAny().ifPresent(p -> p.addLatency(latency));
						}
					}

//					Logger.debug("<FA> Recevied ECHO_RES %d after %d ms from %d", echoReqId, latency, remoteId);
				}
			}
		} catch(IOException e) {
			if(this.running) {
				Logger.error("Error while listening for lobby messages.", e);
			}
		}
	}

	private void gpgpnetListener() {
		try {
			while(running) {

				String command = gpgnetIn.readString();
				List<Object> args = gpgnetIn.readChunks();

				if(command.equals("CreateLobby")) {
//					assertThat(! args.get(0).toString().isEmpty());
					assertThat(args.get(1).equals(lobbyPort));
					assertThat(args.get(2).equals(TestClient.username));
					assertThat(args.get(3).equals(TestClient.playerID));

					synchronized (gpgnetOut) {
						gpgnetOut.writeMessage("GameState", "Lobby");
					}

					Logger.info("<GPG> Creating lobby");
				}

				if(command.equals("HostGame")) {
					assertThat(! args.get(0).toString().isEmpty());
					Logger.info("<GPG> Hosting game on %s", args.get(0));
				}

				if(command.equals("ConnectToPeer")) {
					ForgedAlliancePeer peer = new ForgedAlliancePeer(((String)args.get(0)).split(":")[0], Integer.parseInt(((String)args.get(0)).split(":")[1]), (Integer) args.get(2), (String)args.get(1), ForgedAlliancePeer.Offerer.LOCAL);

					synchronized (peers) {
						if(peers.stream().noneMatch(p -> p.remoteId == peer.remoteId)) {
							peers.add(peer);
						}
					}
				}

				if(command.equals("DisconnectFromPeer")) {
					synchronized (peers) {
						peers.stream().filter(p -> p.remoteId == (Integer) args.get(0)).findAny()
								.ifPresent(p -> {
									p.setConnected(false);
									peers.remove(p);
								});
					}
				}

				Logger.debug("<GPG> Received %s %s", command, args.stream().reduce("", (l, r) -> l + " " + r));
			}
		} catch(IOException e) {
			if(this.running) {
				Logger.error("Error while listening for gpg messages.", e);
                GUI.runAndWait(() -> GUI.instance.getRoot().setBackground(new Background(new BackgroundFill(new Color(189.0 / 255.0, 61.0 / 255.0, 58.0 / 255.0, 1.0), CornerRadii.EMPTY, Insets.EMPTY))));
			}
		}
	}


	private volatile AtomicLong bytesSent = new AtomicLong(0);
	private void sendLobby(String remoteAddress, int remotePort, byte[] data) {
		try {
			DatagramPacket datagramPacket = new DatagramPacket(data, data.length);
			datagramPacket.setAddress(InetAddress.getByName(remoteAddress));
			datagramPacket.setPort(remotePort);
			lobbySocket.send(datagramPacket);

			bytesSent.addAndGet(data.length);
		} catch (IOException e) {
			Logger.warning("Error while sending UDP Packet.", e);
		}
	}

	private void sendLobby(String remoteAddress, int remotePort, ByteArrayOutputStream dataStream) {
		sendLobby(remoteAddress, remotePort, dataStream.toByteArray());
	}

	public void stop() {
		running = false;
		lobbySocket.close();
		try {
			gpgnetSocket.close();
		} catch (IOException e) {}
	}
}
