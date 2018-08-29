package com.faforever.iceadapter.ice;

import com.faforever.iceadapter.IceAdapter;
import com.faforever.iceadapter.logging.Logger;
import com.faforever.iceadapter.rpc.RPCService;
import com.faforever.iceadapter.util.CandidateUtil;
import com.faforever.iceadapter.util.Executor;
import lombok.Getter;
import org.ice4j.Transport;
import org.ice4j.ice.Agent;
import org.ice4j.ice.Component;
import org.ice4j.ice.IceMediaStream;
import org.ice4j.ice.IceProcessingState;
import org.ice4j.ice.harvest.StunCandidateHarvester;
import org.ice4j.ice.harvest.TurnCandidateHarvester;
import org.ice4j.security.LongTermCredential;

import java.io.IOException;
import java.net.DatagramPacket;

import static com.faforever.iceadapter.ice.IceState.*;

@Getter
public class PeerIceModule {
    private static final int PREFERRED_PORT = 6112;//PORT (range) to be used by ICE for communicating

    private Peer peer;

    private Agent agent;
    private IceMediaStream mediaStream;
    private Component component;

    private volatile IceState iceState = NEW;
    private volatile boolean connected = false;
    private volatile Thread listenerThread;

    private final PeerConnectivityCheckerModule connectivityChecker = new PeerConnectivityCheckerModule(this);

    public PeerIceModule(Peer peer) {
        this.peer = peer;
    }

    private void setState(IceState newState) {
        this.iceState = newState;
        RPCService.onIceConnectionStateChanged(IceAdapter.id, peer.getRemoteId(), iceState.getMessage());
    }

    synchronized void initiateIce() {
        if (iceState != NEW && iceState != DISCONNECTED) {
            Logger.warning("ICE already in progress, aborting re initiation. current state: %s", iceState.getMessage());
            return;
        }

        setState(GATHERING);
        Logger.info("Initiating ICE for peer %d", peer.getRemoteId());

        createAgent();
        gatherCandidates();
    }

    private void createAgent() {
        agent = new Agent();
        agent.setControlling(peer.isLocalOffer());


        mediaStream = agent.createMediaStream("faData");
    }

    private void gatherCandidates() {
        Logger.info("Gathering ice candidates");
        GameSession.getStunAddresses().stream().map(StunCandidateHarvester::new).forEach(agent::addCandidateHarvester);
        GameSession.getTurnAddresses().stream().map(a -> new TurnCandidateHarvester(a, new LongTermCredential(GameSession.getTurnUsername(), GameSession.getTurnCredential()))).forEach(agent::addCandidateHarvester);

        try {
            component = agent.createComponent(mediaStream, Transport.UDP, PREFERRED_PORT + (int) (Math.random() * 999.0), PREFERRED_PORT, PREFERRED_PORT + 1000);
        } catch (IOException e) {
            Logger.error("Error while creating stream component", e);
            new Thread(this::reinitIce).start();
            return;
        }

        CandidatesMessage localCandidatesMessage = CandidateUtil.packCandidates(IceAdapter.id, peer.getRemoteId(), agent, component);
        Logger.debug("Sending own candidates to %d", peer.getRemoteId());
        setState(AWAITING_CANDIDATES);
        RPCService.onIceMsg(localCandidatesMessage);

        //TODO: is this a good fix for awaiting candidates loop????
        final int currentacei = ++awaitingCandidatesEventId;
        Executor.executeDelayed(6000, () -> {
            if (iceState == AWAITING_CANDIDATES && currentacei == awaitingCandidatesEventId) {
                onConnectionLost();
            }
        });
    }

    private volatile int awaitingCandidatesEventId = 0;

    //TODO: stuck on awaiting forever? Can candidates even get lost? What if other side discards them?

    public synchronized void onIceMessgageReceived(CandidatesMessage remoteCandidatesMessage) {
        new Thread(() -> {
            Logger.debug("Got IceMsg for peer %d", peer.getRemoteId());

            if (peer.isLocalOffer()) {
                if (iceState != AWAITING_CANDIDATES) {
                    Logger.warning("Received candidates unexpectedly, current state: %s", iceState.getMessage());
                    return;
                }


            } else {
                if (iceState != NEW && iceState != DISCONNECTED) {
                    Logger.info("Received new candidates/offer, stopping...");
                    onConnectionLost();
                }

                initiateIce();
            }

            setState(CHECKING);
            CandidateUtil.unpackCandidates(remoteCandidatesMessage, agent, component, mediaStream);

            startIce();

        }).start();
    }

