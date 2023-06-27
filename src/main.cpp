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
#include <mutex>

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

struct requestData {
	connection_hdl hdl;
	std::chrono::high_resolution_clock::time_point time;
};
std::unordered_map<std::uint64_t, requestData> currentRequests;
std::mutex steamMutex;

char getInfoMsg[7] = "StuP\x00\x1C";
std::atomic<double> steamTimeout = 0.0;
void getStuntServer(CSteamID steamId, connection_hdl hdl) {
	using namespace std::chrono;
	std::lock_guard<std::mutex> lockSteam(steamMutex);

	requestData data{hdl, high_resolution_clock::now()};
	currentRequests.emplace(steamId.ConvertToUint64(), data);
	SteamGameServerNetworking()->SendP2PPacket(steamId, getInfoMsg, 7, k_EP2PSendUnreliable, 0);
}

void steamPacketThread() {
	std::uint32_t packetSize;
	while (true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		steamMutex.lock();
		for (auto iter = currentRequests.cbegin(); iter != currentRequests.cend();) {
			using namespace std::chrono;
			high_resolution_clock::time_point current = high_resolution_clock::now();
			duration<double> time_span = duration_cast<duration<double>>(current - iter->second.time);

			if (time_span.count() >= steamTimeout) {
				spdlog::warn("Serv info request timed out for {}!", iter->first);
				iter = currentRequests.erase(iter);
			}
			else {
				++iter;
			}
		}
		steamMutex.unlock();

		if (!(SteamGameServerNetworking()->IsP2PPacketAvailable(&packetSize, 0))) {
			continue;
		}

		void* packetData = malloc(packetSize + 8);
		if (!packetData) {
			spdlog::error("Malloc failed for received packet!");
			continue;
		}

		std::uint32_t bytesRead;
		CSteamID steamRemote;
		std::unordered_map<std::uint64_t, requestData>::const_iterator data;
		if (SteamGameServerNetworking()->ReadP2PPacket(packetData, packetSize, &bytesRead, &steamRemote, 0)) {
			steamMutex.lock();
			if (currentRequests.find(steamRemote.ConvertToUint64()) == currentRequests.end()) {
				spdlog::warn("Received packet SteamID is not in list! Ignoring packet!");
				free(packetData);

				steamMutex.unlock();
				continue;
			}
			data = currentRequests.find(steamRemote.ConvertToUint64());
			steamMutex.unlock();
		}
		else {
			spdlog::error("Failed to read P2P packet for {}!", steamRemote.ConvertToUint64());
			continue;
		}

		std::memcpy(reinterpret_cast<char*>(packetData) + packetSize, &steamRemote, 8);

		steamMutex.lock();
		webServer.send(data->second.hdl, packetData, packetSize + 8, websocketpp::frame::opcode::binary);
		currentRequests.erase(data);
		steamMutex.unlock();
	}
}

void messageHandler(Server* serv, connection_hdl hdl, Server::message_ptr msg) {
	spdlog::info("Getting a stunt derby server");

	std::uint32_t buffSize;
	CSteamID targetId = std::stoull(msg->get_payload());
	if (!targetId.IsValid()) {
		spdlog::warn("Got invalid SteamID to websock");
		webServer.send(hdl, "", 1, websocketpp::frame::opcode::text);
		return;
	}

	getStuntServer(targetId, hdl);
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

	spdlog::info("Initializing Steam packet reader thread");
	std::thread packetThread(steamPacketThread);
	packetThread.detach();

	spdlog::info("Initializing websock server on port {}", websockPort);
	initWebSock(std::to_string(websockPort), websockHost);
	
	spdlog::info("Shutting down Steam gameserver");
	SteamGameServer_Shutdown();
	return 0;
}