#include "nmea_parser.hh"
#include "test.hh"

#include <format>
#include <string>

using namespace std::string_literals;

namespace
{

// Real-world sentences with valid checksums
constexpr auto kGga = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
constexpr auto kRmc = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
constexpr auto kVtg = "$GPVTG,360.0,T,348.7,M,000.0,N,000.0,K*43\r\n";

// "GPGGA,..." -> "$GPGGA,...*XX\r\n", with a valid checksum
std::string
Sentence(std::string_view body)
{
    uint8_t checksum = 0;
    for (auto c : body)
    {
        checksum ^= static_cast<uint8_t>(c);
    }

    return std::format("${}*{:02X}\r\n", body, checksum);
}

void
RequireMunichPosition(const std::optional<hal::RawGpsData>& data)
{
    REQUIRE(data);
    REQUIRE(data->position);
    REQUIRE(data->position->latitude == doctest::Approx(48.1173));
    REQUIRE(data->position->longitude == doctest::Approx(11.5167));
}

} // namespace

TEST_SUITE_BEGIN("nmea_parser");

TEST_CASE("the NMEA parser can pass GPS data in the sunny-day case")
{
    NmeaParser parser;

    SUBCASE("GGA gives a position")
    {
        auto data = parser.PushData(kGga);

        RequireMunichPosition(data);
        REQUIRE_FALSE(data->speed);
        REQUIRE_FALSE(data->heading);
    }

    SUBCASE("RMC gives position, speed and heading")
    {
        auto data = parser.PushData(kRmc);

        RequireMunichPosition(data);
        REQUIRE(data->speed == doctest::Approx(22.4));
        REQUIRE(data->heading == doctest::Approx(84.4));
    }

    SUBCASE("VTG gives speed and heading")
    {
        auto data = parser.PushData(kVtg);

        REQUIRE(data);
        REQUIRE_FALSE(data->position);
        REQUIRE(data->speed == doctest::Approx(0));
        REQUIRE(data->heading == doctest::Approx(360));
    }

    SUBCASE("a plain newline is accepted as line ending")
    {
        RequireMunichPosition(
            parser.PushData("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\n"));
    }

    SUBCASE("a lowercase checksum is accepted")
    {
        RequireMunichPosition(parser.PushData(
            "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6a\n"));
    }

    SUBCASE("data is only returned once")
    {
        REQUIRE(parser.PushData(kGga));
        REQUIRE_FALSE(parser.PushData(""));
        REQUIRE_FALSE(parser.PushData("\r\n"));
    }
}

TEST_CASE("the NMEA parser accepts any talker ID")
{
    NmeaParser parser;

    for (std::string talker : {"GP", "GN", "GL", "GA", "BD"})
    {
        CAPTURE(talker);

        RequireMunichPosition(parser.PushData(Sentence(
            std::format("{}GGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,", talker))));
        RequireMunichPosition(parser.PushData(Sentence(std::format(
            "{}RMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W", talker))));
        REQUIRE(parser.PushData(
            Sentence(std::format("{}VTG,360.0,T,348.7,M,000.0,N,000.0,K", talker))));
    }
}

TEST_CASE("the NMEA parser handles the southern and western hemispheres")
{
    NmeaParser parser;

    auto data = parser.PushData(
        Sentence("GPRMC,123519,A,3356.000,S,01825.500,W,000.5,054.7,191194,020.3,E"));

    REQUIRE(data);
    REQUIRE(data->position);
    REQUIRE(data->position->latitude == doctest::Approx(-33.9333));
    REQUIRE(data->position->longitude == doctest::Approx(-18.425));
}

TEST_CASE("the NMEA parser works if data is split up")
{
    NmeaParser parser;

    SUBCASE("in two parts")
    {
        auto data = parser.PushData("$GPGGA,123519,4807.038,N");
        REQUIRE(data == std::nullopt);

        data = parser.PushData(",01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\n");
        RequireMunichPosition(data);
    }

    SUBCASE("one character at a time")
    {
        std::optional<hal::RawGpsData> data;
        auto returned = 0;
        for (auto c : std::string_view(kRmc))
        {
            if (auto cur = parser.PushData(std::string_view(&c, 1)))
            {
                data = cur;
                returned++;
            }
        }

        // On the \r
        REQUIRE(returned == 1);
        RequireMunichPosition(data);
    }

    SUBCASE("in the checksum")
    {
        REQUIRE_FALSE(
            parser.PushData("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4"));
        RequireMunichPosition(parser.PushData("7\r\n"));
    }
}

