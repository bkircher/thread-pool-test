#pragma once

#include <iterator>
#include <regex>
#include <stdexcept>
#include <string.h>
#include <string>
#include <vector>

namespace foo {
  struct response final {
    long code;
    std::vector<char> headers;

    std::string status_line() const {
      if (headers.empty()) {
        return "";
      }

      const char* start = headers.data();
      const char* linebreak = strchr(start, '\n');
      return std::string(start, std::distance(start, linebreak));
    }

    std::string reason_phrase() const {
      const auto line = status_line();
      if (line.empty()) {
        return "";
      }

      const auto regex = std::regex("^HTTP\\/\\d\\.\\d \\d{3} ([- \\w]*)\\s*");
      std::smatch match_result;
      if (std::regex_match(line, match_result, regex)) {
        if (match_result.size() == 2) {
          return match_result[1].str();
        }
      }

      throw std::runtime_error("failed parsing reason phrase from status line");
    }
  };
} // namespace foo
