package client;

import client.ice.ICEAdapter;
import common.ICEAdapterTest;
import data.ForgedAlliancePeer;
import javafx.application.Application;
import javafx.application.Platform;
import javafx.beans.property.ReadOnlyStringWrapper;
import javafx.beans.value.ObservableValue;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.geometry.Insets;
import javafx.geometry.Orientation;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.HBox;
import javafx.scene.layout.Region;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;
import javafx.util.Callback;
import logging.Logger;

import java.util.Optional;
import java.util.OptionalDouble;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CountDownLatch;

public class GUI extends Application {

	public static CompletableFuture<GUI> started;
	public static GUI instance;

	public Stage stage;

	public TableView table;
	private TableColumn[] tableColumns = { new TableColumn<>("ID"), new TableColumn<>("name"), new TableColumn<>("latency"), new TableColumn<>("last packet received") };

	private ObservableList<ForgedAlliancePeer> peers = FXCollections.observableArrayList();

	public static void showUsernameDialog() {
		runAndWait(() -> {
			TextInputDialog nameDialog = new TextInputDialog("iceTester" + (int)(Math.random() * 10000));
			nameDialog.setTitle("ICE adapter test");
			nameDialog.setHeaderText("Welcome to the ICE adapter test client");
			nameDialog.setContentText("Please enter a username:");

			if(! TestClient.DEBUG_MODE) {
				nameDialog.showAndWait().ifPresent(name -> {
					if (name.length() > 0) {
						TestClient.username = name;
					} else {
						Logger.error("invalid username provided: %s", name);
						System.exit(1);
					}
				});
			} else {
				TestClient.username = "iceTester" + (int)(Math.random() * 10000);
			}

			if (TestClient.username == null) {
				Logger.error("no username provided");
				System.exit(-1);
			}
		});
	}

	public static void showGDPRDialog() {
		runAndWait(() -> {
			Alert gdprDialog = new Alert(Alert.AlertType.CONFIRMATION);
			gdprDialog.setTitle("ICE adapter test");
			gdprDialog.setHeaderText("Data usage");
			gdprDialog.setContentText(ICEAdapterTest.GDPR);
			gdprDialog.getDialogPane().setMinHeight(Region.USE_PREF_SIZE);

			if(! TestClient.DEBUG_MODE) {
				Optional<ButtonType> result = gdprDialog.showAndWait();
				if(! result.isPresent() || result.get() != ButtonType.OK) {
					System.exit(0);
				}
			}
		});
	}

	public static CompletableFuture<Alert> showDialog(String s) {
		CompletableFuture<Alert> future = new CompletableFuture<>();
		Platform.runLater(() -> {
			Alert alert = new Alert(Alert.AlertType.INFORMATION);
			alert.setTitle("ICE adapter test");
			alert.setHeaderText(s);
			alert.show();
			future.complete(alert);
		});
		return future;
	}

	public static void init(String args[]) {
		Platform.setImplicitExit(false);

		started = new CompletableFuture<>();
		new Thread(() -> launch(args)).start();
		started.join();

		new Thread(GUI.instance::uiUpdateThread).start();
	}

	public static void runAndWait(Runnable action) {
		if (Platform.isFxApplicationThread()) {
			action.run();
			return;
		}

		CountDownLatch completed = new CountDownLatch(1);
		Platform.runLater(() -> {
			try {
				action.run();
			} finally {
				completed.countDown();
			}
		});

		try {
			completed.await();
		} catch (InterruptedException e) {
		}
	}

