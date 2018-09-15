package com.faforever.iceadapter.gpgnet;

import lombok.Getter;
import lombok.RequiredArgsConstructor;

import java.util.Arrays;

@Getter
@RequiredArgsConstructor
/**
 * Lobby init mode, set by the client via RPC, transmitted to game via CreateLobby
 */
public enum LobbyInitMode {
    NORMAL("normal", 0), AUTO("auto", 1); //Normal = normal lobby, Auto = skip lobby screen (e.g. ranked)

    private final String name;
    private final int id;

    public static LobbyInitMode getByName(String name) {
        return Arrays.stream(LobbyInitMode.values()).filter(s -> s.getName().equals(name)).findFirst().orElse(null);
    }
}

