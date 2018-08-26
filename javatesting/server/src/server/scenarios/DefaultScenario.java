package server.scenarios;

import server.Game;
import server.Player;
import server.Scenario;
import server.TestServer;

import java.util.LinkedList;
import java.util.List;

public class DefaultScenario extends Scenario {


    @Override
    public void init() {
        //Set update interval (update will be called all X ms)
        this.init(1500);
    }

    private static List<Player> stoppedAdapters = new LinkedList<>();

    @Override
    public synchronized void update(float d) {
        synchronized (TestServer.games) {
            for(Game game : TestServer.games) {
                for(Player player : TestServer.players) {
                    //Do sth in here...

//                    if(Math.random() < 0.01) {
//                        player.sigKill();
//                    }
                }
            }
        }
    }

    @Override
    public synchronized void onPlayerConnect(Player player) {
        synchronized (TestServer.games) {
            if(TestServer.games.isEmpty()) {
                TestServer.games.add(new Game(player));
            } else {
                TestServer.games.get(0).join(player);
            }
        }
    }

    @Override
    public String getDescription() {
        return "default - single game";
    }
}
