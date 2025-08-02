#include <csignal>

#include "market_data/market_data_publisher.hpp"
#include "matching_engine/matching_engine.hpp"
#include "order_server/order_server.hpp"

common::Logger *logger = nullptr;
exchange::MatchingEngine *matching_engine = nullptr;
exchange::MarketDataPublisher *market_data_publisher = nullptr;
exchange::OrderServer *order_server = nullptr;

void SignalHandler(int /*unused*/) {
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10s);

    delete logger;
    logger = nullptr;
    delete matching_engine;
    matching_engine = nullptr;
    delete market_data_publisher;
    market_data_publisher = nullptr;
    delete order_server;
    order_server = nullptr;

    std::this_thread::sleep_for(10s);

    exit(EXIT_SUCCESS);
}

auto main(int /*unused*/, char ** /*unused*/) -> int {
    logger = new common::Logger("exchange_main.log");

    std::signal(SIGINT, SignalHandler);

    const int sleep_time = 100 * 1000;

    exchange::ClientRequestLFQueue client_requests(common::ME_MAX_CLIENT_UPDATES);
    exchange::ClientResponseLFQueue client_responses(common::ME_MAX_CLIENT_UPDATES);
    exchange::MEMarketUpdateLFQueue market_updates(common::ME_MAX_MARKET_UPDATES);

    std::string time_str;

    logger->Log("%:% %() % Starting Matching Engine...\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str));
    matching_engine = new exchange::MatchingEngine(&client_requests, &client_responses, &market_updates);
    matching_engine->Start();

    const std::string mkt_pub_iface = "lo";
    const std::string snap_pub_ip = "233.252.14.1";
    const std::string inc_pub_ip = "233.252.14.3";
    const int snap_pub_port = 20000;
    const int inc_pub_port = 20001;

    logger->Log("%:% %() % Starting Market Data Publisher...\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str));
    market_data_publisher = new exchange::MarketDataPublisher(&market_updates, mkt_pub_iface, snap_pub_ip,
                                                              snap_pub_port, inc_pub_ip, inc_pub_port);
    market_data_publisher->Start();

    const std::string order_gw_iface = "lo";
    const int order_gw_port = 12345;

    logger->Log("%:% %() % Starting Order Server...\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str));
    order_server = new exchange::OrderServer(&client_requests, &client_responses, order_gw_iface, order_gw_port);
    order_server->Start();

    while (true) {
        logger->Log("%:% %() % Sleeping for a few milliseconds..\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str));
        usleep(sleep_time * 1000);
    }
}
