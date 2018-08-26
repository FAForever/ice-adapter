package net;

import lombok.Data;

@Data
public class DisconnectFromPeerMessage {
	private final int remotePlayerId;
}
