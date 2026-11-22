#pragma once

#include "https_client.hh"

#include <memory>
#include <optional>
#include <string>
#include <trompeloeil/mock.hpp>
#include <vector>

/**
 * @brief Link-time replacement for the HttpsClient in unit tests
 *
 * Every HttpsClient::Get() is forwarded to the mock from GetMock(), so set expectations on it.
 * Hold the returned pointer for the duration of the test.
 */
namespace mock_https_client
{

class MockHttpsClient
{
public:
    MAKE_MOCK1(Get, std::optional<std::vector<std::byte>>(const std::string& url));
};

/// The mock, created if nobody holds it
std::shared_ptr<MockHttpsClient> GetMock();

} // namespace mock_https_client
