package com.faforever.iceadapter.ice;

import lombok.Data;

import java.util.ArrayList;
import java.util.List;

@Data
public class CandidatesMessage {
    private final int srcId;
    private final int destId;
    private final String password;
    private final String ufrag;

    private final List<CandidatePacket> candidates = new ArrayList<>();
}
