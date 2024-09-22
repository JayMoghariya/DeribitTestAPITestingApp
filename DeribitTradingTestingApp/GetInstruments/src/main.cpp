#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/json.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <string>

namespace asio = boost::asio;
namespace beast = boost::beast;     // from <boost/beast.hpp>
namespace http = beast::http;       // from <boost/beast/http.hpp>
namespace net = boost::asio;        // from <boost/asio.hpp>
namespace ssl = net::ssl;           // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;           // from <boost/asio/ip/tcp.hpp>


// Function to asynchronously fetch data from the API
void fetch_data(asio::io_context& ioc, ssl::context& ctx, const std::string& host, const std::string& target) {
    try {
        // Resolver to find the IP of the host
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host, "https");

        // SSL socket setup
        beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx);

        // Establish a TCP connection
        beast::get_lowest_layer(stream).connect(results);

        // Perform the SSL handshake
        stream.handshake(ssl::stream_base::client);

        // Setup HTTP GET request
        http::request<http::string_body> req{http::verb::get, target, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // Send the request
        http::write(stream, req);

        // Buffer to hold the response
        beast::flat_buffer buffer;
        http::response<http::string_body> res;

        // Receive the response
        http::read(stream, buffer, res);

        // Output status code and content
        std::cout << "Response Status Code: " << res.result_int() << std::endl;
        std::cout << "Response Content: " << res.body() << std::endl;

        // Gracefully close the stream
        beast::error_code ec;
        stream.shutdown(ec);
        if (ec == asio::error::eof) {
            ec.assign(0, ec.category());
        }
        if (ec) {
            throw beast::system_error{ec};
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

int main() {
    // Logging
    std::cout << "Fetching data from Deribit..." << std::endl;

    // Connection details
    const std::string host = "test.deribit.com";
    const std::string target = "/api/v2/public/get_instruments?currency=BTC";

    // I/O context
    asio::io_context ioc;

    // SSL context
    ssl::context ctx(ssl::context::tlsv12_client);

    // Fetch the data asynchronously
    fetch_data(ioc, ctx, host, target);

    return 0;
}
