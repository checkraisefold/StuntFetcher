#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <steam/steam_gameserver.h>

#include <toml++/toml.h>

#include <string>
#include <string_view>
#include <set>
#include <chrono>
#include <thread>

#ifdef OS_UNIX
#include <unistd.h>
#else
#include <windows.h>
#endif

using Server = websocketpp::server<websocketpp::config::asio>;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;
using websocketpp::connection_hdl;

Server webServer;

char getInfoMsg[7] = "StuP\x00\x1C";
double steamTimeout = 0.0;
void* getStuntServer(CSteamID steamId, std::uint32_t& outSize) {
	std::string result = "";
	SteamGameServerNetworking()->SendP2PPacket(steamId, getInfoMsg, 7, k_EP2PSendUnreliable, 0);

	std::uint32_t packetSize;
	double waitingFor = 0;
	while (!(SteamGameServerNetworking()->IsP2PPacketAvailable(&packetSize, 0))) {
		if (waitingFor >= steamTimeout) {
			spdlog::warn("Steam networking timed out!");
			return nullptr;
		}

		waitingFor += 0.05;
		spdlog::debug("Waiting for reply!");
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	void* packetData = malloc(packetSize);
	if (!packetData) {
		spdlog::error("Malloc failed for received packet!");
		return nullptr;
	}

	std::uint32_t bytesRead;
	CSteamID steamRemote;
	if (SteamGameServerNetworking()->ReadP2PPacket(packetData, packetSize, &bytesRead, &steamRemote, 0)) {
		if (steamRemote != steamId) {
			spdlog::error("Received packet SteamID is not same as argument! Did you send too many websock messages?");
			free(packetData);
			return nullptr;
		}
	}
	else {
		spdlog::error("Failed to read P2P packet for {}!", steamId.ConvertToUint64());
	}

	outSize = packetSize;
	return packetData;
}

void messageHandler(Server* serv, connection_hdl hdl, Server::message_ptr msg) {
	spdlog::info("Getting a stunt derby server");

	std::uint32_t buffSize;
	CSteamID targetId = std::stoull(msg->get_payload());
	if (!targetId.IsValid()) {
		spdlog::warn("Got invalid SteamID to websock");
		return;
	}

	auto buff = getStuntServer(targetId, buffSize);
	if (buff) {
		spdlog::info("Replied to Stunt Derby serv info request");
		webServer.send(hdl, buff, buffSize, websocketpp::frame::opcode::binary);
		free(buff);
	}
	else {
		spdlog::warn("Replied to Stunt Derby serv info with failure");
		webServer.send(hdl, "", 1, websocketpp::frame::opcode::text);
	}
}

void initWebSock(std::string port, std::string host) {
	// Set log levels
	webServer.set_access_channels(websocketpp::log::alevel::none);

	// Init ASIO and bind message handler
	webServer.init_asio();
	webServer.set_message_handler(bind(&messageHandler, &webServer, _1, _2));

	// Actually start the server
	webServer.listen(host, port);
	webServer.start_accept();
	webServer.run();
}

int main() { 
	using namespace std::literals;

	auto mainLogger = spdlog::stdout_color_mt("Main");
	spdlog::set_default_logger(mainLogger);

#ifdef DEBUG
	spdlog::set_level(spdlog::level::debug);
#endif

	spdlog::info("Reading config.toml");
	std::string bindHost;
	std::string websockHost;
	std::uint16_t bindPort;
	std::uint16_t websockPort;
	try {
		auto config = toml::parse_file("config.toml");
		bindHost = std::string(config["server"]["host"].value_or("0.0.0.0"sv));
		bindPort = config["server"]["port"].value_or(1337);
		steamTimeout = config["server"]["packetTimeout"].value_or(10.0);
		websockHost = std::string(config["websock"]["host"].value_or("0.0.0.0"sv));
		websockPort = config["websock"]["port"].value_or(1338);
	}
	catch (const toml::parse_error& err) {
		spdlog::error("Toml file parse failed (does the file exist/is formatted properly?)! Exiting! {}", err.what());
		return 0;
	}

	spdlog::info("Initializing Steam gameserver API with port {}", bindPort);
	auto hostAddr = ntohl(inet_addr(bindHost.c_str()));
	SteamGameServer_Init(hostAddr, bindPort, STEAMGAMESERVER_QUERY_PORT_SHARED, eServerModeNoAuthentication, "1.0.0.0");
	SteamGameServer()->LogOnAnonymous();

	spdlog::info("Initializing websock server on port {}", websockPort);
	initWebSock(std::to_string(websockPort), websockHost);
	
	spdlog::info("Shutting down Steam gameserver");
	SteamGameServer_Shutdown();
	return 0;
}