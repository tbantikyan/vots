#include <csignal>

#include "matching_engine/matching_engine.hpp"

common::Logger *logger = nullptr;
exchange::MatchingEngine *matching_engine = nullptr;

void SignalHandler(int /*unused*/) {
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10s);

    delete logger;
    logger = nullptr;
    delete matching_engine;
    matching_engine = nullptr;

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

    while (true) {
        logger->Log("%:% %() % Sleeping for a few milliseconds..\n", __FILE__, __LINE__, __FUNCTION__,
                    common::GetCurrentTimeStr(&time_str));
        usleep(sleep_time * 1000);
    }
}
