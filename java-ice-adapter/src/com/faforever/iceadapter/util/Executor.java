package com.faforever.iceadapter.util;

public class Executor {

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