    private void startIce() {
        Logger.debug("Starting ICE for peer %d", peer.getRemoteId());
        agent.startConnectivityEstablishment();

        while (agent.getState() != IceProcessingState.COMPLETED) {//TODO include more?, maybe stop on COMPLETED, is that to early?
            try {
                Thread.sleep(20);
            } catch (InterruptedException e) {
                e.printStackTrace();
            }

            if (agent.getState() == IceProcessingState.FAILED) {
                onConnectionLost();
                return;
            }

        }

        Logger.debug("ICE terminated for %d", peer.getRemoteId());

        //We are connected
        connected = true;
        RPCService.onConnected(IceAdapter.id, peer.getRemoteId(), true);
        setState(CONNECTED);

        if (peer.isLocalOffer()) {
            connectivityChecker.start();
        }

        listenerThread = new Thread(this::listener);
        listenerThread.start();
    }

    public synchronized void onConnectionLost() {
        if (iceState == DISCONNECTED) {
            return;//TODO: will this kill the life cycle?
        }

        IceState previousState = getIceState();

        if (listenerThread != null) {
//            listenerThread.stop();//TODO what if cancelled during sending TO FA???
            listenerThread.interrupt();
            listenerThread = null;
        }

        connectivityChecker.stop();

        if (agent != null) {
            agent.free();
            agent = null;
            mediaStream = null;
            component = null;
        }

        setState(DISCONNECTED);

        if (connected) {
            connected = false;
            Logger.warning("ICE connection has been lost for peer %d", peer.getRemoteId());
            RPCService.onConnected(IceAdapter.id, peer.getRemoteId(), false);
        }

        if (previousState == CONNECTED && peer.isLocalOffer()) {
            Executor.executeDelayed(0, this::reinitIce);
        } else if (peer.isLocalOffer()) {
            Executor.executeDelayed(5000, this::reinitIce);
        }
    }

    private synchronized void reinitIce() {
        initiateIce();
    }

    void onFaDataReceived(byte faData[], int length) {
        byte[] data = new byte[length + 1];
        data[0] = 'd';
        System.arraycopy(faData, 0, data, 1, length);
        sendViaIce(data, 0, data.length);
    }

    void sendViaIce(byte[] data, int offset, int length) {
        if (connected) {
            try {
                component.getSelectedPair().getIceSocketWrapper().send(new DatagramPacket(data, offset, length, component.getSelectedPair().getRemoteCandidate().getTransportAddress().getAddress(), component.getSelectedPair().getRemoteCandidate().getTransportAddress().getPort()));
            } catch (IOException e) {
                Logger.warning("Failed to send data via ICE", e);
                onConnectionLost();
            }
        }
    }

    /**
     * Listens for data incoming via ice socket
     */
    public void listener() {
        Logger.debug("Now forwarding data from ICE to FA for peer %d", peer.getRemoteId());
        Component localComponent = component;

        byte data[] = new byte[65536];//64KiB = UDP MTU, in practice due to ethernet frames being <= 1500 B, this is often not used
        while (IceAdapter.running && IceAdapter.gameSession == peer.getGameSession()) {
            try {
                DatagramPacket packet = new DatagramPacket(data, data.length);
                localComponent.getSelectedPair().getIceSocketWrapper().getUDPSocket().receive(packet);

                if (packet.getLength() == 0) {
                    continue;
                }

                if (data[0] == 'd') {
                    //Received data
                    peer.onIceDataReceived(data, 1, packet.getLength() - 1);
                } else if (data[0] == 'e') {
                    //Received echo req/res
                    if (peer.isLocalOffer()) {
                        connectivityChecker.echoReceived(data, 0, packet.getLength());
                    } else {
                        sendViaIce(data, 0, packet.getLength());//Turn around, send echo back
                    }
                } else {
                    Logger.warning("Received invalid packet, first byte: 0x%x", data[0]);
                }

            } catch (IOException e) {//TODO: nullpointer from localComponent.xxxx????
                Logger.warning("Error while reading from ICE adapter, peer: %d", peer.getRemoteId());
                if(component == localComponent) {
                    onConnectionLost();
                }
                return;
            }
        }

        Logger.debug("No longer listening for messages from ICE");
    }

    void close() {
        agent.free();
    }
}
