package com.faforever.iceadapter.debug;

import com.faforever.iceadapter.IceAdapter;
import com.faforever.iceadapter.gpgnet.GPGNetServer;
import com.faforever.iceadapter.gpgnet.GameState;
import com.faforever.iceadapter.ice.Peer;
import com.faforever.iceadapter.ice.PeerConnectivityCheckerModule;
import com.nbarraille.jjsonrpc.JJsonPeer;
import javafx.application.Application;
import javafx.application.Platform;
import javafx.beans.property.SimpleBooleanProperty;
import javafx.beans.property.SimpleDoubleProperty;
import javafx.beans.property.SimpleIntegerProperty;
import javafx.beans.property.SimpleStringProperty;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.event.Event;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.stage.Stage;
import lombok.AllArgsConstructor;
import lombok.NoArgsConstructor;
import lombok.extern.slf4j.Slf4j;

import java.io.IOException;
import java.util.Optional;
import java.util.concurrent.CompletableFuture;

@Slf4j
public class DebugWindow extends Application implements Debugger {

	private Parent root;
	private Scene scene;
	private DebugWindowController controller;

	private static final int WIDTH = 1200;
	private static final int HEIGHT = 700;

	private ObservableList<DebugPeer> peers = FXCollections.observableArrayList();


	@Override
	public void start(Stage stage) {
		try {
			FXMLLoader loader = new FXMLLoader(getClass().getResource("/debugWindow.fxml"));
			root = loader.load();

			controller = loader.getController();
			controller.peerTable.setItems(peers);

		} catch (IOException e) {
			log.error("Could not load debugger window fxml", e);
		}

		setUserAgentStylesheet(STYLESHEET_MODENA);

		scene = new Scene(root, WIDTH, HEIGHT);

		stage.setScene(scene);
		stage.setTitle(String.format("FAF ICE adapter - Debugger - Build: %s", IceAdapter.VERSION));
		stage.setOnCloseRequest(Event::consume);
		stage.show();

//		new Thread(() -> Debug.debug.complete(this)).start();
		log.info("Created debug window.");
		Debug.debug.complete(this);
	}

	@Override
	public void startupComplete() {
		runOnUIThread(() -> {
			controller.versionLabel.setText(String.format("Version: %s", IceAdapter.VERSION));
			controller.userLabel.setText(String.format("User: %s(%d)", IceAdapter.login, IceAdapter.id));
			controller.rpcPortLabel.setText(String.format("RPC_PORT: %d", IceAdapter.RPC_PORT));
			controller.gpgnetPortLabel.setText(String.format("GPGNET_PORT: %d", IceAdapter.GPGNET_PORT));
			controller.lobbyPortLabel.setText(String.format("LOBBY_PORT: %d", IceAdapter.LOBBY_PORT));
		});
	}

	@Override
	public void rpcStarted(CompletableFuture<JJsonPeer> peerFuture) {
		runOnUIThread(() -> {
			controller.rpcServerStatus.setText("RPCServer: started");
		});
		peerFuture.thenAccept(peer -> runOnUIThread(() -> {
			controller.rpcClientStatus.setText(String.format("RPCClient: %s", peer.getSocket().getInetAddress()));
		}));
	}

	@Override
	public void gpgnetStarted() {
		runOnUIThread(() -> {
			controller.gpgnetServerStatus.setText("GPGNetServer: started");
		});
	}

	@Override
	public void gpgnetConnectedDisconnected() {
		runOnUIThread(() -> {
			controller.gpgnetServerStatus.setText(String.format("GPGNetClient: %s", GPGNetServer.isConnected() ? "connected" : "-"));
			gameStateChanged();
		});
	}

	@Override
	public void gameStateChanged() {
		runOnUIThread(() -> {
			controller.gameState.setText(String.format("GameState: %s", GPGNetServer.getGameState().map(GameState::getName).orElse("-")));
		});
	}

	@Override
	public void connectToPeer(int id, String login, boolean localOffer) {
		new Thread(() -> {
			synchronized (peers) {
				DebugPeer peer = new DebugPeer();
				peer.id.set(id);
				peer.login.set(login);
				peer.localOffer.set(localOffer);
				peers.add(peer);//Might callback into jfx
			}
		}).start();
	}

	@Override
	public void disconnectFromPeer(int id) {
		new Thread(() -> {
			synchronized (peers) {
				peers.removeIf(peer -> peer.id.get() == id);//Might callback into jfx
			}
		}).start();
	}

	@Override
	public void peerStateChanged(Peer peer) {
		new Thread(() -> {
			synchronized (peers) {
				peers.stream().filter(p -> p.id.get() == peer.getRemoteId()).forEach(p -> {
					p.connected.set(peer.getIce().isConnected());
					p.state.set(peer.getIce().getIceState().getMessage());
				});
			}
		}).start();
	}

	@Override
	public void peerConnectivityUpdate(Peer peer) {
		new Thread(() -> {
			synchronized (peers) {
				peers.stream().filter(p -> p.id.get() == peer.getRemoteId()).forEach(p -> {
					p.averageRtt.set(Optional.ofNullable(peer.getIce().getConnectivityChecker()).map(PeerConnectivityCheckerModule::getAverageRTT).orElse(0.0f));
					p.lastRecevied.set(Optional.ofNullable(peer.getIce().getConnectivityChecker()).map(PeerConnectivityCheckerModule::getLastPacketReceived).map(last -> System.currentTimeMillis() - last).orElse(0L).intValue());
				});
			}
		}).start();
	}

	private void runOnUIThread(Runnable runnable) {
		if(Platform.isFxApplicationThread()) {
			runnable.run();
		} else {
			Platform.runLater(runnable);
		}
	}

	public static void launchApplication() {
		launch(DebugWindow.class, null);
	}

	public static boolean isJavaFxSupported() {
		try {
			DebugWindow.class.getClassLoader().loadClass("javafx.application.Application");
			return true;
		} catch(ClassNotFoundException e) {
			log.warn("Could not create debug window, no JavaFX found.");
			return false;
		}
	}

	@NoArgsConstructor
	@AllArgsConstructor
	public static class DebugPeer {
		public SimpleIntegerProperty id = new SimpleIntegerProperty(-1);
		public SimpleStringProperty login = new SimpleStringProperty("");
		public SimpleBooleanProperty localOffer = new SimpleBooleanProperty(false);
		public SimpleBooleanProperty connected = new SimpleBooleanProperty(false);
		public SimpleStringProperty state = new SimpleStringProperty("");
		public SimpleDoubleProperty averageRtt = new SimpleDoubleProperty(0.0);
		public SimpleIntegerProperty lastRecevied = new SimpleIntegerProperty(0);
	}
}
