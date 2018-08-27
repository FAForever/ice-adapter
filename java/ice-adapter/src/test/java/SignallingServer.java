import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.List;

/*
 * Basic signalling server to be used for testing
 */
public class SignallingServer {

    public static void main(String args[]) throws IOException {
        ServerSocket serverSocket = new ServerSocket(49456);
        List<String> messages = new ArrayList<>();
        List<Socket> sockets = new ArrayList<>();


        while(true) {
            Socket socket = serverSocket.accept();
            String s = new DataInputStream(socket.getInputStream()).readUTF();

            if(sockets.size() >= 2) {
                sockets.clear();
                messages.clear();
            }
            System.out.println(s);
            messages.add(s);
            sockets.add(socket);

            if(sockets.size() == 2) {
                DataOutputStream out1 = new DataOutputStream(sockets.get(0).getOutputStream());
                out1.writeUTF(messages.get(1));
                out1.flush();
                DataOutputStream out2 = new DataOutputStream(sockets.get(1).getOutputStream());
                out2.writeUTF(messages.get(0));
                out2.flush();

                sockets.forEach(sock -> {
                    try { sock.close(); } catch(IOException e) {}
                });
                sockets.clear();
                messages.clear();
            }
        }
    }
}
