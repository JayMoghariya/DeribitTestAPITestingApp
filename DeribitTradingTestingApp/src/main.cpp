
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <websocketpp/config/asio.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <boost/asio/ssl.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using websocketpp::client;
using websocketpp::config::asio_client;
using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// Corrected configuration for secure WebSocket connections
using tls_client = websocketpp::client<websocketpp::config::asio_tls_client>;

using context_ptr = websocketpp::lib::shared_ptr<boost::asio::ssl::context>;

class WebSocketAPI {
    tls_client ws_client;
    websocketpp::connection_hdl ws_hdl;

public:
    WebSocketAPI() {
        ws_client.init_asio();
        ws_client.set_tls_init_handler(bind(&WebSocketAPI::on_tls_init, this, _1));
        ws_client.set_message_handler(bind(&WebSocketAPI::on_message, this, _1, _2));
        ws_client.set_open_handler(bind(&WebSocketAPI::on_open, this, _1));
    }

    // Initialize SSL context for secure WebSocket connection
    context_ptr on_tls_init(websocketpp::connection_hdl) {
        context_ptr ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);
        try {
            ctx->set_options(boost::asio::ssl::context::default_workarounds |
                             boost::asio::ssl::context::no_sslv2 |
                             boost::asio::ssl::context::no_sslv3 |
                             boost::asio::ssl::context::single_dh_use);
        } catch (std::exception& e) {
            std::cerr << "Error in TLS initialization: " << e.what() << std::endl;
        }
        return ctx;
    }

    // Handle WebSocket message event
    void on_message(websocketpp::connection_hdl hdl, tls_client::message_ptr msg) {
        std::cout << "Received message: " << msg->get_payload() << std::endl;
    }

    // Send subscription message
    void send_subscription(const std::string& channel) {
        json subscription_msg = {
            {"jsonrpc", "2.0"},
            {"method", "public/subscribe"},
            {"params", {
                {"channels", {channel}}
            }},
            {"id", 10}
        };

        ws_client.send(ws_hdl, subscription_msg.dump(), websocketpp::frame::opcode::text);
        std::cout << "Subscription message sent for channel: " << channel << std::endl;
    }

    // Handle WebSocket connection open event
    void on_open(websocketpp::connection_hdl hdl) {
        std::cout << "WebSocket connection established." << std::endl;
        ws_hdl = hdl;  // Store the handle
        std::string instrument;
        std::cout<<"enter the instrument (e.g BTC-PERPETUAL):"<<std::endl;
        std::cin>>instrument;
        std::cout<<"final instrument channelL:: incremental_ticker."<<instrument<<std::endl;
        // Subscribe to the desired channel after the connection is open
        send_subscription("incremental_ticker.BTC-PERPETUAL");
    }

    // WebSocket manager to establish connection
    void ws_manager(const std::string& ws_url) {
        try {
            websocketpp::lib::error_code ec;
            tls_client::connection_ptr con = ws_client.get_connection(ws_url, ec);

            if (ec) {
                std::cout << "Could not create connection: " << ec.message() << std::endl;
                return;
            }

            ws_client.connect(con);

            // Print the message before run() as it blocks the thread
            std::cout << "WebSocket running." << std::endl;

            // Run the event loop (blocking)
            ws_client.run();

        } catch (const websocketpp::exception& e) {
            std::cout << "WebSocket exception: " << e.what() << std::endl;
        }
    }
};

int main() {
    std::string ws_url = "wss://test.deribit.com/ws/api/v2";

    WebSocketAPI api;
    api.ws_manager(ws_url);

    return 0;
}