package com.faforever.iceadapter.debug;

import com.faforever.iceadapter.ice.Peer;
import com.nbarraille.jjsonrpc.JJsonPeer;

import java.util.concurrent.CompletableFuture;

public interface Debugger {

	void startupComplete();

	void rpcStarted(CompletableFuture<JJsonPeer> peerFuture);

	void gpgnetStarted();

	void gpgnetConnectedDisconnected();

	void gameStateChanged();

	void connectToPeer(int id, String login, boolean localOffer);

	void disconnectFromPeer(int id);

	void peerStateChanged(Peer peer);

	void peerConnectivityUpdate(Peer peer);
}
