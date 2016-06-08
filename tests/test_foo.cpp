#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include <string.h>

#include <response.hpp>

using namespace foo;

namespace
{
    inline std::vector<char> from_string(const char* str)
    {
        std::vector<char> vec;
        std::copy(str, str + strlen(str), std::back_inserter(vec));
        return vec;
    }
}

TEST(TestResponse, EmptyHeader)
{
    response resp;
    EXPECT_STREQ("", resp.status_line().c_str());
    EXPECT_STREQ("", resp.reason_phrase().c_str());
}

TEST(TestResponse, ReasonPhrase)
{
    response resp;
    resp.headers = from_string(
        R"(HTTP/1.1 200 OK
Content-Type: application/json
Date: Fri, 10 Jun 2016 16:45:53 GMT
Connection: keep-alive
Transfer-Encoding: chunked

)");
    EXPECT_STREQ("OK", resp.reason_phrase().c_str());

    resp.headers = from_string(
        R"(HTTP/1.1 414 Request-URI Too Long
Content-Type: text/html; charset=ISO-8859-4
Content-Length: 2748
Date: Fri, 10 Jun 2016 16:49:56 GMT

)");
    EXPECT_STREQ("Request-URI Too Long", resp.reason_phrase().c_str());
}

TEST(TestResponse, StatusLine)
{
    response resp;
    resp.headers = from_string(
        R"(HTTP/1.1 200 OK
Content-Type: application/json
Date: Fri, 10 Jun 2016 16:45:53 GMT
Connection: keep-alive
Transfer-Encoding: chunked

)");
    EXPECT_STREQ("HTTP/1.1 200 OK", resp.status_line().c_str());
}

// vim:et ts=4 sw=4 noic cc=80
