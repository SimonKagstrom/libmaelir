#pragma once

#include "hal/i_display.hh"
#include "hal/i_ota_updater.hh"

#include <esp_http_server.h>
#include <memory>
#include <string>
#include <string_view>

class TargetHttpdOtaUpdater : public hal::IOtaUpdater
{
public:
    enum class Mode
    {
        // Start an access point of its own (e.g., where there is no other network)
        kAccessPoint,
        // Use the existing Wifi connection
        kStation,
    };

    explicit TargetHttpdOtaUpdater(hal::IDisplay& display,
                                   Mode mode = Mode::kAccessPoint,
                                   std::string_view access_point_name = "Maelir");

private:
    void Update(std::function<void(uint8_t)> progress) final;

    bool ApplicationHasBeenUpdated() const final;

    void MarkApplicationAsValid() final;

    std::string GetUpdateUrl() final;

    std::string GetAccessPointSsid() final;

    esp_err_t IndexGetHandler(httpd_req_t* req);
    esp_err_t UpdatePostHandler(httpd_req_t* req);

    esp_err_t HttpServerInit();
    esp_err_t SoftApInit();

    hal::IDisplay& m_display;
    const Mode m_mode;
    std::unique_ptr<char[]> m_receive_buf;
    httpd_handle_t m_http_server {nullptr};
    bool m_application_has_been_updated {false};
    std::string m_wifi_ssid;

    std::function<void(uint8_t)> m_progress;
};

// TMP
void wifi_task(void* Param);