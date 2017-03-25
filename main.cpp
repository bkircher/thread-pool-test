#include "response.hpp"
#include "thread_pool.hpp"

#include <curl/curl.h>

#include <exception>
#include <future>
#include <iostream>
#include <ostream>
#include <string>
#include <vector>

using namespace foo;

int main()
{
    std::vector<std::string> urls{"example.com", "google.com",           "otris.de",     "microsoft.com",
                                  "amicaldo.de", "news.ycombinator.com", "curl.haxx.se", "apple.com",
                                  "github.com",  "de.godaddy.com"};

    curl_global_init(CURL_GLOBAL_DEFAULT);

    thread_pool pool;
    std::vector<std::future<response>> completed;

    try
    {
        for (const auto& url : urls)
        {
            completed.push_back(pool.submit([&]() -> response {
                CURL* handle = curl_easy_init();
                curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
                curl_easy_perform(handle);
                response res;
                curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &res.code);
                curl_easy_cleanup(handle);
                return res;
            }));
        }

        for (auto& result : completed)
        {
            std::cout << result.get().code << std::endl;
        }
    }
    catch (std::exception& exc)
    {
        std::cerr << exc.what() << std::endl;
    }

    curl_global_cleanup();
    return 0;
}

// vim:et ts=4 sw=4 noic cc=120
