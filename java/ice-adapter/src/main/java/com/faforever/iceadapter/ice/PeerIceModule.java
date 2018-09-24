package com.faforever.iceadapter.ice;

import com.faforever.iceadapter.IceAdapter;
import com.faforever.iceadapter.rpc.RPCService;
import com.faforever.iceadapter.util.CandidateUtil;
import com.faforever.iceadapter.util.Executor;
import lombok.Getter;
import lombok.extern.slf4j.Slf4j;
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
@Slf4j
public class PeerIceModule {
    private static final int MINIMUM_PORT = 6112;//PORT (range +1000) to be used by ICE for communicating, each peer needs a seperate port

    private Peer peer;

    private Agent agent;
    private IceMediaStream mediaStream;
    private Component component;

    private volatile IceState iceState = NEW;
    private volatile boolean connected = false;
    private volatile Thread listenerThread;

    //Checks the connection by sending echo requests and initiates a reconnect if needed
    private final PeerConnectivityCheckerModule connectivityChecker = new PeerConnectivityCheckerModule(this);

    public PeerIceModule(Peer peer) {
        this.peer = peer;
    }

    /**
     * Updates the current iceState and informs the client via RPC
     * @param newState the new State
     */
    private void setState(IceState newState) {
        this.iceState = newState;
        RPCService.onIceConnectionStateChanged(IceAdapter.id, peer.getRemoteId(), iceState.getMessage());
    }

    /**
     * Will start the ICE Process
     */
    synchronized void initiateIce() {
        if (iceState != NEW && iceState != DISCONNECTED) {
            log.warn(getLogPrefix() + "ICE already in progress, aborting re initiation. current state: {}", iceState.getMessage());
            return;
        }

        setState(GATHERING);
        log.info(getLogPrefix() + "Initiating ICE for peer");

        createAgent();
        gatherCandidates();
    }

    /**
     * Creates an agent and media stream for handling the ICE
     */
    private void createAgent() {
        agent = new Agent();
        agent.setControlling(peer.isLocalOffer());

        mediaStream = agent.createMediaStream("faData");
    }

    /**
     * Gathers all local candidates, packs them into a message and sends them to the other peer via RPC
     */
    private void gatherCandidates() {
        log.info(getLogPrefix() + "Gathering ice candidates");
        GameSession.getIceServers().stream().flatMap(s -> s.getStunAddresses().stream()).map(StunCandidateHarvester::new).forEach(agent::addCandidateHarvester);
        GameSession.getIceServers().forEach(iceServer ->
                iceServer.getTurnAddresses().stream().map(a -> new TurnCandidateHarvester(a, new LongTermCredential(iceServer.getTurnUsername(), iceServer.getTurnCredential()))).forEach(agent::addCandidateHarvester)
        );

        try {
            component = agent.createComponent(mediaStream, Transport.UDP, MINIMUM_PORT + (int) (Math.random() * 999.0), MINIMUM_PORT, MINIMUM_PORT + 1000);
        } catch (IOException e) {
            log.error(getLogPrefix() + "Error while creating stream component", e);
            new Thread(this::reinitIce).start();
            return;
        }

        CandidatesMessage localCandidatesMessage = CandidateUtil.packCandidates(IceAdapter.id, peer.getRemoteId(), agent, component);
        log.debug(getLogPrefix() + "Sending own candidates to {}", peer.getRemoteId());
        setState(AWAITING_CANDIDATES);
        RPCService.onIceMsg(localCandidatesMessage);

        //TODO: is this a good fix for awaiting candidates loop????
        //Make sure to abort the connection process and reinitiate when we haven't received an answer to our offer in 6 seconds, candidate packet was probably lost
        final int currentacei = ++awaitingCandidatesEventId;
        Executor.executeDelayed(6000, () -> {
            if(peer.isClosing()) {
                log.warn(getLogPrefix() + "Peer {} not connected anymore, aborting reinitiation of ICE", peer.getRemoteId());
                return;
            }
            if (iceState == AWAITING_CANDIDATES && currentacei == awaitingCandidatesEventId) {
                onConnectionLost();
            }
        });
    }

    //How often have we been waiting for a response to local candidates/offer
    private volatile int awaitingCandidatesEventId = 0;

