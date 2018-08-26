package server.scenarios;

import server.Game;
import server.Player;
import server.Scenario;
import server.TestServer;

public class SlowJoinScenario extends Scenario {


    @Override
    public void init() {
        //Set update interval (update will be called all X ms)
        this.init(3000);
    }

    @Override
    public synchronized void update(float d) {
        synchronized (TestServer.games) {
            if(TestServer.games.size() > 0) {
                TestServer.players.stream().filter(p -> ! TestServer.games.get(0).getPlayers().contains(p)).findFirst().ifPresent(TestServer.games.get(0)::join);
            }
        }
    }

    @Override
    public synchronized void onPlayerConnect(Player player) {
        synchronized (TestServer.games) {
            if(TestServer.games.isEmpty()) {
                TestServer.games.add(new Game(player));
            }
        }
    }

    @Override
    public String getDescription() {
        return "delay joins";
    }
}
