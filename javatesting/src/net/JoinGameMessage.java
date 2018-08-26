package net;

import lombok.Data;

@Data
public class JoinGameMessage {
	private final String remotePlayerLogin;
	private final int remotePlayerId;
}
