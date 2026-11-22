#include "mock_https_client.hh"

namespace
{

std::weak_ptr<mock_https_client::MockHttpsClient> g_mock;

} // namespace

std::shared_ptr<mock_https_client::MockHttpsClient>
mock_https_client::GetMock()
{
    if (auto mock = g_mock.lock())
    {
        return mock;
    }

    auto mock = std::make_shared<MockHttpsClient>();
    g_mock = mock;

    return mock;
}

HttpsClient::HttpsClient() = default;

HttpsClient::~HttpsClient() = default;

std::optional<std::vector<std::byte>>
HttpsClient::Get(const std::string& url, milliseconds)
{
    return mock_https_client::GetMock()->Get(url);
}
