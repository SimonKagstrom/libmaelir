#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace hal
{

class IOtaUpdater
{
public:
    virtual ~IOtaUpdater() = default;

    /// Return if the current application is newly updated
    virtual bool ApplicationHasBeenUpdated() const = 0;

    /// Mark the currently running application as valid (disable rollback)
    virtual void MarkApplicationAsValid() = 0;

    /// Return where to upload the firmware (e.g., "http://192.168.1.10"), or empty if not reachable
    virtual std::string GetUpdateUrl() = 0;

    /// Return the name of the Wifi access point to connect to, or empty if using an existing network
    virtual std::string GetAccessPointSsid() = 0;

    /// Perform the update. This might be a blocking call. The progress is reported in percent
    virtual void Update(std::function<void(uint8_t)> progress) = 0;
};

} // namespace hal
