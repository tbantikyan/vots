#include <csignal>

#include "logging/logger.hpp"
#include "market_data/market_data_consumer.hpp"
#include "order_gateway/gateway_client.hpp"
#include "trading_engine/trading_engine.hpp"

// Main components.
common::Logger *logger = nullptr;
trading::TradingEngine *trading_engine = nullptr;
trading::MarketDataConsumer *market_data_consumer = nullptr;
trading::GatewayClient *order_gateway = nullptr;

// ./trading_main CLIENT_ID ALGO_TYPE [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2
// MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...
auto main(int argc, char **argv) -> int {
    if (argc < 3) {
        FATAL(
            "USAGE trading_main CLIENT_ID ALGO_TYPE [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 "
            "THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...");
    }

    const common::ClientId client_id = atoi(argv[1]);
    srand(client_id);

    const auto algo_type = common::StringToAlgoType(argv[2]);

    logger = new common::Logger("trading_main_" + std::to_string(client_id) + ".log");

    const int sleep_time = 20 * 1000;

    // The lock free queues to facilitate communication between order gateway <-> trade engine and market data consumer
    // -> trade engine.
    exchange::ClientRequestLFQueue client_requests(common::ME_MAX_CLIENT_UPDATES);
    exchange::ClientResponseLFQueue client_responses(common::ME_MAX_CLIENT_UPDATES);
    exchange::MEMarketUpdateLFQueue market_updates(common::ME_MAX_MARKET_UPDATES);

    std::string time_str;

    common::TradeEngineCfgMap ticker_cfg;

    // Parse and initialize the TradeEngineCfgHashMap above from the command line arguments.
    // [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2]
    // ...
    size_t next_ticker_id = 0;
    for (int i = 3; i < argc; i += 5, ++next_ticker_id) {
        ticker_cfg.at(next_ticker_id) = {
            .clip_ = static_cast<common::Qty>(std::atoi(argv[i])),
            .threshold_ = std::atof(argv[i + 1]),
            .risk_cfg_ = {.max_order_size_ = static_cast<common::Qty>(std::atoi(argv[i + 2])),
                          .max_position_ = static_cast<common::Qty>(std::atoi(argv[i + 3])),
                          .max_loss_ = std::atof(argv[i + 4])}};
    }

    logger->Log("%:% %() % Starting Trade Engine...\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str));
    trading_engine = new trading::TradingEngine(client_id, algo_type, ticker_cfg, &client_requests, &client_responses,
                                              &market_updates);
    trading_engine->Start();

    const std::string order_gw_ip = "127.0.0.1";
    const std::string order_gw_iface = "lo";
    const int order_gw_port = 12345;

    logger->Log("%:% %() % Starting Order Gateway...\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str));
    order_gateway = new trading::GatewayClient(client_id, &client_requests, &client_responses, order_gw_ip,
                                               order_gw_iface, order_gw_port);
    order_gateway->Start();

    const std::string mkt_data_iface = "lo";
    const std::string snapshot_ip = "233.252.14.1";
    const int snapshot_port = 20000;
    const std::string incremental_ip = "233.252.14.3";
    const int incremental_port = 20001;

    logger->Log("%:% %() % Starting Market Data Consumer...\n", __FILE__, __LINE__, __FUNCTION__,
                common::GetCurrentTimeStr(&time_str));
    market_data_consumer = new trading::MarketDataConsumer(client_id, &market_updates, mkt_data_iface, snapshot_ip,
                                                           snapshot_port, incremental_ip, incremental_port);
    market_data_consumer->Start();

    usleep(10 * 1000 * 1000);

    trading_engine->InitLastEventTime();

    // For the random trading algorithm, we simply implement it here instead of creating a new trading algorithm which
    // is another possibility. Generate random orders with random attributes and randomly cancel some of them.
    if (algo_type == common::AlgoType::RANDOM) {
        common::OrderId order_id = client_id * 1000;
        std::vector<exchange::MEClientRequest> client_requests_vec;
        std::array<common::Price, common::ME_MAX_TICKERS> ticker_base_price;
        for (size_t i = 0; i < common::ME_MAX_TICKERS; ++i) {
            ticker_base_price[i] = (rand() % 100) + 100;
        }
        for (size_t i = 0; i < 10000; ++i) {
            const common::TickerId ticker_id = rand() % common::ME_MAX_TICKERS;
            const common::Price price = ticker_base_price[ticker_id] + (rand() % 10) + 1;
            const common::Qty qty = 1 + (rand() % 100) + 1;
            const common::Side side = (((rand() % 2) != 0) ? common::Side::BUY : common::Side::SELL);

            exchange::MEClientRequest new_request{.type_ = exchange::ClientRequestType::NEW,
                                                  .client_id_ = client_id,
                                                  .ticker_id_ = ticker_id,
                                                  .order_id_ = order_id++,
                                                  .side_ = side,
                                                  .price_ = price,
                                                  .qty_ = qty};
            trading_engine->SendClientRequest(&new_request);
            usleep(sleep_time);

            client_requests_vec.push_back(new_request);
            const auto cxl_index = rand() % client_requests_vec.size();
            auto cxl_request = client_requests_vec[cxl_index];
            cxl_request.type_ = exchange::ClientRequestType::CANCEL;
            trading_engine->SendClientRequest(&cxl_request);
            usleep(sleep_time);

            if (trading_engine->SilentSeconds() >= 60) {
                logger->Log("%:% %() % Stopping early because been silent for % seconds...\n", __FILE__, __LINE__,
                            __FUNCTION__, common::GetCurrentTimeStr(&time_str), trading_engine->SilentSeconds());

                break;
            }
        }
    }

    while (trading_engine->SilentSeconds() < 60) {
        logger->Log("%:% %() % Waiting till no activity, been silent for % seconds...\n", __FILE__, __LINE__,
                    __FUNCTION__, common::GetCurrentTimeStr(&time_str), trading_engine->SilentSeconds());

        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(30s);
    }

    trading_engine->Stop();
    market_data_consumer->Stop();
    order_gateway->Stop();

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10s);

    delete logger;
    logger = nullptr;
    delete trading_engine;
    trading_engine = nullptr;
    delete market_data_consumer;
    market_data_consumer = nullptr;
    delete order_gateway;
    order_gateway = nullptr;

    std::this_thread::sleep_for(10s);

    exit(EXIT_SUCCESS);
}
