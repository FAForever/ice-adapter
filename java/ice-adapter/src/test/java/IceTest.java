import com.faforever.iceadapter.ice.CandidatePacket;
import com.faforever.iceadapter.ice.CandidatesMessage;
import com.google.gson.Gson;
import lombok.extern.slf4j.Slf4j;
import org.ice4j.Transport;
import org.ice4j.TransportAddress;
import org.ice4j.ice.*;
import org.ice4j.ice.harvest.StunCandidateHarvester;
import org.ice4j.ice.harvest.TurnCandidateHarvester;
import org.ice4j.security.LongTermCredential;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.net.DatagramPacket;
import java.net.InetAddress;
import java.net.Socket;
import java.nio.charset.Charset;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.*;

@Slf4j
public class IceTest {

    private static final String COTURN_HOST = "geosearchef.de";
    private static final String COTURN_KEY = "8T2o1yfSu29vf9cJ3WjHS9Ak6zJCB6qECL2Uxlza";
//    private static final String COTURN_HOST = "vmrbg145.informatik.tu-muenchen.de";
//    private static final String COTURN_KEY = "banana";

    private static final Gson gson = new Gson();


    public static void main(String args[]) throws IOException {
        Scanner scan = new Scanner("false\n12345\n");

        String username = "iceTester" + System.currentTimeMillis() % 10000;
        System.out.printf("Username: %s\n", username);

        int timestamp = (int) (System.currentTimeMillis() / 1000) + 3600 * 24;
        String tokenName = String.format("%s:%s", timestamp, username);
        byte[] secret = null;
        try {
            Mac mac = Mac.getInstance("HmacSHA1");
            mac.init(new SecretKeySpec(Charset.forName("cp1252").encode(COTURN_KEY).array(), "HmacSHA1"));
            secret = mac.doFinal(Charset.forName("cp1252").encode(tokenName).array());

        } catch (NoSuchAlgorithmException | InvalidKeyException e) {
            log.error("Could not build secret key", e);
            System.exit(-1);
        }
        String authToken = Base64.getEncoder().encodeToString(secret);

        Map<String, Object> map = new HashMap<>();
        map.put("credential", authToken);
        map.put("credentialType", "authToken");
        map.put("username", tokenName);



//        int localPort

        TransportAddress[] turnAddresses = {
                new TransportAddress(COTURN_HOST, 3478, Transport.TCP),
                new TransportAddress(COTURN_HOST, 3478, Transport.UDP),
//                String.format("turn:%s?transport=tcp", COTURN_HOST),
//                String.format("turn:%s?transport=udp", COTURN_HOST)
        };

        TransportAddress[] stunAddresses = {
                new TransportAddress(COTURN_HOST, 3478, Transport.UDP),
//                new TransportAddress("stun3.l.google.com", 19302, Transport.UDP)
//                String.format("stun:%s", COTURN_HOST)
        };


        Agent agent = new Agent();

        System.out.printf("Controlling?(true|false)\n");
        agent.setControlling(scan.nextBoolean());

        Arrays.stream(turnAddresses)
                .map(a -> new TurnCandidateHarvester(
                        a,
                        new LongTermCredential(/*String.format("%d:%s", ((System.currentTimeMillis() / 1000) + 3600*24), username)*/(String) map.get("username"), (String) map.get("credential"))))
                .forEach(agent::addCandidateHarvester);
        Arrays.stream(stunAddresses).map(StunCandidateHarvester::new).forEach(agent::addCandidateHarvester);

        System.out.printf("Preferred port?\n");//host port
        int preferredPort = scan.nextInt();

        IceMediaStream mediaStream = agent.createMediaStream("mainStream");
        Component component = agent.createComponent(mediaStream, Transport.UDP, preferredPort, preferredPort, preferredPort + 100);

        //------------------------------------------------------------
        //agent done
        //------------------------------------------------------------

        //print candidates
        //may have to be done for multiple components
        CandidatesMessage localCandidatesMessage = new CandidatesMessage(0, 0/*mocked*/, agent.getLocalPassword(), agent.getLocalUfrag());
        int candidateIDFactory = 0;

        for(LocalCandidate localCandidate : component.getLocalCandidates()) {

            CandidatePacket candidatePacket = new CandidatePacket(
                    localCandidate.getFoundation(),
                    localCandidate.getTransportAddress().getTransport().toString(),
                    localCandidate.getPriority(),
                    localCandidate.getTransportAddress().getHostAddress(),
                    localCandidate.getTransportAddress().getPort(),
                    CandidateType.valueOf(localCandidate.getType().name()),
                    agent.getGeneration(),
                    String.valueOf(candidateIDFactory++)
            );

            if(localCandidate.getRelatedAddress() != null) {
                candidatePacket.setRelAddr(localCandidate.getRelatedAddress().getHostAddress());
                candidatePacket.setRelPort(localCandidate.getRelatedAddress().getPort());
            }

            localCandidatesMessage.getCandidates().add(candidatePacket);
        }

        System.out.printf("------------------------------------\n%s\n------------------------------------\n", gson.toJson(localCandidatesMessage));

        //read candidates
        Socket socket = new Socket("localhost", 49456);
        DataOutputStream out = new DataOutputStream(socket.getOutputStream());
        DataInputStream in = new DataInputStream(socket.getInputStream());
        out.writeUTF(gson.toJson(localCandidatesMessage));
        out.flush();
        CandidatesMessage remoteCandidatesMessage = gson.fromJson(in.readUTF(), CandidatesMessage.class);

        Collections.sort(remoteCandidatesMessage.getCandidates());

        //Set candidates
        mediaStream.setRemotePassword(remoteCandidatesMessage.getPassword());
        mediaStream.setRemoteUfrag(remoteCandidatesMessage.getUfrag());
        for(CandidatePacket remoteCandidatePacket : remoteCandidatesMessage.getCandidates()) {

            if(remoteCandidatePacket.getGeneration() == agent.getGeneration()
                    && remoteCandidatePacket.getIp() != null && remoteCandidatePacket.getPort() > 0) {

                TransportAddress mainAddress = new TransportAddress(remoteCandidatePacket.getIp(), remoteCandidatePacket.getPort(), Transport.parse(remoteCandidatePacket.getProtocol().toLowerCase()));

                RemoteCandidate relatedCandidate = null;
                if(remoteCandidatePacket.getRelAddr() != null && remoteCandidatePacket.getRelPort() > 0) {
                    TransportAddress relatedAddr = new TransportAddress(remoteCandidatePacket.getRelAddr(), remoteCandidatePacket.getRelPort(), Transport.parse(remoteCandidatePacket.getProtocol().toLowerCase()));
                    relatedCandidate = component.findRemoteCandidate(relatedAddr);
                }

                RemoteCandidate remoteCandidate = new RemoteCandidate(
                        mainAddress,
                        component,
                        CandidateType.parse(remoteCandidatePacket.getType().toString()),
                        remoteCandidatePacket.getFoundation(),
                        remoteCandidatePacket.getPriority(),
                        relatedCandidate
                );

                component.addRemoteCandidate(remoteCandidate);
            }
        }

        agent.startConnectivityEstablishment();

        while(agent.getState() != IceProcessingState.TERMINATED) {//TODO include more?
            try { Thread.sleep(20); } catch(InterruptedException e) { e.printStackTrace(); }
        }


        while("".equals("")) {
          component.getSelectedPair().getIceSocketWrapper().send(new DatagramPacket("Aeon is the worst faction on this planet!".getBytes(), 0, 4, InetAddress.getByName(component.getSelectedPair().getRemoteCandidate().getHostAddress().getHostAddress()), component.getSelectedPair().getRemoteCandidate().getHostAddress().getPort()));
          byte[] data = new byte[1024];
          component.getSelectedPair().getIceSocketWrapper().receive(new DatagramPacket(data, data.length));
          System.out.println("Got data: " + new String(data));
          try {Thread.sleep(2500);} catch(InterruptedException e) {e.printStackTrace();}
        }

        agent.free();
    }

}
