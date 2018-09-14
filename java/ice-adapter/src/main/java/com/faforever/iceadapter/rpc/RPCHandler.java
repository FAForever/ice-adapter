package com.faforever.iceadapter.rpc;

import com.faforever.iceadapter.IceAdapter;
import com.faforever.iceadapter.IceStatus;
import com.faforever.iceadapter.gpgnet.GPGNetServer;
import com.faforever.iceadapter.gpgnet.LobbyInitMode;
import com.faforever.iceadapter.ice.CandidatesMessage;
import com.faforever.iceadapter.ice.GameSession;
import com.faforever.iceadapter.ice.Peer;
import com.google.gson.Gson;
import lombok.extern.slf4j.Slf4j;
import org.ice4j.TransportAddress;
import org.ice4j.ice.Candidate;
import org.ice4j.ice.CandidatePair;
import org.ice4j.ice.CandidateType;
import org.ice4j.ice.Component;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Optional;

@Slf4j
public class RPCHandler {

    private Gson gson = new Gson();

    public RPCHandler() {

    }

    public void hostGame(String mapName) {
        IceAdapter.onHostGame(mapName);
    }

    public void joinGame(String remotePlayerLogin, long remotePlayerId) {
        IceAdapter.onJoinGame(remotePlayerLogin, (int) remotePlayerId);
    }

    public void connectToPeer(String remotePlayerLogin, long remotePlayerId, boolean offer) {
        IceAdapter.onConnectToPeer(remotePlayerLogin, (int) remotePlayerId, offer);
    }

    public void disconnectFromPeer(long remotePlayerId) {
        IceAdapter.onDisconnectFromPeer((int) remotePlayerId);
    }

    public void setLobbyInitMode(String lobbyInitMode) {
        GPGNetServer.lobbyInitMode = LobbyInitMode.getByName(lobbyInitMode);
        log.debug("LobbyInitMode set to {}", lobbyInitMode);
    }

    public void iceMsg(long remotePlayerId, Object msg) {
        boolean err = true;

        GameSession gameSession = IceAdapter.gameSession;
        if (gameSession != null) {//This is highly unlikely, game session got created if JoinGame/HostGame came first
            Peer peer = gameSession.getPeers().get((int) remotePlayerId);
            if (peer != null) {//This is highly unlikely, peer is present if connectToPeer was called first
                peer.getIce().onIceMessgageReceived(gson.fromJson((String) msg, CandidatesMessage.class));
                err = false;
            }
        }

        if (err) {
            log.error("ICE MESSAGE IGNORED for id: {}", remotePlayerId);
        }

        log.info("IceMsg received %s", msg);
    }

    public void sendToGpgNet(String header, String... chunks) {
        GPGNetServer.clientFuture.thenAccept(gpgNetClient -> {
            gpgNetClient.getLobbyFuture().thenRun(() -> {
                gpgNetClient.sendGpgnetMessage(header, chunks);
            });
        });
    }

    public void setIceServers(List<Map<String, Object>> iceServers) {
        GameSession.setIceServers(iceServers);
    }

    public String status() {
        IceStatus.IceGPGNetState gpgpnet = new IceStatus.IceGPGNetState(IceAdapter.GPGNET_PORT, GPGNetServer.isConnected(), GPGNetServer.getGameState(), "-");

        List<IceStatus.IceRelay> relays = new ArrayList<>();
        GameSession gameSession = IceAdapter.gameSession;
        if (gameSession != null) {
            synchronized (gameSession.getPeers()) {
                gameSession.getPeers().values().stream()
                        .map(peer -> {
                            IceStatus.IceRelay.IceRelayICEState iceRelayICEState = new IceStatus.IceRelay.IceRelayICEState(
                                    peer.isLocalOffer(),
                                    peer.getIce().getIceState().getMessage(),
                                    "",
                                    "",
                                    peer.getIce().isConnected(),
                                    Optional.ofNullable(peer.getIce().getComponent()).map(Component::getSelectedPair).map(CandidatePair::getLocalCandidate).map(Candidate::getHostAddress).map(TransportAddress::toString).orElse(""),
                                    Optional.ofNullable(peer.getIce().getComponent()).map(Component::getSelectedPair).map(CandidatePair::getRemoteCandidate).map(Candidate::getHostAddress).map(TransportAddress::toString).orElse(""),
                                    Optional.ofNullable(peer.getIce().getComponent()).map(Component::getSelectedPair).map(CandidatePair::getLocalCandidate).map(Candidate::getType).map(CandidateType::toString).orElse(""),
                                    Optional.ofNullable(peer.getIce().getComponent()).map(Component::getSelectedPair).map(CandidatePair::getRemoteCandidate).map(Candidate::getType).map(CandidateType::toString).orElse(""),
                                    -1.0
                            );

                            return new IceStatus.IceRelay(peer.getRemoteId(), peer.getRemoteLogin(), peer.getFaSocket().getLocalPort(), iceRelayICEState);
                        })
                        .forEach(relays::add);
            }
        }

        IceStatus status = new IceStatus(
                IceAdapter.VERSION,
                GameSession.getStunAddresses().size() + GameSession.getTurnAddresses().size(),
                IceAdapter.LOBBY_PORT,
                GPGNetServer.lobbyInitMode.getName(),
                new IceStatus.IceOptions(IceAdapter.id, IceAdapter.login, IceAdapter.RPC_PORT, IceAdapter.GPGNET_PORT),
                gpgpnet,
                relays.toArray(new IceStatus.IceRelay[relays.size()])
        );

        return gson.toJson(status);
    }

    public void quit() {
        log.warn("Close requested, stopping...");
        IceAdapter.close();
    }

}
