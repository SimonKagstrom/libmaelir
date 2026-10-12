#include "https_client.hh"

#include <cassert>
#include <cstring>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>

class HttpsClient::Impl
{
public:
    Impl() = default;

    ~Impl()
    {
        Reset();
    }

    static esp_err_t WriteCallback(esp_http_client_event_t* evt)
    {
        if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data && evt->data_len > 0)
        {
            auto* response_data = static_cast<std::vector<std::byte>*>(evt->user_data);
            const std::byte* data = static_cast<const std::byte*>(evt->data);
            response_data->insert(response_data->end(), data, data + evt->data_len);
        }
        return ESP_OK;
    }

    // Setup the client for the URL. Returns true if an existing client (and its connection)
    // is reused
    std::optional<bool> Prepare(const std::string& url, milliseconds timeout)
    {
        const auto timeout_ms = static_cast<int>(timeout.count());

        if (client)
        {
            // Keeps the connection if the host is the same
            if (esp_http_client_set_url(client, url.c_str()) == ESP_OK &&
                esp_http_client_set_timeout_ms(client, timeout_ms) == ESP_OK)
            {
                return true;
            }

            Reset();
        }

        esp_http_client_config_t config = {};
        config.url = url.c_str();
        config.is_async = false;
        config.auth_type = HTTP_AUTH_TYPE_BASIC;
        config.transport_type = HTTP_TRANSPORT_OVER_SSL;
        // Verify the server against the CA certificates bundled in ESP-IDF
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.timeout_ms = timeout_ms;
        // Detect dead connections while idle
        config.keep_alive_enable = true;
        config.event_handler = [](esp_http_client_event_t* evt) -> esp_err_t {
            return Impl::WriteCallback(evt);
        };
        config.user_data = &response_data;

        client = esp_http_client_init(&config);
        if (!client)
        {
            return std::nullopt;
        }

        return false;
    }

    void Reset()
    {
        if (client)
        {
            esp_http_client_cleanup(client);
            client = nullptr;
        }
    }

    // Kept between requests, to reuse the connection (and avoid TLS handshakes)
    esp_http_client_handle_t client {nullptr};
    std::vector<std::byte> response_data;
};

HttpsClient::HttpsClient()
    : m_impl(new Impl())
{
}

HttpsClient::~HttpsClient()
{
    delete m_impl;
}

std::optional<std::vector<std::byte>>
HttpsClient::Get(const std::string& url, milliseconds timeout)
{
    // At most one retry, if a reused connection has been closed by the server
    for (auto attempt = 0; attempt < 2; attempt++)
    {
        m_impl->response_data.clear();

        auto reused = m_impl->Prepare(url, timeout);
        if (!reused)
        {
            return std::nullopt;
        }

        auto err = esp_http_client_perform(m_impl->client);
        if (err == ESP_OK)
        {
            // The connection is still usable, also for non-200 responses
            if (esp_http_client_get_status_code(m_impl->client) == 200)
            {
                return m_impl->response_data;
            }

            return std::nullopt;
        }

        // Connection-level error: Drop the connection. Retry if it was a reused one
        m_impl->Reset();
        if (!*reused)
        {
            break;
        }
    }

    return std::nullopt;
}
