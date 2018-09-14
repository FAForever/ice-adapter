package com.faforever.iceadapter.gpgnet;

import com.faforever.iceadapter.IceAdapter;
import com.faforever.iceadapter.rpc.RPCService;
import com.faforever.iceadapter.util.NetworkToolbox;
import lombok.Getter;
import lombok.extern.slf4j.Slf4j;

import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketException;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Collectors;

@Slf4j
public class GPGNetServer {

    private static ServerSocket serverSocket;
    private static volatile GPGNetClient currentClient;

    //Used by other services to get a callback on FA connecting
    public static volatile CompletableFuture<GPGNetClient> clientFuture = new CompletableFuture<>();

    public static volatile LobbyInitMode lobbyInitMode = LobbyInitMode.NORMAL;


    public static void init() {
        if (IceAdapter.GPGNET_PORT == 0) {
            IceAdapter.GPGNET_PORT = NetworkToolbox.findFreeTCPPort(20000, 65536);
            log.info("Generated GPGNET_PORT: {}", IceAdapter.GPGNET_PORT);
        } else {
            log.info("Using GPGNET_PORT: {}", IceAdapter.GPGNET_PORT);
        }

        if (IceAdapter.LOBBY_PORT == 0) {
            IceAdapter.LOBBY_PORT = NetworkToolbox.findFreeUDPPort(20000, 65536);
            log.info("Generated LOBBY_PORT: {}", IceAdapter.LOBBY_PORT);
        } else {
            log.info("Using LOBBY_PORT: {}", IceAdapter.LOBBY_PORT);
        }

        try {
            serverSocket = new ServerSocket(IceAdapter.GPGNET_PORT);
        } catch (IOException e) {
            log.error("Couldn't start GPGNetServer", e);
            System.exit(-1);
        }

        new Thread(GPGNetServer::acceptThread).start();
        log.info("GPGNetServer started");
    }

    /**
     * Represents a client (a game instance) connected to this GPGNetServer
     */
    @Getter
    public static class GPGNetClient {
        private volatile GameState gameState = GameState.NONE;

        private Socket socket;
        private Thread listenerThread;
        private volatile boolean stopping = false;
        private FaDataOutputStream gpgnetOut;
        private CompletableFuture<GPGNetClient> lobbyFuture = new CompletableFuture<>();

        private GPGNetClient(Socket socket) {
            this.socket = socket;

            try {
                gpgnetOut = new FaDataOutputStream(socket.getOutputStream());
            } catch (IOException e) {
                log.error("Could not create GPGNet output steam to FA", e);
            }

            listenerThread = new Thread(this::listenerThread);
            listenerThread.start();

            RPCService.onConnectionStateChanged("Connected");
            log.info("GPGNetClient has connected");
        }


        /**
         * Process an incoming message from FA
         */
        private void processGpgnetMessage(String command, List<Object> args) {
            switch (command) {
                case "GameState": {
                    gameState = GameState.getByName((String) args.get(0));
                    log.debug("New GameState: {}", gameState.getName());

                    if (gameState == GameState.IDLE) {
                        sendGpgnetMessage("CreateLobby", lobbyInitMode.getId(), IceAdapter.LOBBY_PORT, IceAdapter.login, IceAdapter.id, 1);
                    } else if (gameState == GameState.LOBBY) {
                        lobbyFuture.complete(this);
                    }

                    break;
                }


                default: {

                }
            }

            log.info("Received GPGNet message: {} {}", command, args.stream().map(Object::toString).collect(Collectors.joining(" ")));
            RPCService.onGpgNetMessageReceived(command, args);
        }

        /**
         * Send a message to this FA instance via GPGNet
         */
        public synchronized void sendGpgnetMessage(String command, Object... args) {
            try {
                gpgnetOut.writeMessage(command, args);
            } catch (IOException e) {
                log.error("Error while communicating with FA (output), assuming shutdown", e);
                GPGNetServer.onGpgnetConnectionLost();
            }
        }

        /**
         * Listens for incoming messages from FA
         */
        private void listenerThread() {
            log.debug("Listening for GPG messages");
            boolean triggerActive = false;//Prevents a race condition between this thread and the thread that has created this object and is now going to set GPGNetServer.currentClient
            try {
                FaDataInputStream gpgnetIn = new FaDataInputStream(socket.getInputStream());

                while ((!triggerActive || GPGNetServer.currentClient == this) && !stopping) {
                    String command = gpgnetIn.readString();
                    List<Object> args = gpgnetIn.readChunks();

                    processGpgnetMessage(command, args);

                    if (!triggerActive && GPGNetServer.currentClient != null) {
                        triggerActive = true;//From now on we will check GPGNetServer.currentClient to see if we should stop
                    }
                }
            } catch (IOException e) {
                log.error("Error while communicating with FA (input), assuming shutdown", e);
                GPGNetServer.onGpgnetConnectionLost();
            }
            log.debug("No longer listening for GPGPNET from FA");
        }

        public void close() {
            stopping = true;
            this.listenerThread.interrupt();
            log.debug("Closing GPGNetClient");

            try {
                socket.close();
            } catch (IOException e) {
                log.error("Error while closing GPGNetClient socket", e);
            }
        }
    }

    /**
     * Closes all connections to the current client, removes this client.
     * To be called on encountering an error during the communication with the game instance
     * or on receiving an incoming connection request while still connected to a different instance.
     * THIS TRIGGERS A DISCONNECT FROM ALL PEERS AND AN ICE SHUTDOWN.
     */
    private static void onGpgnetConnectionLost() {
        log.info("GPGNet connection lost");
        synchronized (serverSocket) {
            if (currentClient != null) {
                currentClient.close();
                currentClient = null;

                if (clientFuture.isDone()) {
                    clientFuture = new CompletableFuture<>();
                }

                RPCService.onConnectionStateChanged("Disconnected");

                IceAdapter.onFAShutdown();
            }
        }
    }

    /**
     * Listens for incoming connections from a game instance
     */
    private static void acceptThread() {
        while (IceAdapter.running) {
            try {
                Socket socket = serverSocket.accept();
                synchronized (serverSocket) {
                    if (currentClient != null) {
                        onGpgnetConnectionLost();
                    }

                    currentClient = new GPGNetClient(socket);
                    clientFuture.complete(currentClient);
                }
            } catch (SocketException e) {
                return;
            } catch (IOException e) {
                log.error("Could not listen on socket", e);
            }
        }
    }

    public static boolean isConnected() {
        return currentClient != null;
    }

    public static String getGameState() {
        return Optional.ofNullable(currentClient).map(GPGNetClient::getGameState).map(GameState::getName).orElse("");
    }

    /**
     * Stops the GPGNetServer and thereby the conenction to a currently connected client
     */
    public static void close() {
        if (currentClient != null) {
            currentClient = null;
            currentClient.close();
            clientFuture = new CompletableFuture<>();
        }

        if (serverSocket != null) {
            try {
                serverSocket.close();
            } catch (IOException e) {
                log.error("Could not close gpgnet server socket", e);
            }
        }
        log.info("GPGNetServer stopped");
    }
}
