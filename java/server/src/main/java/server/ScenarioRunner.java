package server;

import server.scenarios.SlowJoinScenario;

public class ScenarioRunner {
    public static final Scenario scenario = new SlowJoinScenario();

    public static void start() {
        scenario.init();
        scenario.start();
    }
}
