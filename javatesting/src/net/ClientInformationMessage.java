package net;

import data.ForgedAlliancePeer;
import data.IceStatus;
import lombok.Data;

import java.util.List;

@Data
public class ClientInformationMessage {

	private final String username;
	private final int playerId;
	private final long currentTimeMillis;
	private final IceStatus iceStatus;

	private final String clientLog;
	private final List<ForgedAlliancePeer> forgedAlliancePeers;

}
