#include <algorithm>
#include <string.h>
#include <vector>

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include <response.hpp>

using namespace foo;

namespace {
  inline std::vector<char> from_string(const char* str) {
    std::vector<char> vec;
    std::copy(str, str + strlen(str), std::back_inserter(vec));
    return vec;
  }

  TEST_CASE("response") {

    SECTION("empty header") {
      response resp;
      CHECK(resp.status_line() == "");
      CHECK(resp.reason_phrase() == "");
    }

    SECTION("reason phrase") {
      response resp;
      resp.headers = from_string(
          R"(HTTP/1.1 200 OK
Content-Type: application/json
Date: Fri, 10 Jun 2016 16:45:53 GMT
Connection: keep-alive
Transfer-Encoding: chunked

)");
      CHECK(resp.reason_phrase() == "OK");

      resp.headers = from_string(
          R"(HTTP/1.1 414 Request-URI Too Long
Content-Type: text/html; charset=ISO-8859-4
Content-Length: 2748
Date: Fri, 10 Jun 2016 16:49:56 GMT

)");
      CHECK(resp.reason_phrase() == "Request-URI Too Long");
    }

    SECTION("status line") {
      response resp;
      resp.headers = from_string(
          R"(HTTP/1.1 200 OK
Content-Type: application/json
Date: Fri, 10 Jun 2016 16:45:53 GMT
Connection: keep-alive
Transfer-Encoding: chunked

)");
      CHECK(resp.status_line() == "HTTP/1.1 200 OK");
    }
  }

} // namespace
