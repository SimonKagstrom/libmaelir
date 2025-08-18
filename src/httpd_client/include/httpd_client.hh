// Simple client for HTTP requests, implemented for esp32/qt
#pragma once

#include "time.hh"

#include <optional>
#include <vector>

class HttpdClient
{
public:
    HttpdClient();
    virtual ~HttpdClient();

    std::optional<std::vector<std::byte>> Get(const std::string& url, milliseconds timeout = 5s);

    class Impl;

    Impl* m_impl {nullptr};
};
