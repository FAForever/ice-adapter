package com.faforever.iceadapter.rpc;

import com.faforever.iceadapter.IceAdapter;
import com.faforever.iceadapter.ice.CandidatesMessage;
import com.faforever.iceadapter.logging.Logger;
import com.google.gson.Gson;
import com.nbarraille.jjsonrpc.JJsonPeer;
import com.nbarraille.jjsonrpc.TcpServer;

import java.util.Arrays;
import java.util.List;

/**
 * Handles communication between client and adapter, opens a server for the client to connect to
 */
public class RPCService {

    private static Gson gson = new Gson();

    private static TcpServer tcpServer;
    private static RPCHandler rpcHandler;

    public static void init() {
        Logger.info("Creating RPC server on port %s", IceAdapter.RPC_PORT);

        rpcHandler = new RPCHandler();
        tcpServer = new TcpServer(IceAdapter.RPC_PORT, rpcHandler);
        tcpServer.start();
    }

    public static void onConnectionStateChanged(String newState) {
        getPeerOrWait().sendAsyncRequest("onConnectionStateChanged", Arrays.asList(newState), null, false);
    }

    public static void onGpgNetMessageReceived(String header, List<Object> chunks) {
        getPeerOrWait().sendAsyncRequest("onGpgNetMessageReceived", Arrays.asList(header, chunks), null, false);
    }

    public static void onIceMsg(CandidatesMessage candidatesMessage) {
        getPeerOrWait().sendAsyncRequest("onIceMsg", Arrays.asList(candidatesMessage.getSrcId(), candidatesMessage.getDestId(), gson.toJson(candidatesMessage)), null, false);
    }

    public static void onIceConnectionStateChanged(long localPlayerId, long remotePlayerId, String state) {
        getPeerOrWait().sendAsyncRequest("onIceConnectionStateChanged", Arrays.asList(localPlayerId, remotePlayerId, state), null, false);
    }

    public static void onConnected(long localPlayerId, long remotePlayerId, boolean connected) {
        getPeerOrWait().sendAsyncRequest("onConnected", Arrays.asList(localPlayerId, remotePlayerId, connected), null, false);
    }


    public static JJsonPeer getPeerOrWait() {
        try {
            return tcpServer.getFirstPeer().get();
        } catch (Exception e) {
            Logger.error(e);
        }
        return null;
    }

    public static void close() {
        tcpServer.stop();
    }
}
