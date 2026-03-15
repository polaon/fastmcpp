#include "fastmcpp/server/http_server.hpp"
#include "fastmcpp/server/server.hpp"
#include "fastmcpp/server/sse_server.hpp"
#include "fastmcpp/server/streamable_http_server.hpp"

#include <cassert>
#include <chrono>
#include <httplib.h>
#include <iostream>
#include <thread>

using namespace fastmcpp;

void test_http_server_port_zero()
{
    std::cout << "Test: HTTP server with port 0...\n";

    auto srv = std::make_shared<server::Server>();
    srv->route("ping", [](const Json&) { return Json{{"pong", true}}; });

    server::HttpServerWrapper http{srv, "127.0.0.1", 0};
    assert(http.port() == 0); // Before start, port is 0

    if (!http.start())
    {
        std::cerr << "  [FAIL] Failed to start HTTP server with port 0\n";
        std::abort();
    }

    assert(http.port() > 0); // After start, port is resolved
    std::cout << "  Bound to port: " << http.port() << "\n";

    // Verify the server is actually reachable on the assigned port
    httplib::Client client("127.0.0.1", http.port());
    client.set_connection_timeout(std::chrono::seconds(2));
    auto res = client.Post("/ping", R"({"hello":"world"})", "application/json");
    assert(res);
    assert(res->status == 200);

    auto body = Json::parse(res->body);
    assert(body["pong"] == true);

    http.stop();
    std::cout << "  [PASS] HTTP server with port 0\n";
}

void test_sse_server_port_zero()
{
    std::cout << "Test: SSE server with port 0...\n";

    auto handler = [](const Json& req) -> Json
    { return Json{{"jsonrpc", "2.0"}, {"id", req["id"]}, {"result", Json::object()}}; };

    server::SseServerWrapper sse{handler, "127.0.0.1", 0};
    assert(sse.port() == 0);

    if (!sse.start())
    {
        std::cerr << "  [FAIL] Failed to start SSE server with port 0\n";
        std::abort();
    }

    assert(sse.port() > 0);
    std::cout << "  Bound to port: " << sse.port() << "\n";

    // Verify the SSE endpoint is reachable
    bool received_data = false;
    httplib::Client client("127.0.0.1", sse.port());
    client.set_connection_timeout(std::chrono::seconds(2));
    client.set_read_timeout(std::chrono::seconds(2));
    client.Get(sse.sse_path().c_str(),
               [&](const char*, size_t)
               {
                   received_data = true;
                   return false; // Cancel after first chunk
               });
    assert(received_data);

    sse.stop();
    std::cout << "  [PASS] SSE server with port 0\n";
}

void test_streamable_http_server_port_zero()
{
    std::cout << "Test: Streamable HTTP server with port 0...\n";

    auto handler = [](const Json& req) -> Json
    { return Json{{"jsonrpc", "2.0"}, {"id", req["id"]}, {"result", Json::object()}}; };

    server::StreamableHttpServerWrapper streamable{handler, "127.0.0.1", 0};
    assert(streamable.port() == 0);

    if (!streamable.start())
    {
        std::cerr << "  [FAIL] Failed to start Streamable HTTP server with port 0\n";
        std::abort();
    }

    assert(streamable.port() > 0);
    std::cout << "  Bound to port: " << streamable.port() << "\n";

    // Verify the server is reachable — GET returns 405 (Method Not Allowed)
    httplib::Client client("127.0.0.1", streamable.port());
    client.set_connection_timeout(std::chrono::seconds(2));
    auto res = client.Get(streamable.mcp_path().c_str());
    assert(res);
    assert(res->status == 405);

    streamable.stop();
    std::cout << "  [PASS] Streamable HTTP server with port 0\n";
}

void test_two_port_zero_servers()
{
    std::cout << "Test: Two servers both using port 0...\n";

    auto srv1 = std::make_shared<server::Server>();
    srv1->route("id", [](const Json&) { return Json{{"server", 1}}; });

    auto srv2 = std::make_shared<server::Server>();
    srv2->route("id", [](const Json&) { return Json{{"server", 2}}; });

    server::HttpServerWrapper http1{srv1, "127.0.0.1", 0};
    server::HttpServerWrapper http2{srv2, "127.0.0.1", 0};

    assert(http1.start());
    assert(http2.start());

    // Both should get different ports
    assert(http1.port() > 0);
    assert(http2.port() > 0);
    assert(http1.port() != http2.port());
    std::cout << "  Server 1 port: " << http1.port() << ", Server 2 port: " << http2.port() << "\n";

    // Both should be independently reachable
    httplib::Client c1("127.0.0.1", http1.port());
    auto r1 = c1.Post("/id", "{}", "application/json");
    assert(r1 && Json::parse(r1->body)["server"] == 1);

    httplib::Client c2("127.0.0.1", http2.port());
    auto r2 = c2.Post("/id", "{}", "application/json");
    assert(r2 && Json::parse(r2->body)["server"] == 2);

    http1.stop();
    http2.stop();
    std::cout << "  [PASS] Two servers both using port 0\n";
}

int main()
{
    std::cout << "\n=== Port 0 (ephemeral port) binding tests ===\n\n";

    test_http_server_port_zero();
    test_sse_server_port_zero();
    test_streamable_http_server_port_zero();
    test_two_port_zero_servers();

    std::cout << "\n[OK] All port 0 binding tests passed!\n";
    return 0;
}
