package server;

import com.google.gson.Gson;
import logging.Logger;
import lombok.Data;

import java.io.File;
import java.io.IOException;
import java.util.Scanner;

@Data
public class IceTestServerConfig {

    public static IceTestServerConfig INSTANCE;

    public static void init() {
        StringBuilder sb = new StringBuilder();
        try {
            Scanner scanner = new Scanner(new File("iceServer.cfg"));
            while(scanner.hasNext()) {
                sb.append(scanner.nextLine()).append("\n");
            }
            INSTANCE = new Gson().fromJson(sb.toString(), IceTestServerConfig.class);
        } catch(IOException e) {
            Logger.crash(e);
        }
    }

    private final int max_users;
    private final boolean host;
    private final boolean stun;
    private final boolean turn;
}
