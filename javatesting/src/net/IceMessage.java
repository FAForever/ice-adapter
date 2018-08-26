package net;

import lombok.Data;

@Data
public class IceMessage {
	private final int srcPlayerId;
	private final int destPlayerId;
	private final Object msg;
}
