package net;

import lombok.Data;

@Data
public class ConnectToPeerMessage {
	private final String remotePlayerLogin;
	private final int remotePlayerId;
	private final boolean offer;
}
