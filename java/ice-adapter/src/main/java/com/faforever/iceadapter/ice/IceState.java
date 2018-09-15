package com.faforever.iceadapter.ice;

import lombok.AllArgsConstructor;
import lombok.Getter;

@Getter
@AllArgsConstructor
/**
 * IceState, does not match WebRTC states, represents peer connection "lifecycle"
 */
public enum IceState {
    NEW("new"), GATHERING("gathering"), AWAITING_CANDIDATES("awaitingCandidates"), CHECKING("checking"), CONNECTED("connected"), COMPLETED("completed"), DISCONNECTED("disconnected");

    private final String message;
}