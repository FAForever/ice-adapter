package net;

import lombok.AllArgsConstructor;
import lombok.Getter;
import lombok.NoArgsConstructor;

@Getter
@NoArgsConstructor
@AllArgsConstructor
public class ScenarioOptionsMessage {
    private boolean uploadLog = true;
    private boolean uploadIceStatus = true;
}
