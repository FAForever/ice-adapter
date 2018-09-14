package com.faforever.iceadapter.ice;

import com.google.common.primitives.Longs;
import lombok.extern.slf4j.Slf4j;

import java.util.Arrays;

@Slf4j
public class PeerConnectivityCheckerModule {

    private static final int ECHO_INTERVAL = 1000;

    private final PeerIceModule ice;
    private volatile boolean running = false;
    private volatile Thread checkerThread;

    private float averageRTT = 0.0f;
    private long lastPacketReceived;

    public PeerConnectivityCheckerModule(PeerIceModule ice) {
        this.ice = ice;
    }

    synchronized void start() {
        if (running) {
            return;
        }

        running = true;
        log.debug("Starting connectivity checker for peer {}", ice.getPeer().getRemoteId());

        averageRTT = 0.0f;
        lastPacketReceived = System.currentTimeMillis();

        checkerThread = new Thread(this::checkerThread);
        checkerThread.start();
    }

    synchronized void stop() {
        if (!running) {
            return;
        }

        running = false;

        if (checkerThread != null) {
            checkerThread.stop();
            checkerThread = null;
        }
    }

    void echoReceived(byte[] data, int offset, int length) {
        if (length != 9) {
            log.warn("Received echo of wrong length, length: {}", length);
        }

        int rtt = (int) (System.currentTimeMillis() - Longs.fromByteArray(Arrays.copyOfRange(data, offset + 1, length)));
        if (averageRTT == 0) {
            averageRTT = rtt;
        } else {
            averageRTT = (float) averageRTT * 0.8f + (float) rtt * 0.2f;
        }

        lastPacketReceived = System.currentTimeMillis();

        log.debug("Received echo from {} after {} ms, averageRTT: {} ms", ice.getPeer().getRemoteId(), rtt, (int) averageRTT);
    }

    private void checkerThread() {
        while (running) {
            byte[] data = new byte[9];
            data[0] = 'e';

            System.arraycopy(Longs.toByteArray(System.currentTimeMillis()), 0, data, 1, 8);

            ice.sendViaIce(data, 0, data.length);

            try {
                Thread.sleep(ECHO_INTERVAL);
            } catch (InterruptedException e) {
                log.warn("Sleeping checkerThread was interrupted");
            }

            if (System.currentTimeMillis() - lastPacketReceived > 10000) {
                new Thread(ice::onConnectionLost).start();
                return;
            }
        }
    }
}
