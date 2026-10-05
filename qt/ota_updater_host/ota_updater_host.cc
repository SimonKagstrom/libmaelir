#include "ota_updater_host.hh"

#include "time.hh"

// Horror!
bool g_upgrade_started;

OtaUpdaterHost::OtaUpdaterHost(bool updated)
    : m_updated(updated)
{
}


void
OtaUpdaterHost::Update(std::function<void(uint8_t)> progress)
{
    while (!g_upgrade_started)
    {
        os::Sleep(50ms);
    }

    for (auto perc = 0; perc <= 100; ++perc)
    {
        progress(perc);
        os::Sleep(50ms);
    }
}


bool
OtaUpdaterHost::ApplicationHasBeenUpdated() const
{
    return m_updated;
}

void
OtaUpdaterHost::MarkApplicationAsValid()
{
    printf("OtaUpdater: MarkApplicationAsValid\n");
}

std::string
OtaUpdaterHost::GetUpdateUrl()
{
    return "http://localhost";
}

std::string
OtaUpdaterHost::GetAccessPointSsid()
{
    return "";
}
