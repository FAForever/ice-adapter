package net;

import lombok.Getter;
import lombok.RequiredArgsConstructor;

@Getter
@RequiredArgsConstructor
public class HolePunchingMessage {
    private final int id;
    private final String address;
    private final int port;
}
