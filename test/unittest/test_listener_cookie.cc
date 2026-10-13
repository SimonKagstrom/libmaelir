#include "listener_cookie.hh"
#include "test.hh"

#include <vector>

TEST_SUITE_BEGIN("listener_cookie");

TEST_CASE("a default-constructed listener cookie is empty")
{
    ListenerCookie cookie;

    REQUIRE(!cookie);

    // Nothing to call
    cookie.Reset();
}

TEST_CASE("the detach function is called when the listener cookie is destroyed")
{
    auto detached = 0;

    {
        ListenerCookie cookie([&detached]() { detached++; });
        REQUIRE(cookie);
        REQUIRE(detached == 0);
    }

    REQUIRE(detached == 1);
}

TEST_CASE("a listener cookie can be reset")
{
    auto detached = 0;
    ListenerCookie cookie([&detached]() { detached++; });

    cookie.Reset();
    REQUIRE(detached == 1);
    REQUIRE(!cookie);

    WHEN("it's reset again, or destroyed")
    {
        cookie.Reset();
        cookie = ListenerCookie();

        THEN("the detach function isn't called again")
        {
            REQUIRE(detached == 1);
        }
    }
}

TEST_CASE("a listener cookie can be moved")
{
    auto detached = 0;
    ListenerCookie cookie([&detached]() { detached++; });

    WHEN("it's move-constructed")
    {
        ListenerCookie moved(std::move(cookie));

        THEN("the source is empty, and nothing is detached")
        {
            REQUIRE(!cookie);
            REQUIRE(moved);
            REQUIRE(detached == 0);
        }

        AND_WHEN("both are destroyed")
        {
            cookie = ListenerCookie();
            moved = ListenerCookie();

            THEN("the detach function is called once")
            {
                REQUIRE(detached == 1);
            }
        }
    }

    WHEN("it's move-assigned to a cookie with another listener")
    {
        auto other_detached = 0;
        ListenerCookie other([&other_detached]() { other_detached++; });

        other = std::move(cookie);

        THEN("the other listener is detached, and the moved one kept")
        {
            REQUIRE(other_detached == 1);
            REQUIRE(detached == 0);
            REQUIRE(other);
            REQUIRE(!cookie);
        }
    }

    WHEN("it's stored in a container which reallocates")
    {
        std::vector<ListenerCookie> cookies;

        cookies.push_back(std::move(cookie));
        for (auto i = 0; i < 16; i++)
        {
            cookies.emplace_back();
        }

        THEN("nothing is detached until the container is cleared")
        {
            REQUIRE(detached == 0);
            cookies.clear();
            REQUIRE(detached == 1);
        }
    }
}

TEST_SUITE_END();
