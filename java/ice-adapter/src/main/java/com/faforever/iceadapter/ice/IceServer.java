package com.faforever.iceadapter.ice;

import lombok.Data;
import org.ice4j.TransportAddress;

import java.util.ArrayList;
import java.util.List;

@Data
public class IceServer {
  private List<TransportAddress> stunAddresses = new ArrayList<>();
  private List<TransportAddress> turnAddresses = new ArrayList<>();
  private String turnUsername = "";
  private String turnCredential = "";
}
