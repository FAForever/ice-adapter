package server;

import lombok.Data;
import net.ClientInformationMessage;
import net.IceMessage;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;

@Data
public class CollectedInformation {
	private final int id;
	private final String username;

	private List<ClientInformationMessage> informationMessages = new LinkedList<>();
	private Map<Long, IceMessage> iceMessages = new HashMap<>();

	public String getLog() {
    return informationMessages.stream().map(ClientInformationMessage::getClientLog).reduce("", (l, r) -> l + r).replace("\\\\", "");//TODO: why is this necessary?
	}
}
