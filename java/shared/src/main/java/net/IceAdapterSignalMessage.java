package net;

import lombok.Getter;
import lombok.RequiredArgsConstructor;

@Getter
@RequiredArgsConstructor
public class IceAdapterSignalMessage {
    private final String signal;//stop, cont, kill
}
