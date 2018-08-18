package com.faforever.iceadapter;

import com.faforever.iceadapter.gpgnet.GPGNetServer;
import com.faforever.iceadapter.ice.GameSession;
import com.faforever.iceadapter.logging.Logger;
import com.faforever.iceadapter.rpc.RPCService;
import com.faforever.iceadapter.util.ArgumentParser;
import com.faforever.iceadapter.util.Util;

import java.io.File;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Map;
import java.util.stream.Collectors;

public class IceAdapter {

    public static volatile boolean running = true;

    public static final String VERSION = "0.0.1";
    public static String COMMAND_LINE_ARGUMENTS;

    public static int id = -1;
    public static String login;
    public static int RPC_PORT;
    public static int GPGNET_PORT = 0;
    public static int LOBBY_PORT = 0;
    private static String logDirectory = null;

    public static volatile GameSession gameSession;

    public static void main(String args[]) {
        COMMAND_LINE_ARGUMENTS = Arrays.stream(args).collect(Collectors.joining(" "));
        interpretArguments(ArgumentParser.parse(args));

        if (logDirectory != null) {
            Util.mkdir(Paths.get(logDirectory).toFile());
            Logger.enableLogging();
            Logger.init("FAF ICE Adapter", Paths.get(logDirectory).resolve("iceAdapter.log").toFile());
        } else {
            Logger.disableLogging();
            Logger.init("FAF ICE Adapter", new File("iceAdapter.log"));
        }

        Logger.info("Version: %s", VERSION);

        GPGNetServer.init();
        RPCService.init();
    }

    public static void onHostGame(String mapName) {
        Logger.info("onHostGame");
        createGameSession();

        GPGNetServer.clientFuture.thenAccept(gpgNetClient -> {
            gpgNetClient.getLobbyFuture().thenRun(() -> {
                gpgNetClient.sendGpgnetMessage("HostGame", mapName);
            });
        });
    }

    public static void onJoinGame(String remotePlayerLogin, int remotePlayerId) {
        Logger.info("onJoinGame %d %s", remotePlayerId, remotePlayerLogin);
        createGameSession();
        int port = gameSession.connectToPeer(remotePlayerLogin, remotePlayerId, false);

        GPGNetServer.clientFuture.thenAccept(gpgNetClient -> {
            gpgNetClient.getLobbyFuture().thenRun(() -> {
                gpgNetClient.sendGpgnetMessage("JoinGame", "127.0.0.1:" + port, remotePlayerLogin, remotePlayerId);
            });
        });
    }

    public static void onConnectToPeer(String remotePlayerLogin, int remotePlayerId, boolean offer) {
        Logger.info("onConnectToPeer %d %s, offer: %s", remotePlayerId, remotePlayerLogin, String.valueOf(offer));
        int port = gameSession.connectToPeer(remotePlayerLogin, remotePlayerId, offer);

        GPGNetServer.clientFuture.thenAccept(gpgNetClient -> {
            gpgNetClient.getLobbyFuture().thenRun(() -> {
                gpgNetClient.sendGpgnetMessage("ConnectToPeer", "127.0.0.1:" + port, remotePlayerLogin, remotePlayerId);
            });
        });
    }

    public static void onDisconnectFromPeer(int remotePlayerId) {
        Logger.info("onDisconnectFromPeer %d", remotePlayerId);
        gameSession.disconnectFromPeer(remotePlayerId);

        GPGNetServer.clientFuture.thenAccept(gpgNetClient -> {
            gpgNetClient.getLobbyFuture().thenRun(() -> {
                gpgNetClient.sendGpgnetMessage("DisconnectFromPeer", remotePlayerId);
            });
        });
    }

    private synchronized static void createGameSession() {
        if (gameSession != null) {
            gameSession.close();
            gameSession = null;
        }

        gameSession = new GameSession();
    }

    /**
     * Triggered by losing gpgnet connection to FA.
     * Closes the active Game/ICE session
     */
    public synchronized static void onFAShutdown() {
        if(gameSession != null) {
            Logger.info("FA SHUTDOWN, closing everything");
            gameSession.close();
            gameSession = null;
            //Do not put code outside of this if clause, else it will be executed multiple times
        }
    }

    /**
     * Stop the ICE adapter
     */
    public static void close() {
        onFAShutdown();//will close gameSession aswell
        GPGNetServer.close();
        RPCService.close();

        Logger.close();
    }


    /**
     * Read command line arguments and set global, constant values
     * @param arguments The arguments to be read
     */
    public static void interpretArguments(Map<String, String> arguments) {
        if(arguments.containsKey("help")) {
            System.out.println("faf-ice-adapter usage:\n" +
                    "--help                               produce help message\n" +
                    "--id arg                             set the ID of the local player\n" +
                    "--login arg                          set the login of the local player, e.g. \"Rhiza\"\n" +
                    "--rpc-port arg (=7236)               set the port of internal JSON-RPC server\n" +
                    "--gpgnet-port arg (=0)               set the port of internal GPGNet server\n" +
                    "--lobby-port arg (=0)                set the port the game lobby should use for incoming UDP packets from the PeerRelay\n" +
                    "--log-directory arg                  set a log directory to write ice_adapter_0 log files\n");
            System.exit(0);
        }

        if(! Arrays.asList("id", "login").stream().allMatch(arguments::containsKey)) {
            Logger.crash("Missing necessary argument.");
        }

        id = Integer.parseInt(arguments.get("id"));
        login = arguments.get("login");
        if(arguments.containsKey("rpc-port")) {
            RPC_PORT = Integer.parseInt(arguments.get("rpc-port"));
        }
        if(arguments.containsKey("gpgnet-port")) {
            GPGNET_PORT = Integer.parseInt(arguments.get("gpgnet-port"));
        }
        if(arguments.containsKey("lobby-port")) {
            LOBBY_PORT = Integer.parseInt(arguments.get("lobby-port"));
        }
        if(arguments.containsKey("log-directory")) {
            logDirectory = arguments.get("log-directory");
        }
    }
}
