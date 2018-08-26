package util;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.function.Consumer;

import static com.github.nocatch.NoCatch.noCatch;

public class OsUtil {

  public static void gobbleLines(InputStream stream, Consumer<String> lineConsumer) {
    Thread thread = new Thread(() -> noCatch(() -> {
      try (BufferedReader bufferedReader = new BufferedReader(new InputStreamReader(stream))) {
        String line;
        while ((line = bufferedReader.readLine()) != null) {
          lineConsumer.accept(line);
        }
      }
    }));
    thread.setDaemon(true);
    thread.start();
  }

}
