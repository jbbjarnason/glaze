#include <glaze/glaze.hpp>
#include <glaze/tuplet/tuple.hpp>
#include <glaze/util/expected.hpp>

#include <algorithm>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#if __cpp_lib_to_underlying
namespace util
{
   using to_underlying = std::to_underlying;
}
#else
namespace util
{
   template <typename T>
   [[nodiscard]] constexpr std::underlying_type_t<T> to_underlying(T value) noexcept
   {
      return static_cast<std::underlying_type_t<T>>(value);
   }
}
#endif

namespace glz::rpc
{
   enum struct error_e : int {
      no_error = 0,
      server_error_lower = -32000,
      server_error_upper = -32099,
      invalid_request = -32600,
      method_not_found = -32601,
      invalid_params = -32602,
      internal = -32603,
      parse_error = -32700,
   };
   static constexpr std::array<error_e, 8> error_e_iterable{
      error_e::no_error,         error_e::server_error_lower, error_e::server_error_upper, error_e::invalid_request,
      error_e::method_not_found, error_e::invalid_params,     error_e::internal,           error_e::parse_error,
   };
}

template <>
struct glz::meta<glz::rpc::error_e>
{
   using enum glz::rpc::error_e;
   static constexpr auto value{[](auto&& enum_value) -> auto { return util::to_underlying(enum_value); }};
};

// jsonrpc
namespace glz::rpc
{
   using jsonrpc_id_type = std::variant<glz::json_t::null_t, std::string, std::int64_t>;
   static constexpr std::string_view supported_version{"2.0"};

   namespace detail
   {
      template <typename CharType, unsigned N>
      struct [[nodiscard]] basic_fixed_string
      {
         using char_type = CharType;

         char_type data_[N + 1]{};

         constexpr basic_fixed_string() noexcept : data_{} {}

         template <typename other_char_type>
            requires std::same_as<other_char_type, char_type>
         constexpr basic_fixed_string(const other_char_type (&foo)[N + 1]) noexcept
         {
            std::copy_n(foo, N + 1, data_);
         }

         [[nodiscard]] constexpr std::basic_string_view<char_type> view() const noexcept { return {&data_[0], N}; }

         constexpr operator std::basic_string_view<char_type>() const noexcept { return {&data_[0], N}; }

         template <unsigned M>
         constexpr auto operator==(basic_fixed_string<char_type, M> const& r) const noexcept
         {
            return N == M && view() == r.view();
         }
      };

      template <typename char_type, unsigned N>
      basic_fixed_string(char_type const (&str)[N]) -> basic_fixed_string<char_type, N - 1>;

      template <typename char_type, std::size_t N, std::size_t M>
      consteval auto concat_fixed_string(basic_fixed_string<char_type, N> l,
                                         basic_fixed_string<char_type, M> r) noexcept
      {
         basic_fixed_string<char_type, N + M> result;
         auto it{std::copy(l.begin(), l.end(), result.begin())};
         it = std::copy(r.begin(), r.end(), it);
         *it = {};
         return result;
      }

      template <typename char_type, std::size_t N, std::size_t M, typename... U>
      consteval auto concat_fixed_string(basic_fixed_string<char_type, N> l, basic_fixed_string<char_type, M> r,
                                         U... u) noexcept
      {
         return concat_fixed_string(l, concat_fixed_string(r, u...));
      }
   }

   class error
   {
     public:
      static error invalid(parse_error const& pe, auto& buffer)
      {
         std::string format_err{format_error(pe, buffer)};
         return error(rpc::error_e::invalid_request, format_err.empty() ? glz::json_t{} : format_err);
      }
      static error version(std::string_view presumed_version)
      {
         return error(error_e::invalid_request, "Invalid version: " + std::string(presumed_version) +
                                                   " only supported version is " + std::string(rpc::supported_version));
      }
      static error method(std::string_view presumed_method)
      {
         return error(error_e::method_not_found, "Method: \"" + std::string(presumed_method) + "\" not found");
      }

      error() = default;

      explicit error(error_e code, glz::json_t&& data = {})
         : code(util::to_underlying(code)), message(code_as_string(code)), data(std::move(data))
      {}

