#include "httpd_client.hh"

#include <cassert>
#include <cstring>
#include <esp_http_client.h>
#include <esp_log.h>

class HttpdClient::Impl
{
public:
    Impl() = default;
    ~Impl() = default;

    static esp_err_t WriteCallback(esp_http_client_event_t *evt)
    {
        if (evt->event_id == HTTP_EVENT_ON_DATA && evt->user_data && evt->data_len > 0) {
            auto* response_data = static_cast<std::vector<std::byte>*>(evt->user_data);
            const std::byte* data = static_cast<const std::byte*>(evt->data);
            response_data->insert(response_data->end(), data, data + evt->data_len);
        }
        return ESP_OK;
    }

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

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.auth_type = HTTP_AUTH_TYPE_BASIC;
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
    config.timeout_ms = static_cast<int>(timeout.count());
    config.event_handler = [](esp_http_client_event_t *evt) -> esp_err_t {
        return Impl::WriteCallback(evt);
    };
    config.user_data = &m_impl->response_data;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return std::nullopt;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err == ESP_OK && status == 200) {
        return m_impl->response_data;
    } else {
        return std::nullopt;
    }
}
