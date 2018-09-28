package com.faforever.iceadapter.debug;

import com.faforever.iceadapter.ice.Peer;
import com.nbarraille.jjsonrpc.JJsonPeer;

import java.util.concurrent.CompletableFuture;

public class NoDebugger implements Debugger {


	@Override
	public void startupComplete() {

	}

	@Override
	public void rpcStarted(CompletableFuture<JJsonPeer> peerFuture) {

	}

	@Override
	public void gpgnetStarted() {

	}

	@Override
	public void gpgnetConnectedDisconnected() {

	}

	@Override
	public void gameStateChanged() {

	}

	@Override
	public void connectToPeer(int id, String login, boolean localOffer) {

	}

	@Override
	public void disconnectFromPeer(int id) {

	}

	@Override
	public void peerStateChanged(Peer peer) {

	}

	@Override
	public void peerConnectivityUpdate(Peer peer) {

	}
}
