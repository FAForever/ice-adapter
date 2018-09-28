package com.faforever.iceadapter.debug;

import lombok.extern.slf4j.Slf4j;

import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ExecutionException;

@Slf4j
public class Debug {
	static CompletableFuture<Debugger> debug = new CompletableFuture<>();

	static {
		if(DebugWindow.isJavaFxSupported()) {
			new Thread(DebugWindow::launchApplication).start();//Completes future once application started
//			debug.join();
		} else {
			debug.complete(new NoDebugger());
		}
	}


	public static Debugger debug() {
		try {
			return debug.get();
		} catch (InterruptedException | ExecutionException e) {
			log.error("Could not get debugger", e);
			return new NoDebugger();
		}
	}
}