    /**
     * Starts harvesting local candidates if in answer mode, then initiates the actual ICE process
     * @param remoteCandidatesMessage
     */
    public synchronized void onIceMessgageReceived(CandidatesMessage remoteCandidatesMessage) {
        if(peer.isClosing()) {
            log.warn(getLogPrefix() + "Peer not connected anymore, discarding ice message");
            return;
        }

        //Start ICE async as it's blocking and this is the RPC thread
        new Thread(() -> {
            log.debug(getLogPrefix() + "Got IceMsg for peer");

            if (peer.isLocalOffer()) {
                if (iceState != AWAITING_CANDIDATES) {
                    log.warn(getLogPrefix() + "Received candidates unexpectedly, current state: {}", iceState.getMessage());
                    return;
                }

            } else {
                //Check if we are already processing an ICE offer and if so stop it
                if (iceState != NEW && iceState != DISCONNECTED) {
                    log.info(getLogPrefix() + "Received new candidates/offer, stopping...");
                    onConnectionLost();
                }

                //Answer mode, initialize agent and gather candidates
                initiateIce();
            }

            setState(CHECKING);
            CandidateUtil.unpackCandidates(remoteCandidatesMessage, agent, component, mediaStream);

            startIce();

        }).start();
    }

    /**
     * Runs the actual connectivity establishment, candidates have been exchanged and need to be checked
     */
    private void startIce() {
        log.debug(getLogPrefix() + "Starting ICE for peer {}", peer.getRemoteId());
        agent.startConnectivityEstablishment();

        //Wait for termination/completion of the agent
        long iceStartTime = System.currentTimeMillis();
        while (agent.getState() != IceProcessingState.COMPLETED) {//TODO include more?, maybe stop on COMPLETED, is that to early?
            try {
                Thread.sleep(20);
            } catch (InterruptedException e) {
                log.error(getLogPrefix() + "Interrupted while waiting for ICE", e);
            }

            if (agent.getState() == IceProcessingState.FAILED) {//TODO null pointer due to no agent?
                onConnectionLost();
                return;
            }


            if(System.currentTimeMillis() - iceStartTime > 15_000) {
                log.error(getLogPrefix() + "ABORTING ICE DUE TO TIMEOUT");
                onConnectionLost();
                return;
            }
        }

        log.debug(getLogPrefix() + "ICE terminated");

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

    /**
     * Connection has been lost, ice failed or we received a new offer
     * Will close agent, stop listener and connectivity checker thread and change state to disconnected
     * Will then reinitiate ICE
     */
    public synchronized void onConnectionLost() {
        if(peer.isClosing()) {
            log.warn(getLogPrefix() + "Peer not connected anymore, aborting onConnectionLost of ICE");
            return;
        }

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
            log.warn(getLogPrefix() + "ICE connection has been lost for peer");
            RPCService.onConnected(IceAdapter.id, peer.getRemoteId(), false);
        }

        if (previousState == CONNECTED && peer.isLocalOffer()) {
            //We were connected before, retry immediately
            Executor.executeDelayed(0, this::reinitIce);
        } else if (peer.isLocalOffer()) {
            //Last ice attempt didn't succeed, so wait a bit
            Executor.executeDelayed(5000, this::reinitIce);
        }
    }

    private synchronized void reinitIce() {
        if(peer.isClosing()) {
            log.warn(getLogPrefix() + "Peer not connected anymore, aborting reinitiation of ICE");
            return;
        }
        initiateIce();
    }

    /**
     * Data received from FA, prepends prefix and sends it via ICE to the other peer
     * @param faData
     * @param length
     */
    void onFaDataReceived(byte faData[], int length) {
        byte[] data = new byte[length + 1];
        data[0] = 'd';
        System.arraycopy(faData, 0, data, 1, length);
        sendViaIce(data, 0, data.length);
    }


    /**
     * Send date via ice to the other peer
     * @param data
     * @param offset
     * @param length
     */
    void sendViaIce(byte[] data, int offset, int length) {
        if (connected) {
            try {
                component.getSelectedPair().getIceSocketWrapper().send(new DatagramPacket(data, offset, length, component.getSelectedPair().getRemoteCandidate().getTransportAddress().getAddress(), component.getSelectedPair().getRemoteCandidate().getTransportAddress().getPort()));
            } catch (IOException e) {
                log.warn(getLogPrefix() + "Failed to send data via ICE", e);
                onConnectionLost();
            }
        }
    }

    /**
     * Listens for data incoming via ice socket
     */
    public void listener() {
        log.debug(getLogPrefix() + "Now forwarding data from ICE to FA for peer");
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
                    log.warn(getLogPrefix() + "Received invalid packet, first byte: 0x{}", data[0]);
                }

            } catch (IOException e) {//TODO: nullpointer from localComponent.xxxx????
                log.warn(getLogPrefix() + "Error while reading from ICE adapter");
                if(component == localComponent) {
                    onConnectionLost();
                }
                return;
            }
        }

        log.debug(getLogPrefix() + "No longer listening for messages from ICE");
    }

    void close() {
        agent.free();
    }

    public String getLogPrefix() {
        return String.format("ICE %s: ", peer.getPeerIdentifier());
    }
}
