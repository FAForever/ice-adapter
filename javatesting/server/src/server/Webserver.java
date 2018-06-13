package server;

import com.google.gson.Gson;
import common.ICEAdapterTest;
import logging.Logger;
import net.ClientInformationMessage;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.Date;
import java.util.stream.Collectors;

import static spark.Spark.*;

public class Webserver {

	private static final Gson gson = new Gson();

	private static final String interfaceHTML = readFile("/overview.html");
	private static final String loginString = readFile("/login.html");

	public static void init() {

		port(ICEAdapterTest.TEST_SERVER_WEB_INTERFACE_PORT);

		get("/", (req, res) -> loginString);

		get("/login", (req, res) -> {
			if(req.queryParams("pw").equals("8102faf")) {
				req.session(true).attribute("loggedIn", true);
				res.redirect("/data/overview");
			} else {
				res.redirect("/");
			}
			return null;
		});


		path("/data", () -> {
			before("/*", (req, res) -> {
				if(! req.session(true).attributes().contains("loggedIn") || !req.session(true).attribute("loggedIn").equals(Boolean.TRUE)) {
					res.redirect("/");
					halt(401);
				}
			});

			get("/overview", (req, res) -> {
				String iceStatusLinks = "";
				String playerLogLinks = "";
				synchronized (TestServer.players) {
					iceStatusLinks = TestServer.players.stream().mapToInt(Player::getId).mapToObj(i -> String.format("<a href=\"/data/iceStatus?id=%d\">id: %d</a>", i, i)).collect(Collectors.joining("   "));
					playerLogLinks = TestServer.players.stream().mapToInt(Player::getId).mapToObj(i -> String.format("<a href=\"/data/playerLog?id=%d\">id: %d</a>", i, i)).collect(Collectors.joining("   "));
				}

				return String.format(interfaceHTML,
						new Date(System.currentTimeMillis()).toString(),
						TestServer.players.size(),
						iceStatusLinks,
						playerLogLinks,
						"<font face=\"monospace\">" + TestServer.getLatencyLog().replace("\n", "<br>") + "</font>",
						"<font face=\"monospace\">" + Logger.collectedLog.replace("\n", "<br>") + "</font>"
				);
			});


			get("/iceStatus", (req, res) -> {
				if(! req.queryParams().contains("id")) {
					return "400 Bad request";
				}
				int id = Integer.parseInt(req.queryParams("id"));
				if(TestServer.collectedData.containsKey(id)) {
					res.type("application/json");
					return gson.toJson(TestServer.collectedData.get(id).getInformationMessages().stream().reduce((l, r) -> r).orElse(null));
				} else {
					return "404 User ID not present";
				}
			});

			get("/playerLog", (req, res) -> {
				if(! req.queryParams().contains("id")) {
					return "400 Bad request";
				}
				int id = Integer.parseInt(req.queryParams("id"));
				if(TestServer.collectedData.containsKey(id)) {
					return "<font face=\"monospace\">" +
						TestServer.collectedData.get(id).getInformationMessages().stream().map(ClientInformationMessage::getClientLog).map(s -> s.replace("\n", "<br>")).collect(Collectors.joining("<br>")) +
						"</font>";
				} else {
					return "404 User ID not present";
				}
			});

			get("/writeLogs", (req, res) -> {
				TestServer.writeCollectedData();
				return "Logs written!";
			});

			get("/stop", (req, res) -> {
				new Thread(TestServer::close).start();
				return "Stopped.";
			});
		});
	}

	public static void close() {
		stop();
	}


	private static String readFile(String fileName) {
		StringBuilder sb = new StringBuilder();
		BufferedReader scanner = new BufferedReader(new InputStreamReader(TestServer.class.getResourceAsStream(fileName)));
		String line;
		try {
			while((line = scanner.readLine()) != null) {
				sb.append(line).append("\n");
			}
		} catch (IOException e) {
			e.printStackTrace();
		}
		return sb.toString();
	}
}
