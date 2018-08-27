package client.ice;

import client.TestServerAccessor;
import logging.Logger;
import net.IceMessage;

import java.util.List;

public class IceAdapterCallbacks {

	public IceAdapterCallbacks() {

	}

	public void onConnectionStateChanged(String newState) {
		Logger.debug("ICE adapter connection state changed to: %s", newState);
	}

	public void onGpgNetMessageReceived(String header, List<Object> chunks) {
		Logger.debug("Message from forgedalliance: '%s' '%s'", header, chunks);

//		if(header.equals("GameState") && chunks.get(0).equals("Lobby")) {
//			ICEAdapter.hostGame("mapName");//TODO
//		}
	}

	public void onIceMsg(long localPlayerId, long remotePlayerId, Object message) {
		Logger.debug("ICE message for connection '%d/%d': %s", localPlayerId, remotePlayerId, message);
		TestServerAccessor.send(new IceMessage((int)localPlayerId, (int)remotePlayerId, message));
	}

	public void onIceConnectionStateChanged(long localPlayerId, long remotePlayerId, String state) {
		Logger.debug("ICE connection state for peer '%s' changed to: %s", remotePlayerId, state);
	}

	public void onConnected(long localPlayerId, long remotePlayerId, boolean connected) {
		if (connected) {
			Logger.info("Connection between '%s' and '%s' has been established", localPlayerId, remotePlayerId);
		} else {
			Logger.info("Connection between '%s' and '%s' has been lost", localPlayerId, remotePlayerId);
		}
	}
}
