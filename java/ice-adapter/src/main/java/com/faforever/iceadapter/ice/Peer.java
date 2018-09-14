package com.faforever.iceadapter.ice;

import com.faforever.iceadapter.IceAdapter;
import lombok.Getter;
import lombok.extern.slf4j.Slf4j;

import java.io.IOException;
import java.net.*;

@Getter
@Slf4j
public class Peer {

    private final GameSession gameSession;

    private final int remoteId;
    private final String remoteLogin;
    private final boolean localOffer;

    private PeerIceModule ice = new PeerIceModule(this);
    private DatagramSocket faSocket;

    public Peer(GameSession gameSession, int remoteId, String remoteLogin, boolean localOffer) {
        this.gameSession = gameSession;
        this.remoteId = remoteId;
        this.remoteLogin = remoteLogin;
        this.localOffer = localOffer;

        log.debug("Peer created: {}, {}, localOffer: {}", remoteId, remoteLogin, String.valueOf(localOffer));

        initForwarding();

        if (localOffer) {
            new Thread(ice::initiateIce).start();
        }
    }

    private void initForwarding() {
        try {
            faSocket = new DatagramSocket(0);
        } catch (SocketException e) {
            log.error("Could not create socket for peer: {}, {}", remoteId, remoteLogin, e);
        }

        new Thread(this::faListener).start();

        log.debug("Now forwarding data to peer {}, {}", remoteId, remoteLogin);
    }

    synchronized void onIceDataReceived(byte data[], int offset, int length) {
        try {
            DatagramPacket packet = new DatagramPacket(data, offset, length, InetAddress.getByName("127.0.0.1"), IceAdapter.LOBBY_PORT);
            faSocket.send(packet);
        } catch (UnknownHostException e) {
        } catch (IOException e) {
            log.error("Error while writing to local FA as peer (probably disconnecting from peer) " + remoteId, e);
            return;
        }
    }

    private void faListener() {
        byte data[] = new byte[65536];//64KiB = UDP MTU, in practice due to ethernet frames being <= 1500 B, this is often not used
        while (IceAdapter.running && IceAdapter.gameSession == gameSession) {
            try {
                DatagramPacket packet = new DatagramPacket(data, data.length);
                faSocket.receive(packet);
                ice.onFaDataReceived(data, packet.getLength());
            } catch (IOException e) {
                log.debug("Error while reading from local FA as peer (probably disconnecting from peer) " + remoteId, e);
                return;
            }
        }
        log.debug("No longer listening for messages from FA");
    }

    public volatile boolean closing = false;
    public void close() {
        closing = true;
        faSocket.close();

        ice.close();
    }
}
