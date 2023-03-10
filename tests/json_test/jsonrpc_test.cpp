
#include "glaze/ext/jsonrpc.hpp"

#include <boost/ut.hpp>
#include <string>

namespace rpc = glz::rpc;
namespace ut = boost::ut;
using ut::operator""_test;
using ut::operator/;

ut::suite valid_vector_test_cases_server = [] {
   using request_vec = std::vector<int>;

   rpc::server<rpc::server_method_t<"add", request_vec, int>> server;

   server.on<"add">([](request_vec const& vec) -> glz::expected<int, rpc::error> {
      int sum{std::reduce(std::cbegin(vec), std::cend(vec))};
      return sum;
   });

   const std::array valid_requests = {
      std::make_pair(R"({"jsonrpc": "2.0","method": "add", "params": [1, 2, 3],"id": 1})",
                     R"({"jsonrpc": "2.0","result": 6,"id": 1})"),
      std::make_pair(
         // No id is valid
         R"({"jsonrpc": "2.0","method": "add", "params": [1, 2, 3]})", ""),
      std::make_pair(R"({"jsonrpc": "2.0","method": "add", "params": [1, 2, 3],"id": null})", ""),
      std::make_pair(R"({"jsonrpc": "2.0","method": "add", "params": [1, 2, 3],"id": 2.0})",
                     R"({"jsonrpc": "2.0","result": 6, "id": 2})"),
      std::make_pair(R"({"jsonrpc": "2.0","method": "add","params": [1, 2, 3],"id": "some_client_22"})",
                     R"({"jsonrpc": "2.0","result": 6, "id": "some_client_22"})")};
   std::string raw_json;
   std::string resulting_request;

   for (auto const& pair : valid_requests) {
      std::tie(raw_json, resulting_request) = pair;
      std::string stripped{resulting_request};
      stripped.erase(std::remove_if(stripped.begin(), stripped.end(), ::isspace), stripped.end());

      ut::test("response:" + stripped) = [&server, &raw_json, &stripped]() {
         auto response_vec = server.call(raw_json);
         if (stripped.empty()) {  // if no id is supplied expected response should be none
            ut::expect(response_vec.empty());
            return;
         }
         ut::expect(response_vec.size() == 1);
         ut::expect(response_vec.at(0).first == stripped);
      };
   }
};

ut::suite vector_test_cases = [] {
   using request_vec = std::vector<int>;

   rpc::server<rpc::server_method_t<"summer", request_vec, int>> server;
   rpc::client<rpc::client_method_t<"summer", request_vec, int>> client;

   server.on<"summer">([](request_vec const& vec) -> glz::expected<int, rpc::error> {
      int sum{std::reduce(std::cbegin(vec), std::cend(vec))};
      return sum;
   });

   ut::test("sum_result = 6") = [&server, &client] {
      auto request_str{client.request(1, std::vector{1, 2, 3})};
      ut::expect(request_str == R"({"jsonrpc":"2.0","method":"summer","params":[1,2,3],"id":1})");

      server.on<"summer">([](request_vec const& vec) -> glz::expected<int, rpc::error> {
         ut::expect(vec == std::vector{1, 2, 3});
         int sum{std::reduce(std::cbegin(vec), std::cend(vec))};
         return sum;
      });
      auto response_vec = server.call(request_str);
      ut::expect(response_vec.size() == 1);
      ut::expect(response_vec.at(0).first == R"({"jsonrpc":"2.0","result":6,"id":1})");

      client.on<"summer">([](glz::expected<int, rpc::error> value, rpc::jsonrpc_id_type id) -> void {
         ut::expect(value.has_value());
         ut::expect(value.value() == 6);
         ut::expect(std::holds_alternative<std::int64_t>(id));
         ut::expect(std::get<std::int64_t>(id) == std::int64_t{1});
      });
      client.call(response_vec.at(0).first);
   };
};