TEST_CASE("the NMEA parser merges multiple sentences in one chunk")
{
    NmeaParser parser;

    SUBCASE("GGA followed by VTG gives position, speed and heading")
    {
        auto data = parser.PushData(std::string(kGga) + kVtg);

        RequireMunichPosition(data);
        REQUIRE(data->speed == doctest::Approx(0));
        REQUIRE(data->heading == doctest::Approx(360));
    }

    SUBCASE("VTG followed by GGA gives position, speed and heading")
    {
        auto data = parser.PushData(std::string(kVtg) + kGga);

        RequireMunichPosition(data);
        REQUIRE(data->speed == doctest::Approx(0));
        REQUIRE(data->heading == doctest::Approx(360));
    }

    SUBCASE("the latest value wins")
    {
        auto data =
            parser.PushData(std::string(kRmc) + Sentence("GPVTG,123.0,T,110.0,M,010.0,N,018.5,K"));

        RequireMunichPosition(data);
        REQUIRE(data->speed == doctest::Approx(10));
        REQUIRE(data->heading == doctest::Approx(123));
    }

    SUBCASE("many sentences don't overflow anything")
    {
        std::string buf;
        for (auto i = 0; i < 64; i++)
        {
            buf += kGga;
        }

        RequireMunichPosition(parser.PushData(buf));
    }

    SUBCASE("an invalid sentence doesn't clear earlier data")
    {
        auto data =
            parser.PushData(std::string(kGga) + Sentence("GPGGA,123519,,,,,0,00,99.9,,M,,M,,"));

        RequireMunichPosition(data);
    }
}

