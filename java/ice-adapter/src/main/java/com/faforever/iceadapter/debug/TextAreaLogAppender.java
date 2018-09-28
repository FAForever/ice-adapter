package com.faforever.iceadapter.debug;

import ch.qos.logback.core.OutputStreamAppender;
import javafx.scene.control.TextArea;

import java.io.FilterOutputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

public class TextAreaLogAppender<E> extends OutputStreamAppender<E> {

	private TextAreaOutputStream textAreaOutputStream = new TextAreaOutputStream();

	public TextAreaLogAppender() {
	}

	public void setTextArea(TextArea textArea) {
		textAreaOutputStream.setTextArea(textArea);
	}

	@Override
	public void start() {
		setOutputStream(new FilterOutputStream(textAreaOutputStream));
		super.start();
	}


	private static class TextAreaOutputStream extends OutputStream {

		private TextArea textArea;
		private List<Integer> buffer = new ArrayList<>();

		public TextAreaOutputStream() {

		}

		@Override
		public void write(int b) {
			if(Debug.debug.isDone() && !(Debug.debug() instanceof DebugWindow)) {
				if(! buffer.isEmpty()) {
					buffer.clear();
				}
				return;
			}

			if(textArea != null) {
				while(! buffer.isEmpty()) {
					textArea.appendText(String.valueOf((char) buffer.remove(0).intValue()));
				}
				textArea.appendText(String.valueOf((char) b));
			} else {
				buffer.add(b);
			}
		}

		public void setTextArea(TextArea textArea) {
			this.textArea = textArea;
		}
	}

}