struct method_foo_params
{
   int foo_a{};
   std::string foo_b{};
};
template <>
struct glz::meta<method_foo_params>
{
   static constexpr auto value{glz::object("foo_a", &method_foo_params::foo_a, "foo_b", &method_foo_params::foo_b)};
};
struct method_foo_result
{
   bool foo_c{};
   std::string foo_d{};
   bool operator==(method_foo_result const& rhs) const noexcept { return foo_c == rhs.foo_c && foo_d == rhs.foo_d; }
};
template <>
struct glz::meta<method_foo_result>
{
   static constexpr auto value{glz::object("foo_c", &method_foo_result::foo_c, "foo_d", &method_foo_result::foo_d)};
};

struct method_bar_params
{
   int bar_a{};
   std::string bar_b{};
};
template <>
struct glz::meta<method_bar_params>
{
   static constexpr auto value{glz::object("bar_a", &method_bar_params::bar_a, "bar_b", &method_bar_params::bar_b)};
};
struct method_bar_result
{
   bool bar_c{};
   std::string bar_d{};
   bool operator==(method_bar_result const& rhs) const noexcept { return bar_c == rhs.bar_c && bar_d == rhs.bar_d; }
};
template <>
struct glz::meta<method_bar_result>
{
   static constexpr auto value{glz::object("bar_c", &method_bar_result::bar_c, "bar_d", &method_bar_result::bar_d)};
};

