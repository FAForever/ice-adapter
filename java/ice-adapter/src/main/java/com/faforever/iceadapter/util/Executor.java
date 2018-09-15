package com.faforever.iceadapter.util;

public class Executor {

    /**
     * Creates a that sleeps for a give time and then calls the provided runnable
     * @param timeMs The time to wait in ms
     * @param runnable The runnable
     */
    public static void executeDelayed(int timeMs, Runnable runnable) {
        new Thread(() -> {
            try {
                Thread.sleep(timeMs);
            } catch (InterruptedException e) {
            }
            runnable.run();
        }).start();
    }

}
