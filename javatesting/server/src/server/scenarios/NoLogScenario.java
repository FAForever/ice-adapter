package server.scenarios;

import net.ScenarioOptionsMessage;
import server.Game;
import server.Player;
import server.Scenario;
import server.TestServer;

public class NoLogScenario extends Scenario {


    @Override
    public void init() {
        //Set update interval (update will be called all X ms)
        this.init(1500);
    }

    @Override
    public synchronized void update(float d) {

    }

    @Override
    public synchronized void onPlayerConnect(Player player) {
        player.send(new ScenarioOptionsMessage(false, false));

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
        return "default - logging disabled";
    }
}
