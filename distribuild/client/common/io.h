#pragma once

#include <string>
#include <chrono>

namespace distribuild::client {

std::ptrdiff_t WriteTo(int fd, const std::string_view& data, std::size_t start);

std::ptrdiff_t ReadTo(int fd, char* data, std::size_t bytes);

void WriteAll(const std::string& filename, const std::string_view& data);

void SetNonblocking(int fd);

bool WaitForEvent(int fd, int event, std::chrono::steady_clock::time_point timeout);

}