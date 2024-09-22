#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <curl/curl.h>
#include "config.hpp"

using json = nlohmann::json;
using websocketpp::client;
using websocketpp::config::asio_client;
using websocketpp::connection_hdl;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// Callback function to handle the data received from cURL
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

class DeribitAPI {
    std::string api_key;
    std::string api_secret;
    std::string access_token;
    client<asio_client> ws_client;
    websocketpp::connection_hdl ws_hdl;
    std::string refresh_token;
    std::chrono::steady_clock::time_point refresh_token_expiry_time;

public:
    DeribitAPI(const std::string& key, const std::string& secret) : api_key(key), api_secret(secret) {}

    void authenticate() {
        CURL *curl;
        CURLcode res;
        std::string auth_url = "https://www.deribit.com/api/v2/oauth2/token";
        std::string post_fields = "grant_type=client_credentials&client_id=" + api_key + "&client_secret=" + api_secret;
        std::string response_body;

        curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, auth_url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

            // Set the content type to application/x-www-form-urlencoded
            struct curl_slist *headers = NULL;
            headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            res = curl_easy_perform(curl);
            std::cout<<res<<"\n";
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                try {
                    json j = json::parse(response_body);
                    if (j.contains("result") && j["result"].contains("access_token")) {
                        access_token = j["result"]["access_token"].get<std::string>();
                        std::cout << "Access Token: " << access_token << std::endl;
                    } else {
                        std::cerr << "Failed to retrieve access token: " << j.dump() << std::endl;
                    }
                } catch (json::parse_error& e) {
                    std::cerr << "JSON parse error: " << e.what() << std::endl;
                }
            }

            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
        }
    }

    void on_message(websocketpp::connection_hdl hdl, client<asio_client>::message_ptr msg) {
        std::cout << "Received: " << msg->get_payload() << std::endl;
        try {
            json j = json::parse(msg->get_payload());
            if (j.contains("id")) {
                if (j["id"] == 9929) {
                    if (refresh_token.empty()) {
                        std::cout << "Successfully authenticated WebSocket Connection" << std::endl;
                    } else {
                        std::cout << "Successfully refreshed the authentication of the WebSocket Connection" << std::endl;
                    }

                    refresh_token = j["result"]["refresh_token"].get<std::string>();
                    int expires_in = j.contains("testnet") ? 300 : j["result"]["expires_in"].get<int>() - 240;
                    refresh_token_expiry_time = std::chrono::steady_clock::now() + std::chrono::seconds(expires_in);
                } else if (j["id"] == 8212) {
                    // Avoid logging Heartbeat messages
                    return;
                }
            } else if (j.contains("method") && j["method"] == "heartbeat") {
                // Respond to Heartbeat Message
                send_heartbeat_response();
            }
        } catch (json::parse_error& e) {
            std::cerr << "JSON parse error: " << e.what() << std::endl;
        }
    }

    void send_heartbeat_response() {
        json heartbeat_response_msg = {
            {"jsonrpc", "2.0"},
            {"id", 8212},
            {"method", "public/test"},
            {"params", {}}
        };

        ws_client.send(ws_hdl, heartbeat_response_msg.dump(), websocketpp::frame::opcode::text);
    }

    void send_subscription(const std::string& ws_channel) {
        json subscription_msg = {
            {"jsonrpc", "2.0"},
            {"method", "public/subscribe"},
            {"id", 42},
            {"params", {
                {"channels", {ws_channel}}
            }}
        };

        ws_client.send(ws_hdl, subscription_msg.dump(), websocketpp::frame::opcode::text);
    }

    void ws_manager(const std::string& ws_url, const std::string& instrument_name) {
        try {
            ws_client.init_asio();
            ws_client.set_message_handler(bind(&DeribitAPI::on_message, this, _1, _2));

            websocketpp::lib::error_code ec;
            client<asio_client>::connection_ptr con = ws_client.get_connection(ws_url, ec);

            if (ec) {
                std::cout << "Could not create connection: " << ec.message() << std::endl;
                return;
            }

            ws_client.connect(con);
            ws_hdl = con->get_handle();
            ws_client.run();

            authenticate();

            // Establish Heartbeat
            json heartbeat_msg = {
                {"jsonrpc", "2.0"},
                {"id", 9098},
                {"method", "public/set_heartbeat"},
                {"params", {
                    {"interval", 10}
                }}
            };
            ws_client.send(ws_hdl, heartbeat_msg.dump(), websocketpp::frame::opcode::text);

            // Subscribe to the specified WebSocket Channel
            send_subscription("orderBook." + instrument_name);

            // Start Authentication Refresh Task
            while (true) {
                std::this_thread::sleep_for(std::chrono::seconds(150));
                if (std::chrono::steady_clock::now() > refresh_token_expiry_time) {
                    json refresh_msg = {
                        {"jsonrpc", "2.0"},
                        {"id", 9929},
                        {"method", "public/auth"},
                        {"params", {
                            {"grant_type", "refresh_token"},
                            {"refresh_token", refresh_token}
                        }}
                    };

                    ws_client.send(ws_hdl, refresh_msg.dump(), websocketpp::frame::opcode::text);
                }
            }
        } catch (const websocketpp::exception &e) {
            std::cout << "WebSocket exception: " << e.what() << std::endl;
        }
    }
};

int mainold() {
    // Replace with actual API credentials
    std::string api_key = get_api_key();
    std::string api_secret = get_api_secret();
    std::string ws_url = "wss://test.deribit.com/ws/api/v2"; // Change to live URL if needed
    std::string instrument_name = "BTC-PERPETUAL"; // Example instrument name

    DeribitAPI api(api_key, api_secret);
    api.ws_manager(ws_url, instrument_name);

    return 0;
}
