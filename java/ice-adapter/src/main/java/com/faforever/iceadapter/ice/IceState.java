package com.faforever.iceadapter.ice;

import lombok.AllArgsConstructor;
import lombok.Getter;

@Getter
@AllArgsConstructor
public enum IceState {
    NEW("new"), GATHERING("gathering"), AWAITING_CANDIDATES("awaitingCandidates"), CHECKING("checking"), CONNECTED("connected"), COMPLETED("completed"), DISCONNECTED("disconnected");

    private final String message;
}