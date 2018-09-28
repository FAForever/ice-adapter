package com.faforever.iceadapter.debug;

import javafx.fxml.FXML;
import javafx.scene.control.Label;
import javafx.scene.control.TableColumn;
import javafx.scene.control.TableView;
import javafx.scene.control.TextArea;
import javafx.scene.control.cell.PropertyValueFactory;
import javafx.scene.layout.HBox;
import lombok.extern.slf4j.Slf4j;
import org.slf4j.LoggerFactory;


@Slf4j
public class DebugWindowController {
	public HBox genericInfo;
	public Label versionLabel;
	public Label userLabel;
	public Label rpcPortLabel;
	public Label gpgnetPortLabel;
	public Label lobbyPortLabel;
	public TextArea logTextArea;
	public HBox rpcGpgInfo;
	public HBox gpgnetInfo;
	public Label rpcServerStatus;
	public Label rpcClientStatus;
	public HBox rpcInfo;
	public Label gpgnetServerStatus;
	public Label gpgnetClientStatus;
	public Label gameState;
	public TableView peerTable;
	public TableColumn idColumn;
	public TableColumn loginColumn;
	public TableColumn offerColumn;
	public TableColumn connectedColumn;
	public TableColumn stateColumn;
	public TableColumn rttColumn;
	public TableColumn lastColumn;
	public TableColumn localCandColumn;
	public TableColumn remoteCandColumn;

	public DebugWindowController() {

	}

	@FXML
	private void initialize() {
		((TextAreaLogAppender)((ch.qos.logback.classic.Logger)LoggerFactory.getLogger(org.slf4j.Logger.ROOT_LOGGER_NAME)).getAppender("TEXTAREA")).setTextArea(logTextArea);

		logTextArea.textProperty().addListener((observableValue, oldVal, newVal) -> logTextArea.setScrollTop(Double.MAX_VALUE));

		idColumn.setCellValueFactory(new PropertyValueFactory<>("id"));
		loginColumn.setCellValueFactory(new PropertyValueFactory<>("login"));
		offerColumn.setCellValueFactory(new PropertyValueFactory<>("localOffer"));
		connectedColumn.setCellValueFactory(new PropertyValueFactory<>("connected"));
		stateColumn.setCellValueFactory(new PropertyValueFactory<>("state"));
		rttColumn.setCellValueFactory(new PropertyValueFactory<>("averageRtt"));
		lastColumn.setCellValueFactory(new PropertyValueFactory<>("lastReceived"));
		localCandColumn.setCellValueFactory(new PropertyValueFactory<>("localCandidate"));
		remoteCandColumn.setCellValueFactory(new PropertyValueFactory<>("remoteCandidate"));
	}
}
