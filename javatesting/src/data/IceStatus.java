package data;

import lombok.Data;

@Data
public class IceStatus {
	private String version;
	private int ice_servers_size;
	private int lobby_port;
	private String init_mode;
	private IceOptions options;
	private IceGPGNetState gpgpnet;
	private IceRelay relays[];

	@Data
	public class IceOptions {
		private int player_id;
		private String player_login;
		private int rpc_port;
		private int gpgnet_port;
	}

	@Data
	public class IceGPGNetState {
		private int local_port;
		private boolean connected;
		private String game_state;
		private String task_string;
	}

	@Data
	public class IceRelay {

		private int remote_player_id;
		private String remote_player_login;
		private int local_game_udp_port;
		private IceRelayICEState ice;

		@Data
		public class IceRelayICEState {
			private boolean offerer;
			private String state;
			private String gathering_state;
			private String datachannel_state;
			private boolean connected;
			private String loc_cand_addr;
			private String rem_cand_addr;
			private String loc_cand_type;
			private String rem_cand_type;
			private Double time_to_connected;
		}
	}
}