TEST_CASE("the NMEA parser rejects sentences with invalid checksums")
{
    NmeaParser parser;

    SUBCASE("wrong checksum")
    {
        REQUIRE_FALSE(parser.PushData(
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*48\r\n"));
    }

    SUBCASE("corrupted data")
    {
        // A bit flip in the latitude
        REQUIRE_FALSE(parser.PushData(
            "$GPGGA,123519,4817.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"));
    }

    SUBCASE("missing checksum")
    {
        REQUIRE_FALSE(
            parser.PushData("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,\r\n"));
    }

    SUBCASE("non-hex checksum")
    {
        REQUIRE_FALSE(parser.PushData(
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*G7\r\n"));
        REQUIRE_FALSE(parser.PushData(
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4G\r\n"));
    }

    SUBCASE("short checksum")
    {
        REQUIRE_FALSE(parser.PushData(
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4\r\n"));
    }

    SUBCASE("garbage after the checksum")
    {
        REQUIRE_FALSE(parser.PushData(
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47X\r\n"));
    }

    // And the parser recovers
    RequireMunichPosition(parser.PushData(kGga));
}

TEST_CASE("the NMEA parser doesn't report data without a fix")
{
    NmeaParser parser;

    SUBCASE("GGA with fix quality 0")
    {
        REQUIRE_FALSE(parser.PushData(Sentence("GPGGA,123519,,,,,0,00,99.9,,M,,M,,")));
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPGGA,123519,4807.038,N,01131.000,E,0,08,0.9,545.4,M,46.9,M,,")));
    }

    SUBCASE("GGA with an estimated (dead reckoning) fix")
    {
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPGGA,123519,4807.038,N,01131.000,E,6,08,0.9,545.4,M,46.9,M,,")));
    }

    SUBCASE("GGA with RTK fixes are fine")
    {
        RequireMunichPosition(parser.PushData(
            Sentence("GNGGA,123519,4807.038,N,01131.000,E,4,27,0.5,545.4,M,46.9,M,2.0,")));
        RequireMunichPosition(parser.PushData(
            Sentence("GNGGA,123519,4807.038,N,01131.000,E,5,27,0.5,545.4,M,46.9,M,2.0,")));
    }

    SUBCASE("RMC with status V")
    {
        REQUIRE_FALSE(parser.PushData(Sentence("GPRMC,123519,V,,,,,,,230394,,,N")));
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPRMC,123519,V,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W")));
    }

    SUBCASE("RMC with mode indicator N")
    {
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W,N")));
    }

    SUBCASE("VTG with empty fields")
    {
        REQUIRE_FALSE(parser.PushData(Sentence("GPVTG,,T,,M,,N,,K,N")));
        REQUIRE_FALSE(parser.PushData(Sentence("GPVTG,,,,,,,,")));
    }

    SUBCASE("VTG with mode indicator N")
    {
        REQUIRE_FALSE(parser.PushData(Sentence("GPVTG,360.0,T,348.7,M,000.0,N,000.0,K,N")));
    }
}

TEST_CASE("the NMEA parser handles empty fields")
{
    NmeaParser parser;

    SUBCASE("RMC without a course gives position and speed, but no heading")
    {
        auto data =
            parser.PushData(Sentence("GPRMC,123519,A,4807.038,N,01131.000,E,0.01,,230394,,,A"));

        RequireMunichPosition(data);
        REQUIRE(data->speed == doctest::Approx(0.01));
        REQUIRE_FALSE(data->heading);
    }

    SUBCASE("VTG without a course gives speed, but no heading")
    {
        auto data = parser.PushData(Sentence("GPVTG,,T,,M,0.01,N,0.02,K,A"));

        REQUIRE(data);
        REQUIRE(data->speed == doctest::Approx(0.01));
        REQUIRE_FALSE(data->heading);
    }

    SUBCASE("a missing hemisphere gives no position")
    {
        auto data = parser.PushData(
            Sentence("GPRMC,123519,A,4807.038,,01131.000,E,022.4,084.4,230394,003.1,W"));

        REQUIRE(data);
        REQUIRE_FALSE(data->position);
        REQUIRE(data->speed);
    }

    SUBCASE("a hemisphere without a coordinate gives no position")
    {
        REQUIRE_FALSE(parser.PushData(Sentence("GPGGA,123519,,S,,W,1,08,0.9,545.4,M,46.9,M,,")));

        auto data = parser.PushData(Sentence("GPRMC,123519,A,,S,,W,022.4,084.4,230394,003.1,W"));
        REQUIRE(data);
        REQUIRE_FALSE(data->position);
    }

    SUBCASE("an empty sentence")
    {
        REQUIRE_FALSE(parser.PushData("$*00\r\n"));
        REQUIRE_FALSE(parser.PushData(Sentence("GPGGA")));
        REQUIRE_FALSE(parser.PushData(Sentence("GPGGA,")));
    }
}

TEST_CASE("the NMEA parser handles broken messages")
{
    NmeaParser parser;

    SUBCASE("too few fields")
    {
        REQUIRE_FALSE(parser.PushData(Sentence("GPGGA,123519,4807.038,N,01131.000")));
        REQUIRE_FALSE(parser.PushData(Sentence("GPRMC,123519,A,4807.038,N")));
    }

    SUBCASE("malformed numbers")
    {
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPGGA,123519,48o7.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,")));
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPGGA,123519,4807.038,N,01131.000,E,x,08,0.9,545.4,M,46.9,M,,")));
        REQUIRE_FALSE(parser.PushData(Sentence("GPVTG,36x.0,T,348.7,M,0x10,N,000.0,K")));
    }

    SUBCASE("minutes out of range")
    {
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPGGA,123519,4867.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,")));
    }

    SUBCASE("unknown sentences are ignored")
    {
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00")));
        REQUIRE_FALSE(parser.PushData(Sentence("PMTK001,314,3")));
        REQUIRE_FALSE(parser.PushData(
            Sentence("GPGG,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,")));
    }

    // And the parser recovers
    RequireMunichPosition(parser.PushData(kGga));
}

TEST_CASE("the NMEA parser resyncs on broken streams")
{
    NmeaParser parser;

    SUBCASE("an oversized message is thrown")
    {
        std::string buf = "$GPGGA,";
        for (auto i = 0; i < 1024; i++)
        {
            buf.push_back('1');
        }
        buf += "*00\r\n";

        REQUIRE_FALSE(parser.PushData(buf));
    }

    SUBCASE("an oversized message without newline doesn't hide the next sentence")
    {
        std::string buf = "$GPGGA,";
        for (auto i = 0; i < 1024; i++)
        {
            buf.push_back('1');
        }

        RequireMunichPosition(parser.PushData(buf + kGga));
    }

    SUBCASE("a truncated sentence followed by a valid one")
    {
        RequireMunichPosition(parser.PushData(std::string("$GPGGA,123519,4807.0") + kGga));
        RequireMunichPosition(parser.PushData(std::string("$GPVTG,360.0,T*") + kGga));
        RequireMunichPosition(parser.PushData(std::string("$GPVTG,360.0,T*4") + kGga));
        RequireMunichPosition(parser.PushData(std::string("$GPVTG,360.0,T*43") + kGga));
    }

    SUBCASE("a truncated sentence followed by a valid one of another type")
    {
        auto data = parser.PushData(std::string("$GPGGA,123519,4807.038,N,01131.000,E,1,0") + kVtg);

        REQUIRE(data);
        REQUIRE_FALSE(data->position);
        REQUIRE(data->heading == doctest::Approx(360));
    }

    SUBCASE("a sentence broken by a newline")
    {
        REQUIRE_FALSE(parser.PushData(
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,\n545.4,M,46.9,M,,*47\r\n"));
    }

    SUBCASE("binary garbage and padding between sentences")
    {
        auto buf = "\0\0\xff\xfe garbage\r\n"s + kGga + std::string(32, '\0');

        RequireMunichPosition(parser.PushData(buf));
    }

    SUBCASE("binary garbage within a sentence")
    {
        REQUIRE_FALSE(parser.PushData(
            "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,\0*47\r\n"s));
    }

    // And the parser recovers
    RequireMunichPosition(parser.PushData(kRmc));
}

TEST_SUITE_END();
