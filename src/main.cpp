#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#ifdef OS_UNIX
#include <unistd.h>
#else
#include <windows.h>
#endif

using Client = websocketpp::client<websocketpp::config::asio_client>;

int main() {
	auto mainLogger = spdlog::stdout_color_mt("Main");
	spdlog::set_default_logger(mainLogger);
	spdlog::info("Testing 123");
	
	return 0;
}