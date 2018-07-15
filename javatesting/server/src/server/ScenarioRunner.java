package server;

import server.scenarios.NoLogScenario;

public class ScenarioRunner {
    public static final Scenario scenario = new NoLogScenario();

    public static void start() {
        scenario.init();
        scenario.start();
    }
}