      [[nodiscard]] auto get_code() const noexcept -> std::optional<error_e>
      {
         auto const it{std::find_if(std::cbegin(error_e_iterable), std::cend(error_e_iterable),
                                    [this](error_e err_val) { return util::to_underlying(err_val) == this->code; })};
         if (it == std::cend(error_e_iterable)) {
            return std::nullopt;
         }
         return *it;
      }
      [[nodiscard]] auto get_raw_code() const noexcept -> int { return code; }
      [[nodiscard]] auto get_message() const noexcept -> std::string const& { return message; }
      [[nodiscard]] auto get_data() const noexcept -> glz::json_t const& { return data; }

      operator bool() const noexcept
      {
         auto const my_code{get_code()};
         return my_code.has_value() && my_code.value() != rpc::error_e::no_error;
      }

      bool operator==(const rpc::error_e err) const noexcept
      {
         auto const my_code{get_code()};
         return my_code.has_value() && my_code.value() == err;
      }

     private:
      std::underlying_type_t<error_e> code{util::to_underlying(error_e::no_error)};
      std::string message{code_as_string(error_e::no_error)};  // string reflection of member variable code
      glz::json_t data{};                                      // Optional detailed error information

      static constexpr auto code_as_string(error_e error_code) -> std::string
      {
         switch (error_code) {
         case error_e::no_error:
            return "No error";
         case error_e::parse_error:
            return "Parse error";
         case error_e::server_error_lower:
         case error_e::server_error_upper:
            return "Server error";
         case error_e::invalid_request:
            return "Invalid request";
         case error_e::method_not_found:
            return "Method not found";
         case error_e::invalid_params:
            return "Invalid params";
         case error_e::internal:
            return "Internal error";
         }
         return "Unknown";
      }

     public:
      struct glaze
      {
         using T = error;
         static constexpr auto value{glz::object("code", &T::code, "message", &T::message, "data", &T::data)};
      };
   };

   template <typename params_type>
   struct request_t
   {
      using params_t = params_type;
      using jsonrpc_id_t = jsonrpc_id_type;

      request_t() = default;
      request_t(jsonrpc_id_t&& id, std::string&& method, params_t&& params)
         : id(std::move(id)), version(rpc::supported_version), method(std::move(method)), params(std::move(params))
      {}

      jsonrpc_id_t id{};
      std::string version{};
      std::string method{};
      params_t params{};

      struct glaze
      {
         using T = request_t;
         static constexpr auto value{
            glz::object("jsonrpc", &T::version, "method", &T::method, "params", &T::params, "id", &T::id)};
      };
   };
   using generic_request_t = request_t<glz::skip>;

   template <typename result_type>
   struct response_t
   {
      using result_t = result_type;
      using jsonrpc_id_t = jsonrpc_id_type;

      response_t() = default;
      explicit response_t(rpc::error&& err) : version(rpc::supported_version), error(std::move(err)) {}
      response_t(jsonrpc_id_t&& id, result_t&& result)
         : id(std::move(id)), version(rpc::supported_version), result(std::move(result))
      {}
      response_t(jsonrpc_id_t&& id, rpc::error&& err)
         : id(std::move(id)), version(rpc::supported_version), error(std::move(err))
      {}

      jsonrpc_id_t id{};
      std::string version{};
      std::optional<result_t> result{};  // todo can this be instead expected<result_t, error>
      std::optional<rpc::error> error{};
      struct glaze
      {
         using T = response_t;
         static constexpr auto value{
            glz::object("jsonrpc", &T::version, "result", &T::result, "error", &T::error, "id", &T::id)};
      };
   };
   using generic_response_t = response_t<glz::skip>;

   template <detail::basic_fixed_string name, typename params_type, typename result_type>
   struct server_method_t
   {
      static constexpr std::string_view name_v{name};
      using params_t = params_type;
      using result_t = result_type;
      using jsonrpc_id_t = jsonrpc_id_type;
      using request_t = rpc::request_t<params_t>;
      using response_t = rpc::response_t<result_t>;
      std::function<expected<result_t, rpc::error>(params_t const&)> callback{
         [](auto const&) { return glz::unexpected{rpc::error(rpc::error_e::internal, "Not implemented")}; }};
   };

