package server;

public abstract class Scenario {

    private long UPDATE_INTERVAL = 500;

    public abstract void init();

    protected void init(long updateInterval) {
        this.UPDATE_INTERVAL = updateInterval;
    }
    public abstract void update(float d);
    public synchronized void onPlayerConnect(Player player) { }

    public void start() {
        new Thread(this::run).start();
    }

    private void run() {
        long time = System.currentTimeMillis();
        while(true) {
            while(System.currentTimeMillis() - time < UPDATE_INTERVAL) {
                try { Thread.sleep(UPDATE_INTERVAL / 10); } catch(InterruptedException e) {}
            }

            long newTime = System.currentTimeMillis();
            this.update(newTime - time);

            time = newTime;
        }
    }
}
