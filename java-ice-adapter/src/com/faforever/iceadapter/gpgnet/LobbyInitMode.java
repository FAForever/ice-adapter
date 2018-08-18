package com.faforever.iceadapter.gpgnet;

import lombok.Getter;
import lombok.RequiredArgsConstructor;

import java.util.Arrays;

@Getter
@RequiredArgsConstructor
public enum LobbyInitMode {
    NORMAL("normal", 0), AUTO("auto", 1);

    private final String name;
    private final int id;

    public static LobbyInitMode getByName(String name) {
        return Arrays.stream(LobbyInitMode.values()).filter(s -> s.getName().equals(name)).findFirst().orElse(null);
    }
}