   template <detail::basic_fixed_string name, typename params_type, typename result_type>
   struct client_method_t
   {
      static constexpr std::string_view name_v{name};
      using params_t = params_type;
      using result_t = result_type;
      using jsonrpc_id_t = jsonrpc_id_type;
      using request_t = rpc::request_t<params_t>;
      using response_t = rpc::response_t<result_t>;
      std::function<void(glz::expected<result_t, rpc::error> const&, jsonrpc_id_type const&)> callback{
         [](auto const&, auto const&) {}};
   };
   namespace concepts
   {
      template <typename method_t>
      concept method_type = requires(method_t meth) {
         method_t::name_v;
         {
            std::same_as<decltype(method_t::name_v), std::string_view>
         };
         typename method_t::params_t;
         typename method_t::jsonrpc_id_t;
         typename method_t::request_t;
         typename method_t::response_t;
         {
            meth.callback
         };
      };

      template <typename method_t>
      concept server_method_type = requires(method_t meth) {
         requires method_type<method_t>;
         requires std::same_as<decltype(meth.callback), std::function<expected<typename method_t::result_t, rpc::error>(
                                                           typename method_t::params_t const&)>>;
      };

      template <typename method_t>
      concept client_method_type = requires(method_t meth) {
         requires method_type<method_t>;
         requires std::same_as<
            decltype(meth.callback),
            std::function<void(glz::expected<typename method_t::result_t, rpc::error> const&, jsonrpc_id_type const&)>>;
      };
   }

   namespace detail
   {
      template <detail::basic_fixed_string name, typename... method_type>
      inline constexpr void set_callback(glz::tuplet::tuple<method_type...>& methods, auto const& callback)
      {
         constexpr bool method_found = ((method_type::name_v == name) || ...);
         static_assert(method_found, "Method not settable in given tuple.");

         methods.any([&callback](auto&& method) -> bool {
            if constexpr (name == std::remove_reference_t<decltype(method)>::name_v) {
               method.callback = callback;
               return true;
            }
            return false;
         });
      }
   }

   template <concepts::server_method_type... method_type>
   struct server
   {
      server() = default;
      glz::tuplet::tuple<method_type...> methods{};

      template <detail::basic_fixed_string name>
      constexpr void on(auto const& callback)  // std::function<expected<result_t, rpc::error>(params_t const&)>
      {
         detail::set_callback<name>(methods, callback);
      }

