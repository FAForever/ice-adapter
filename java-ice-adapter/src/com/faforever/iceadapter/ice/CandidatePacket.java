package com.faforever.iceadapter.ice;

import lombok.Data;
import org.ice4j.ice.CandidateType;

@Data
public class CandidatePacket implements Comparable {
    private final String foundation;
    private final String protocol;
    private final long priority;
    private final String ip;
    private final int port;
    private final CandidateType type;
    private final int generation;
    private final String id;

    private String relAddr;
    private int relPort;

    @Override
    public int compareTo(Object o) {
        if (o == null || !(o instanceof CandidateType)) {
            return 0;
        } else {
            return (int) (((CandidatePacket)o).priority - this.priority);
        }
    }
}
