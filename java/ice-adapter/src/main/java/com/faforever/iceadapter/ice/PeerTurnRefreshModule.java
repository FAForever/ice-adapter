package com.faforever.iceadapter.ice;

import lombok.Getter;
import lombok.extern.slf4j.Slf4j;
import org.ice4j.ice.RelayedCandidate;
import org.ice4j.ice.harvest.StunCandidateHarvest;
import org.ice4j.ice.harvest.TurnCandidateHarvest;
import org.ice4j.message.MessageFactory;
import org.ice4j.message.Request;
import org.ice4j.stack.TransactionID;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.time.Duration;

/**
 * Sends continuous refresh requests to the turn server
 */
@Slf4j
public class PeerTurnRefreshModule {

	private static final int REFRESH_INTERVAL = (int) Duration.ofMinutes(2).toMillis();

	private static Field harvestField;
	private static Method sendRequestMethod;
	static {
		try {
			harvestField = RelayedCandidate.class.getDeclaredField("turnCandidateHarvest");
			harvestField.setAccessible(true);
			sendRequestMethod = StunCandidateHarvest.class.getDeclaredMethod("sendRequest", Request.class, boolean.class, TransactionID.class);
			sendRequestMethod.setAccessible(true);
		} catch(NoSuchFieldException | NoSuchMethodException e) {
			log.error("Could not initialize harvestField for turn refreshing.", e);
		}
	}

	@Getter private final PeerIceModule ice;
	@Getter private final RelayedCandidate candidate;

	private TurnCandidateHarvest harvest = null;

	private Thread refreshThread;
	private volatile boolean running = true;

	public PeerTurnRefreshModule(PeerIceModule ice, RelayedCandidate candidate) {
		this.ice = ice;
		this.candidate = candidate;

		try {
			harvest = (TurnCandidateHarvest) harvestField.get(candidate);
		} catch (IllegalAccessException e) {
			log.error("Could not get harvest from candidate.", e);
		}

		if(harvest != null) {
			refreshThread = new Thread(this::refreshThread);
			refreshThread.start();

			log.info("Started turn refresh module for peer {}", ice.getPeer().getRemoteLogin());
		}
	}

	private void refreshThread() {
		while(running) {

			Request refreshRequest = MessageFactory.createRefreshRequest(600); //Maximum lifetime of turn is 600 seconds (10 minutes), server may limit this even further

			try {
				TransactionID transactionID = (TransactionID) sendRequestMethod.invoke(harvest, refreshRequest, false, null);

				log.info("Sent turn refresh request.");
			} catch (IllegalAccessException | InvocationTargetException e) {
				log.error("Could not send turn refresh request!.", e);
			}

			try {
				Thread.sleep(REFRESH_INTERVAL);
			} catch(InterruptedException e) {
				log.warn("Sleeping refreshThread was interrupted");
			}
		}
	}

	public void close() {
		running = false;
		refreshThread.interrupt();
	}
}