      // Return json stringified response_t meaning it can both be error and non-error
      // If `id` is null in json_request a response will not be generated
      std::vector<std::pair<std::string, rpc::error>> call(std::string_view json_request)
      {
         if (auto parse_err{glz::validate_json(json_request)}) {
            generic_response_t response_parse_error{
               rpc::error(rpc::error_e::parse_error, format_error(parse_err, json_request))};
            return {{glz::write_json(response_parse_error), response_parse_error.error.value()}};
         }

         // Extract each request, if request does not include and ID a std::nullopt will be returned
         auto const per_request{
            [this](std::string_view json_request) -> std::optional<std::pair<std::string, rpc::error>> {
               expected<generic_request_t, glz::parse_error> request{glz::read_json<generic_request_t>(json_request)};

               if (!request.has_value()) {
                  // Failed, but let's try to extract the `id`
                  expected<json_t, glz::parse_error> exp_json_parsed{glz::read_json<json_t>(json_request)};
                  if (exp_json_parsed.has_value()) {
                     auto const& json_parsed{exp_json_parsed.value()};
                     if (json_parsed.contains("id")) {
                        generic_response_t response_w_id{json_parsed["id"].as<jsonrpc_id_type>(),
                                                         rpc::error::invalid(request.error(), json_request)};
                        return std::make_pair(glz::write_json(response_w_id), response_w_id.error.value());
                     }
                  }
                  generic_response_t response_no_id{rpc::error::invalid(request.error(), json_request)};
                  return std::make_pair(glz::write_json(response_no_id), response_no_id.error.value());
               }

               auto& req{request.value()};
               if (req.version != rpc::supported_version) {
                  if (std::holds_alternative<glz::json_t::null_t>(req.id)) {
                     return std::nullopt;
                  }
                  generic_response_t response_inv_version{std::move(req.id), rpc::error::version(req.version)};
                  return std::make_pair(glz::write_json(response_inv_version), response_inv_version.error.value());
               }

               std::optional<std::pair<std::string, rpc::error>> return_v{};
               bool method_found = methods.any([&json_request, &req, &return_v](auto&& method) -> bool {
                  using meth_t = std::remove_reference_t<decltype(method)>;
                  if (req.method == meth_t::name_v) {
                     expected<typename meth_t::request_t, glz::parse_error> const& params_request{
                        glz::read_json<typename meth_t::request_t>(json_request)};
                     if (params_request.has_value()) {
                        expected<typename meth_t::result_t, rpc::error> result{std::invoke(
                           method.callback, params_request->params)};  // todo what if method.callback is null
                        if (result.has_value()) {
                           typename meth_t::response_t response_w_result{std::move(req.id), std::move(result.value())};
                           return_v = std::make_pair(glz::write_json(response_w_result), rpc::error{});
                        }
                        else {
                           typename meth_t::response_t response_w_err{std::move(req.id), std::move(result.error())};
                           return_v = std::make_pair(glz::write_json(response_w_err), result.error());
                        }
                     }
                     else {
                        typename meth_t::response_t response_w_parse_err{
                           std::move(req.id), rpc::error::invalid(params_request.error(), json_request)};
                        return_v =
                           std::make_pair(glz::write_json(response_w_parse_err), response_w_parse_err.error.value());
                     }
                     if (std::holds_alternative<glz::json_t::null_t>(req.id)) {
                        return_v = std::nullopt;
                     }
                     return true;
                  }
                  return false;
               });
               if (!method_found) {
                  generic_response_t response_inv_method{std::move(req.id), rpc::error::method(req.method)};
                  return std::make_pair(glz::write_json(response_inv_method), response_inv_method.error.value());
               }
               return return_v;
            }};

         auto batch_requests{glz::read_json<std::vector<glz::json_t>>(json_request)};
         if (batch_requests.has_value()) {
            if (batch_requests->empty()) {
               generic_response_t response_empty_vector{rpc::error(rpc::error_e::invalid_request)};
               return {{glz::write_json(response_empty_vector), response_empty_vector.error.value()}};
            }

            std::vector<std::pair<std::string, rpc::error>> return_vec;
            return_vec.reserve(batch_requests->size());
            for (auto&& request : batch_requests.value()) {
               auto response{per_request(glz::write_json(request))};
               if (response.has_value()) {
                  return_vec.emplace_back(std::move(response.value()));
               }
            }
            return return_vec;
         }
         else {
            auto response{per_request(json_request)};
            if (response.has_value()) {
               return {{std::move(response.value())}};
            }
         }
         return {};
      }
   };

   namespace concepts
   {
      template <typename pair_t>
      concept id_params_pair = requires(pair_t pair_v) {
         {
            pair_v.first
         };
         {
            pair_v.second
         };
         requires std::convertible_to<decltype(pair_t().first), jsonrpc_id_type>;
      };
   }

   template <concepts::client_method_type... method_type>
   struct client
   {
      // Create client with default queue size of 100
      client() = default;
      // Create client given the queue size to store request ids
      explicit client(std::size_t max_queue_size) : queue_size(max_queue_size) {}
      std::size_t const queue_size{100};
      glz::tuplet::tuple<method_type...> methods{};
      // <id, method_name>
      std::deque<std::pair<jsonrpc_id_type, std::string>> id_ring_buffer{};

      template <detail::basic_fixed_string name>
      constexpr void on(auto const& callback)  // std::function<void(glz::expected<result_t, rpc::error> const&,
                                               // jsonrpc_id_type const&)>
      {
         detail::set_callback<name>(methods, callback);
      }

