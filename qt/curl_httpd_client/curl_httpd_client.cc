#include "httpd_client.hh"

#include <cassert>
#include <curl/curl.h>

class HttpdClient::Impl
{
public:
    Impl()
    {
        curl_global_init(CURL_GLOBAL_ALL);
        curl = curl_easy_init();
        assert(curl);
    }

    ~Impl()
    {
        curl_easy_cleanup(curl);
        curl_global_cleanup();
    }

    static size_t WriteCallback(void* ptr, size_t size, size_t nmemb, void* userdata)
    {
        auto p = static_cast<Impl*>(userdata);

        auto byte_size = size * nmemb;

        p->response_data.insert(p->response_data.end(),
                                static_cast<std::byte*>(ptr),
                                static_cast<std::byte*>(ptr) + byte_size);

        return byte_size;
    }

    CURL* curl;

    std::vector<std::byte> response_data;
};

HttpdClient::HttpdClient()
    : m_impl(new Impl())
{
}

HttpdClient::~HttpdClient()
{
    delete m_impl;
}

std::optional<std::vector<std::byte>>
HttpdClient::Get(const std::string& url, milliseconds timeout)
{
    m_impl->response_data.clear();

    curl_easy_setopt(m_impl->curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_impl->curl, CURLOPT_TIMEOUT_MS, static_cast<long>(timeout.count()));

    curl_easy_setopt(m_impl->curl, CURLOPT_WRITEFUNCTION, Impl::WriteCallback);
    curl_easy_setopt(
        m_impl->curl, CURLOPT_WRITEDATA, (void*)m_impl);

    auto res = curl_easy_perform(m_impl->curl);
    if (res != CURLE_OK)
    {
        return std::nullopt; // Handle error appropriately
    }

    return m_impl->response_data;
}
