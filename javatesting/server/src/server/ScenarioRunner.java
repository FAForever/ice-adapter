package server;

import server.scenarios.DefaultScenario;

public class ScenarioRunner {
    public static final Scenario scenario = new DefaultScenario();

    public static void start() {
        scenario.init();
        scenario.start();
    }
}