      struct call_error
      {
         std::optional<glz::parse_error> parse_error{};

         enum struct other_error_e : std::uint8_t { id_not_found = 0, method_not_found };
         std::optional<other_error_e> other{};

         operator bool() const noexcept { return parse_error.has_value() || other.has_value(); }
      };

      call_error call(std::string_view json_response)
      {
         expected<generic_response_t, glz::parse_error> response{glz::read_json<generic_response_t>(json_response)};

         if (!response.has_value()) {
            return {.parse_error = response.error()};
         }

         auto& res{response.value()};

         auto id_it{std::find_if(std::begin(id_ring_buffer), std::end(id_ring_buffer),
                                 [&res](auto&& pair) { return res.id == pair.first; })};

         if (id_it == std::end(id_ring_buffer)) {
            return {.other = call_error::other_error_e::id_not_found};
         }

         jsonrpc_id_type id;
         std::string method_name;
         std::tie(id, method_name) = *id_it;

         id_ring_buffer.erase(id_it);

         call_error return_v;
         bool method_found = methods.any([&json_response, &method_name, &return_v](auto&& method) -> bool {
            using meth_t = std::remove_reference_t<decltype(method)>;
            if (method_name == meth_t::name_v) {
               expected<typename meth_t::response_t, glz::parse_error> response{
                  glz::read_json<typename meth_t::response_t>(json_response)};

               if (!response.has_value()) {
                  return_v.parse_error = response.error();
               }
               else {
                  auto& res_typed{response.value()};
                  if (res_typed.result.has_value()) {
                     std::invoke(method.callback, res_typed.result.value(), res_typed.id);
                  }
                  else if (res_typed.error.has_value()) {
                     std::invoke(method.callback, unexpected(res_typed.error.value()), res_typed.id);
                  }
                  else {
                     // todo change response_t::result, response_t::error to response_t::output where output is
                     // std::variant<result_t, parse_error>
                     return_v.parse_error = glz::parse_error{.ec = glz::error_code::key_not_found};
                  }
               }
               return true;
            }
            return false;
         });

         if (!method_found) {
            if (!return_v) {
               return_v.other = call_error::other_error_e::method_not_found;
            }
         }

         return return_v;
      }

      [[nodiscard]] constexpr auto request(concepts::id_params_pair auto&& id_and_params,
                                           concepts::id_params_pair auto&&... batch) -> std::string
      {
         const auto per_request{[this](jsonrpc_id_type&& id, auto&& params) {
            using params_type = std::remove_cvref_t<decltype(params)>;
            constexpr bool params_type_found = (std::is_same_v<typename method_type::params_t, params_type> || ...);
            static_assert(params_type_found, "Input parameters do not match constructed client parameter types");

            std::string method_name{};
            methods.any([&method_name](auto&& method) -> bool {
               using meth_t = std::remove_reference_t<decltype(method)>;
               if constexpr (std::is_same_v<typename meth_t::params_t, params_type>) {
                  method_name = meth_t::name_v;
                  return true;
               }
               return false;
            });

            id_ring_buffer.emplace_front(id, method_name);
            if (id_ring_buffer.size() > queue_size) {
               id_ring_buffer.pop_back();
            }

            rpc::request_t<params_type> req{std::forward<jsonrpc_id_type>(id), std::move(method_name),
                                            std::forward<params_type>(params)};
            return req;
         }};
         if constexpr (sizeof...(batch) > 0) {
            std::string buffer{
               "[" + glz::write_json(per_request(std::move(id_and_params.first), std::move(id_and_params.second))) +
               ","};
            (buffer.append(glz::write_json(per_request(std::move(batch.first), std::move(batch.second))).append(",")),
             ...);
            buffer.back() = ']';
            return buffer;
         }

         return glz::write_json(per_request(std::move(id_and_params.first), std::move(id_and_params.second)));
      }

      [[nodiscard]] constexpr auto request(jsonrpc_id_type&& id, auto&& params) -> std::string
      {
         return request(std::make_pair(std::forward<jsonrpc_id_type>(id), std::forward<decltype(params)>(params)));
      }
   };
}  // namespace glz::rpc
