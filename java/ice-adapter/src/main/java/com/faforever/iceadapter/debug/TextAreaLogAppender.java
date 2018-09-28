package com.faforever.iceadapter.debug;

import ch.qos.logback.core.OutputStreamAppender;

import java.io.FilterOutputStream;
import java.io.OutputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

public class TextAreaLogAppender<E> extends OutputStreamAppender<E> {

	private TextAreaOutputStream textAreaOutputStream = new TextAreaOutputStream();

	public TextAreaLogAppender() {
	}

	public void setTextArea(Object textArea) {
		textAreaOutputStream.setTextArea(textArea);
	}

	@Override
	public void start() {
		setOutputStream(new FilterOutputStream(textAreaOutputStream));
		super.start();
	}


	private static class TextAreaOutputStream extends OutputStream {

		private Object textArea;
		private Method textAreaAppendMethod;
		private List<Integer> buffer = new ArrayList<>();

		public TextAreaOutputStream() {

		}

		@Override
		public void write(int b) {
			if(Debug.debug.isDone() && !(Debug.debug().getClass().equals("com.faforever.iceadapter.debug.DebugWindow"))) {
				if(! buffer.isEmpty()) {
					buffer.clear();
				}
				return;
			}

			if(textArea != null) {
				while(! buffer.isEmpty()) {
					appendText(String.valueOf((char) buffer.remove(0).intValue()));
				}
				appendText(String.valueOf((char) b));
			} else {
				buffer.add(b);
			}
		}

		private void appendText(String text) {
			try {
				textAreaAppendMethod.invoke(textArea, text);
			} catch (IllegalAccessException | InvocationTargetException e) {
				e.printStackTrace();
				throw new RuntimeException("Could not append log to textArea");
			}
		}

		public void setTextArea(Object textArea) {
			if(! textArea.getClass().getCanonicalName().equals("javafx.scene.control.TextArea")) {
				throw new RuntimeException(String.format("Object is of class %s, expected javafx.scene.control.TextArea", textArea.getClass().getCanonicalName()));
			}
			this.textArea = textArea;
			try {
				this.textAreaAppendMethod = textArea.getClass().getMethod("appendText", String.class);
			} catch (NoSuchMethodException e) {
				e.printStackTrace();
				throw new RuntimeException("Could not instantiate TextAreaLogAppender, could not find TextArea appendText method");
			}
		}
	}

}