	@Override
	public void start(Stage stage) {
		instance = this;
		this.stage = stage;
		stage.setTitle("ICE adapter testclient");
		stage.setWidth(700);
		stage.setHeight(800);

		stage.show();
		stage.hide();

		VBox root = new VBox();
		root.setSpacing(10);
		root.setPadding(new Insets(10, 10, 10, 10));
		HBox connectionStatus = new HBox();

		Label serverConnectionStatus = new Label("Connected: false");
		TestServerAccessor.connected.addListener(((observable, oldValue, newValue) -> runAndWait(() -> serverConnectionStatus.setText("TestServer: " + newValue + "(" + TestClient.playerID + ")"))));
		connectionStatus.getChildren().add(serverConnectionStatus);


		Separator separator = new Separator(Orientation.VERTICAL);
		separator.setMinWidth(30);
		connectionStatus.getChildren().add(separator);

		Label iceConnectionStatus = new Label("ICE: false");
		ICEAdapter.connected.addListener(((observable, oldValue, newValue) -> runAndWait(() -> iceConnectionStatus.setText("ICE: " + newValue))));
		connectionStatus.getChildren().add(iceConnectionStatus);

		separator = new Separator(Orientation.VERTICAL);
		separator.setMinWidth(30);
		connectionStatus.getChildren().add(separator);

		Label gameRunningStatus = new Label("ForgedAlliance: false");
		TestClient.isGameRunning.addListener(((observable, oldValue, newValue) -> runAndWait(() -> gameRunningStatus.setText("ForgedAlliance: " + (newValue ? "running" : "stopped")))));
		connectionStatus.getChildren().add(gameRunningStatus);


		root.getChildren().add(connectionStatus);


		HBox FAstatus = new HBox();
		FAstatus.visibleProperty().bind(TestClient.isGameRunning);
		FAstatus.managedProperty().bind(TestClient.isGameRunning);

		FAstatus.getChildren().add(peerCount);
		separator = new Separator(Orientation.VERTICAL);
		separator.setMinWidth(30);
		FAstatus.getChildren().add(separator);
		FAstatus.getChildren().add(connectedCount);
		separator = new Separator(Orientation.VERTICAL);
		separator.setMinWidth(30);
		FAstatus.getChildren().add(separator);
		FAstatus.getChildren().add(quietCount);

		root.getChildren().add(FAstatus);



		separator = new Separator(Orientation.HORIZONTAL);
		root.getChildren().add(separator);
		root.getChildren().add(new Label("ICE Test is running. Please keep this window open."));

		separator = new Separator(Orientation.HORIZONTAL);
		root.getChildren().add(separator);


		table = new TableView();
		tableColumns[0].setMinWidth(50);
		tableColumns[1].setMinWidth(150);
		tableColumns[2].setMinWidth(50);
		tableColumns[3].setMinWidth(200);
		//TODO: use lambdas
		tableColumns[0].setCellValueFactory(new Callback<TableColumn.CellDataFeatures<ForgedAlliancePeer, String>, ObservableValue<String>>() {
			@Override
			public ObservableValue<String> call(TableColumn.CellDataFeatures<ForgedAlliancePeer, String> cellData) {
				return new ReadOnlyStringWrapper(String.valueOf(cellData.getValue().getRemoteId()));
			}
		});
		tableColumns[1].setCellValueFactory(new Callback<TableColumn.CellDataFeatures<ForgedAlliancePeer, String>, ObservableValue<String>>() {
			@Override
			public ObservableValue<String> call(TableColumn.CellDataFeatures<ForgedAlliancePeer, String> cellData) {
				return new ReadOnlyStringWrapper(cellData.getValue().getRemoteUsername());
			}
		});
		tableColumns[2].setCellValueFactory(new Callback<TableColumn.CellDataFeatures<ForgedAlliancePeer, String>, ObservableValue<String>>() {
			@Override
			public ObservableValue<String> call(TableColumn.CellDataFeatures<ForgedAlliancePeer, String> cellData) {
				synchronized (cellData.getValue().getLatencies()) {
					OptionalDouble lat = cellData.getValue().getLatencies().stream().mapToInt(Integer::intValue).average();
					if(lat.isPresent()) {
						return new ReadOnlyStringWrapper(String.format("%.0f ms", lat.getAsDouble()));
					} else {
						return new ReadOnlyStringWrapper("");
					}
				}
			}
		});
		tableColumns[3].setCellValueFactory(new Callback<TableColumn.CellDataFeatures<ForgedAlliancePeer, String>, ObservableValue<String>>() {
			@Override
			public ObservableValue<String> call(TableColumn.CellDataFeatures<ForgedAlliancePeer, String> cellData) {
				return new ReadOnlyStringWrapper(String.valueOf(System.currentTimeMillis() - cellData.getValue().getLastPacketReceived()) + " ms");
			}
		});
		table.setItems(peers);
		table.getColumns().addAll(tableColumns);
		root.getChildren().add(table);


		separator = new Separator(Orientation.HORIZONTAL);
		root.getChildren().add(separator);


		HBox iceAdapterControls = new HBox();
		Button stopButton = new Button("SIGSTOP");
		stopButton.setOnAction(e -> {
			ICEAdapter.sigStop();
		});
		iceAdapterControls.getChildren().add(stopButton);
		Button contButton = new Button("SIGCONT");
		contButton.setOnAction(e -> {
			ICEAdapter.sigCont();
		});
		iceAdapterControls.getChildren().add(contButton);
		Button killButton = new Button("SIGKILL");
		killButton.setOnAction(e -> {
			ICEAdapter.sigKill();
		});
		iceAdapterControls.getChildren().add(killButton);

		if(System.getProperty("os.name").contains("Windows")) {
			stopButton.setDisable(true);
			killButton.setDisable(true);
		}


		root.getChildren().add(iceAdapterControls);




		stage.setScene(new Scene(root, stage.getWidth(), stage.getHeight()));


		stage.setOnCloseRequest(e -> {
			Logger.info("Close requested");
			TestClient.close();
		});

		started.complete(instance);
	}

	private Label peerCount = new Label();
	private Label connectedCount = new Label();
	private Label quietCount = new Label();

	private void uiUpdateThread() {
		while(true) {
			Platform.runLater(() -> {
				if(TestClient.isGameRunning.get() && TestClient.forgedAlliance.peers != null) {
					synchronized (TestClient.forgedAlliance.peers) {

						peerCount.setText("Peers: " + TestClient.forgedAlliance.peers.size());
						connectedCount.setText("Connected: " + TestClient.forgedAlliance.peers.stream().filter(ForgedAlliancePeer::isConnected).count());
						quietCount.setText("Quiet: " + TestClient.forgedAlliance.peers.stream().filter(ForgedAlliancePeer::isQuiet).count());

						this.peers.clear();
						this.peers.addAll(TestClient.forgedAlliance.peers);
					}
				}
			});

			try { Thread.sleep(100); } catch(InterruptedException e) {}
		}
	}

	public void showStage() {
		runAndWait(() -> {
			stage.show();
			stage.centerOnScreen();
		});
	}
}