ut::suite struct_test_cases = [] {
   rpc::server<rpc::server_method_t<"foo", method_foo_params, method_foo_result>,
               rpc::server_method_t<"bar", method_bar_params, method_bar_result>>
      server;
   rpc::client<rpc::client_method_t<"foo", method_foo_params, method_foo_result>,
               rpc::client_method_t<"bar", method_bar_params, method_bar_result>>
      client;

   ut::test("valid foo request") = [&server, &client] {
      auto request_str{client.request("42", method_foo_params{.foo_a = 1337, .foo_b = "hello world"})};
      ut::expect(request_str ==
                 R"({"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"42"})");

      server.on<"foo">([](method_foo_params const& params) -> glz::expected<method_foo_result, rpc::error> {
         ut::expect(params.foo_a == 1337);
         ut::expect(params.foo_b == "hello world");
         return method_foo_result{.foo_c = true, .foo_d = "new world"};
      });

      auto response_vec = server.call(request_str);
      ut::expect(response_vec.size() == 1);
      ut::expect(response_vec.at(0).first ==
                 R"({"jsonrpc":"2.0","result":{"foo_c":true,"foo_d":"new world"},"id":"42"})");

      client.on<"foo">([](glz::expected<method_foo_result, rpc::error> value, rpc::jsonrpc_id_type id) -> void {
         ut::expect(value.has_value());
         ut::expect(value.value() == method_foo_result{.foo_c = true, .foo_d = "new world"});
         ut::expect(std::holds_alternative<std::string>(id));
         ut::expect(std::get<std::string>(id) == std::string{"42"});
      });
      client.call(response_vec.at(0).first);
   };

   ut::test("valid bar request") = [&server, &client] {
      auto request_str{client.request("bar-uuid", method_bar_params{.bar_a = 1337, .bar_b = "hello world"})};
      ut::expect(request_str ==
                 R"({"jsonrpc":"2.0","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"},"id":"bar-uuid"})");

      server.on<"bar">([](method_bar_params const& params) -> glz::expected<method_bar_result, rpc::error> {
         ut::expect(params.bar_a == 1337);
         ut::expect(params.bar_b == "hello world");
         return method_bar_result{.bar_c = true, .bar_d = "new world"};
      });

      auto response_vec = server.call(request_str);
      ut::expect(response_vec.size() == 1);
      ut::expect(response_vec.at(0).first ==
                 R"({"jsonrpc":"2.0","result":{"bar_c":true,"bar_d":"new world"},"id":"bar-uuid"})");

      client.on<"bar">([](glz::expected<method_bar_result, rpc::error> value, rpc::jsonrpc_id_type id) -> void {
         ut::expect(value.has_value());
         ut::expect(value.value() == method_bar_result{.bar_c = true, .bar_d = "new world"});
         ut::expect(std::holds_alternative<std::string>(id));
         ut::expect(std::get<std::string>(id) == std::string{"bar-uuid"});
      });
      client.call(response_vec.at(0).first);
   };

   ut::test("foo request error") = [&server, &client] {
      auto request_str{client.request("42", method_foo_params{.foo_a = 1337, .foo_b = "hello world"})};
      ut::expect(request_str ==
                 R"({"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"42"})");

      server.on<"foo">([](method_foo_params const& params) -> glz::expected<method_foo_result, rpc::error> {
         ut::expect(params.foo_a == 1337);
         ut::expect(params.foo_b == "hello world");
         return glz::unexpected(rpc::error(rpc::error_e::server_error_lower, "my error"));
      });

      auto response_vec = server.call(request_str);
      ut::expect(response_vec.size() == 1);
      ut::expect(response_vec.at(0).first ==
                 R"({"jsonrpc":"2.0","error":{"code":-32000,"message":"Server error","data":"my error"},"id":"42"})");

      client.on<"foo">([](glz::expected<method_foo_result, rpc::error> value, rpc::jsonrpc_id_type id) -> void {
         ut::expect(!value.has_value());
         ut::expect(value.error() == rpc::error{rpc::error_e::server_error_lower, "my error"});
         ut::expect(std::holds_alternative<std::string>(id));
         ut::expect(std::get<std::string>(id) == std::string{"42"});
      });
      client.call(response_vec.at(0).first);
   };

   ut::test("server invalid version error") = [&server] {
      server.on<"foo">([](method_foo_params const&) -> glz::expected<method_foo_result, rpc::error> { return {}; });

      // invalid jsonrpc version
      auto response_vec =
         server.call(R"({"jsonrpc":"42.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"uuid"})");
      ut::expect(response_vec.size() == 1);
      ut::expect(
         response_vec.at(0).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"Invalid version: 42.0 only supported version is 2.0"},"id":"uuid"})");
      ut::expect(response_vec.at(0).second.get_code().value() == rpc::error_e::invalid_request);
   };

   ut::test("server method not found") = [&server] {
      server.on<"foo">([](method_foo_params const&) -> glz::expected<method_foo_result, rpc::error> { return {}; });

      // invalid method name
      auto response_vec = server.call(
         R"({"jsonrpc":"2.0","method":"invalid_method_name","params":{"foo_a":1337,"foo_b":"hello world"},"id":"uuid"})");
      ut::expect(response_vec.size() == 1);
      ut::expect(
         response_vec.at(0).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"Method: \"invalid_method_name\" not found"},"id":"uuid"})");
      ut::expect(response_vec.at(0).second.get_code().value() == rpc::error_e::method_not_found);
   };

   ut::test("server invalid json") = [&server] {
      server.on<"foo">([](method_foo_params const&) -> glz::expected<method_foo_result, rpc::error> { return {}; });

      // key "id" illformed missing `"`
      auto response_vec = server.call(
         R"({"jsonrpc":"2.0","method":"invalid_method_name","params":{"foo_a":1337,"foo_b":"hello world"},"id:"uuid"}")");
      ut::expect(response_vec.size() == 1);
      ut::expect(
         response_vec.at(0).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error","data":"1:100: syntax_error\n   {\"jsonrpc\":\"2.0\",\"method\":\"invalid_method_name\",\"params\":{\"foo_a\":1337,\"foo_b\":\"hello world\"},\"id:\"uuid\"}\"\n                                                                                                      ^\n"},"id":null})");
      ut::expect(response_vec.at(0).second.get_code().value() == rpc::error_e::parse_error);
   };

   ut::test("server invalid json batch") = [&server] {
      server.on<"foo">([](method_foo_params const&) -> glz::expected<method_foo_result, rpc::error> { return {}; });

      // batch cut at params key
      auto response_vec = server.call(
         R"([{"jsonrpc":"2.0","method":"invalid_method_name","params":{"foo_a":1337,"foo_b":"hello world"},"id":"uuid"},{"jsonrpc":"2.0","method":"invalid_method_name","params":]")");
      ut::expect(response_vec.size() == 1);
      ut::expect(
         response_vec.at(0).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error","data":"1:167: syntax_error\n   [{\"jsonrpc\":\"2.0\",\"method\":\"invalid_method_name\",\"params\":{\"foo_a\":1337,\"foo_b\":\"hello world\"},\"id\":\"uuid\"},{\"jsonrpc\":\"2.0\",\"method\":\"invalid_method_name\",\"params\":]\"\n                                                                                                                                                                         ^\n"},"id":null})");
      ut::expect(response_vec.at(0).second.get_code().value() == rpc::error_e::parse_error);
   };

   ut::test("server invalid json batch empty array") = [&server] {
      server.on<"foo">([](method_foo_params const&) -> glz::expected<method_foo_result, rpc::error> { return {}; });

      auto response_vec = server.call(R"([])");
      ut::expect(response_vec.size() == 1);
      ut::expect(response_vec.at(0).first ==
                 R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":null},"id":null})");
      ut::expect(response_vec.at(0).second.get_code().value() == rpc::error_e::invalid_request);
   };

   ut::test("server invalid json illformed batch one item") = [&server] {
      server.on<"foo">([](method_foo_params const&) -> glz::expected<method_foo_result, rpc::error> { return {}; });

      auto response_vec = server.call(R"([1])");
      ut::expect(response_vec.size() == 1);
      ut::expect(
         response_vec.at(0).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:1: syntax_error\n   1\n   ^\n"},"id":null})");
      ut::expect(response_vec.at(0).second.get_code().value() == rpc::error_e::invalid_request);
   };

   ut::test("server invalid json illformed batch three items") = [&server] {
      server.on<"foo">([](method_foo_params const&) -> glz::expected<method_foo_result, rpc::error> { return {}; });

      auto response_vec = server.call(R"([1,2,3])");
      ut::expect(response_vec.size() == 3);
      ut::expect(
         response_vec.at(0).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:1: syntax_error\n   1\n   ^\n"},"id":null})");
      ut::expect(response_vec.at(0).second.get_code().value() == rpc::error_e::invalid_request);
      ut::expect(
         response_vec.at(1).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:1: syntax_error\n   2\n   ^\n"},"id":null})");
      ut::expect(response_vec.at(1).second.get_code().value() == rpc::error_e::invalid_request);
      ut::expect(
         response_vec.at(2).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:1: syntax_error\n   3\n   ^\n"},"id":null})");
      ut::expect(response_vec.at(2).second.get_code().value() == rpc::error_e::invalid_request);
   };

   "server batch with both invalid and valid"_test = [&server] {
      server.on<"foo">([](method_foo_params const&) -> glz::expected<method_foo_result, rpc::error> { return {}; });

      auto response_vec = server.call(R"(
[
    {"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"42"},
    {"jsonrpc":"2.0","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"},"id":"bar-uuid"},
    {"jsonrpc": "2.0", "method": "invalid_method_name", "params": [42,23], "id": "2"},
    {"foo": "boo"},
    {"jsonrpc":"2.0","method":"bar","params":{"bar_a":1337,"bar_b":"hello world"}},
    {"jsonrpc":"2.0","method":"foo","params":{"foo_a":1337,"foo_b":"hello world"},"id":"4222222"}
]
)");
      ut::expect(response_vec.size() == 5);
      ut::expect(response_vec.at(0).first == R"({"jsonrpc":"2.0","result":{"foo_c":false,"foo_d":""},"id":"42"})");
      ut::expect(response_vec.at(1).first ==
                 R"({"jsonrpc":"2.0","result":{"bar_c":true,"bar_d":"new world"},"id":"bar-uuid"})");
      ut::expect(
         response_vec.at(2).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":"Method: \"invalid_method_name\" not found"},"id":"2"})");
      ut::expect(
         response_vec.at(3).first ==
         R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request","data":"1:8: unknown_key\n   {\"foo\":\"boo\"}\n          ^\n"},"id":null})");
      ut::expect(response_vec.at(4).first == R"({"jsonrpc":"2.0","result":{"foo_c":false,"foo_d":""},"id":"4222222"})");
   };
};

auto main(int, char**) -> int
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
