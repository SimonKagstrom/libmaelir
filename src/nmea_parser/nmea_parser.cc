// https://aprs.gids.nl/nmea
#include "nmea_parser.hh"

#include <cmath>
#include <etl/vector.h>
#include <ranges>

namespace
{

// Everything which can be part of the data between $ and *
bool
IsSentenceCharacter(char c)
{
    return c >= ' ' && c <= '~' && c != '$' && c != '*';
}

bool
IsEndOfLine(char c)
{
    return c == '\r' || c == '\n';
}

std::optional<uint8_t>
HexValue(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }

    return std::nullopt;
}

uint8_t
CalculateChecksum(std::string_view sentence)
{
    uint8_t checksum = 0;
    for (auto c : sentence)
    {
        checksum ^= static_cast<uint8_t>(c);
    }

    return checksum;
}

// An unsigned decimal number, e.g., "4807.038". strtod would also accept, e.g., " 1e3",
// "0x10" and "inf", and is locale dependent
std::optional<double>
ParseNumber(std::string_view word)
{
    auto value = 0.0;
    auto scale = 1.0;
    auto seen_digit = false;
    auto seen_dot = false;

    for (auto c : word)
    {
        if (c >= '0' && c <= '9')
        {
            seen_digit = true;
            if (seen_dot)
            {
                scale /= 10;
                value += (c - '0') * scale;
            }
            else
            {
                value = value * 10 + (c - '0');
            }
        }
        else if (c == '.' && !seen_dot)
        {
            seen_dot = true;
        }
        else
        {
            return std::nullopt;
        }
    }

    if (!seen_digit)
    {
        return std::nullopt;
    }

    return value;
}

// (d)ddmm.mmmm and a hemisphere, e.g., "4807.038", "N" -> 48.1173
std::optional<float>
ParseCoordinate(std::string_view value,
                std::string_view hemisphere,
                std::string_view positive,
                std::string_view negative)
{
    auto nmea = ParseNumber(value);
    if (!nmea)
    {
        return std::nullopt;
    }

    auto degrees = std::floor(*nmea / 100);
    auto minutes = *nmea - degrees * 100;
    if (minutes >= 60)
    {
        return std::nullopt;
    }

    auto coordinate = static_cast<float>(degrees + minutes / 60);
    if (hemisphere == positive)
    {
        return coordinate;
    }
    if (hemisphere == negative)
    {
        return -coordinate;
    }

    return std::nullopt;
}

std::optional<GpsPosition>
ParsePosition(std::string_view latitude,
              std::string_view latitude_hemisphere,
              std::string_view longitude,
              std::string_view longitude_hemisphere)
{
    auto lat = ParseCoordinate(latitude, latitude_hemisphere, "N", "S");
    auto lon = ParseCoordinate(longitude, longitude_hemisphere, "E", "W");

    if (lat && lon)
    {
        return GpsPosition {*lat, *lon};
    }

    return std::nullopt;
}

std::optional<float>
ParseFloat(std::string_view word)
{
    if (auto value = ParseNumber(word))
    {
        return static_cast<float>(*value);
    }

    return std::nullopt;
}

} // namespace

class NmeaParser::Fields
{
public:
    explicit Fields(std::string_view sentence)
    {
        for (const auto sub_range : std::views::split(sentence, ','))
        {
            if (m_fields.full())
            {
                // Only the first fields are used
                break;
            }
            m_fields.emplace_back(sub_range.begin(), sub_range.end());
        }
    }

    // Missing fields are empty
    std::string_view operator[](size_t index) const
    {
        return index < m_fields.size() ? m_fields[index] : std::string_view {};
    }

private:
    etl::vector<std::string_view, 16> m_fields;
};


std::optional<hal::RawGpsData>
NmeaParser::PushData(std::string_view data)
{
    for (auto c : data)
    {
        m_next_char = c;
        RunStateMachine();
    }

    return std::exchange(m_pending_data, std::nullopt);
}

std::optional<char>
NmeaParser::PeekChar() const
{
    return m_next_char;
}

char
NmeaParser::ConsumeChar()
{
    // Only called when leaving a state, i.e., after a successful PeekChar()
    debug_assert(m_next_char);

    auto c = *m_next_char;
    m_next_char = std::nullopt;

    if (c == '$')
    {
        // Starts a new sentence, from all states
        m_sentence.clear();
    }

    return c;
}

std::string_view
NmeaParser::SentenceView() const
{
    return std::string_view(m_sentence.data(), m_sentence.size());
}

NmeaParser::WaitForDollar::Next
NmeaParser::Evaluate(WaitForDollar&)
{
    auto c = PeekChar();
    if (!c)
    {
        return kStay;
    }
    if (*c == '$')
    {
        return State::kSentence;
    }

    return State::kWaitForDollar;
}

void
NmeaParser::Exit(WaitForDollar&)
{
    ConsumeChar();
}

NmeaParser::Sentence::Next
NmeaParser::Evaluate(Sentence&)
{
    auto c = PeekChar();
    if (!c)
    {
        return kStay;
    }
    if (*c == '$')
    {
        return State::kSentence;
    }
    if (*c == '*')
    {
        return State::kChecksum0;
    }
    if (!IsSentenceCharacter(*c) || m_sentence.full())
    {
        return State::kWaitForDollar;
    }

    return State::kSentence;
}

void
NmeaParser::Exit(Sentence&)
{
    auto c = ConsumeChar();

    if (IsSentenceCharacter(c) && !m_sentence.full())
    {
        m_sentence.push_back(c);
    }
}

NmeaParser::Checksum0::Next
NmeaParser::Evaluate(Checksum0&)
{
    auto c = PeekChar();
    if (!c)
    {
        return kStay;
    }
    if (HexValue(*c))
    {
        return State::kChecksum1;
    }
    if (*c == '$')
    {
        return State::kSentence;
    }

    return State::kWaitForDollar;
}

void
NmeaParser::Exit(Checksum0&)
{
    m_checksum = HexValue(ConsumeChar()).value_or(0) << 4;
}

NmeaParser::Checksum1::Next
NmeaParser::Evaluate(Checksum1&)
{
    auto c = PeekChar();
    if (!c)
    {
        return kStay;
    }
    if (HexValue(*c))
    {
        return State::kEndOfLine;
    }
    if (*c == '$')
    {
        return State::kSentence;
    }

    return State::kWaitForDollar;
}

void
NmeaParser::Exit(Checksum1&)
{
    m_checksum |= HexValue(ConsumeChar()).value_or(0);
}

NmeaParser::EndOfLine::Next
NmeaParser::Evaluate(EndOfLine&)
{
    auto c = PeekChar();
    if (!c)
    {
        return kStay;
    }
    if (IsEndOfLine(*c) && CalculateChecksum(SentenceView()) == m_checksum)
    {
        return State::kComplete;
    }
    if (*c == '$')
    {
        return State::kSentence;
    }

    return State::kWaitForDollar;
}

void
NmeaParser::Exit(EndOfLine&)
{
    ConsumeChar();
}

void
NmeaParser::Enter(Complete&)
{
    ParseSentence(SentenceView());
}

NmeaParser::Complete::Next
NmeaParser::Evaluate(Complete&)
{
    return State::kWaitForDollar;
}

void
NmeaParser::ParseSentence(std::string_view sentence)
{
    auto fields = Fields(sentence);

    // Talker ID (GP, GN, GL, ...) + sentence type
    auto address = fields[0];
    if (address.size() != 5)
    {
        // E.g., proprietary $PMTK...
        return;
    }

    auto type = address.substr(2);
    if (type == "GGA")
    {
        ParseGga(fields);
    }
    else if (type == "RMC")
    {
        ParseRmc(fields);
    }
    else if (type == "VTG")
    {
        ParseVtg(fields);
    }
}

void
NmeaParser::ParseGga(const Fields& fields)
{
    // GPGGA,time,lat,N/S,lon,E/W,fix quality,satellites,hdop,altitude,M,height,M,dgps age,dgps id
    // GPGGA,174558.00,5917.60788,N,01757.40192,E,1,08,1.08,48.3,M,24.6,M,,
    constexpr auto kLatitudeIndex = 2;
    constexpr auto kLatitudeHemisphereIndex = 3;
    constexpr auto kLongitudeIndex = 4;
    constexpr auto kLongitudeHemisphereIndex = 5;
    constexpr auto kFixQualityIndex = 6;

    // 0: invalid, 1: GPS, 2: DGPS, 3: PPS, 4: RTK, 5: float RTK, 6: estimated, 7: manual,
    // 8: simulation
    auto fix = fields[kFixQualityIndex];
    if (fix.size() != 1 || fix[0] < '1' || fix[0] > '5')
    {
        return;
    }

    Merge({
        .position = ParsePosition(fields[kLatitudeIndex],
                                  fields[kLatitudeHemisphereIndex],
                                  fields[kLongitudeIndex],
                                  fields[kLongitudeHemisphereIndex]),
    });
}

void
NmeaParser::ParseRmc(const Fields& fields)
{
    // GPRMC,time,status,lat,N/S,lon,E/W,speed (knots),course,date,variation,E/W,mode
    // GNRMC,072446.00,A,3130.5226316,N,12024.0937010,E,0.01,0.00,040620,0.0,E,D
    constexpr auto kStatusIndex = 2;
    constexpr auto kLatitudeIndex = 3;
    constexpr auto kLatitudeHemisphereIndex = 4;
    constexpr auto kLongitudeIndex = 5;
    constexpr auto kLongitudeHemisphereIndex = 6;
    constexpr auto kSpeedIndex = 7;
    constexpr auto kCourseIndex = 8;
    // NMEA 2.3 and later
    constexpr auto kModeIndex = 12;

    if (fields[kStatusIndex] != "A" || fields[kModeIndex] == "N")
    {
        return;
    }

    Merge({
        .position = ParsePosition(fields[kLatitudeIndex],
                                  fields[kLatitudeHemisphereIndex],
                                  fields[kLongitudeIndex],
                                  fields[kLongitudeHemisphereIndex]),
        .heading = ParseFloat(fields[kCourseIndex]),
        .speed = ParseFloat(fields[kSpeedIndex]),
    });
}

void
NmeaParser::ParseVtg(const Fields& fields)
{
    // GPVTG,course,T,magnetic course,M,speed (knots),N,speed (km/h),K,mode
    // GPVTG,360.0,T,348.7,M,000.0,N,000.0,K
    constexpr auto kCourseIndex = 1;
    constexpr auto kSpeedIndex = 5;
    // NMEA 2.3 and later
    constexpr auto kModeIndex = 9;

    if (fields[kModeIndex] == "N")
    {
        return;
    }

    Merge({
        .heading = ParseFloat(fields[kCourseIndex]),
        .speed = ParseFloat(fields[kSpeedIndex]),
    });
}

void
NmeaParser::Merge(const hal::RawGpsData& data)
{
    if (!data.position && !data.heading && !data.speed)
    {
        return;
    }

    auto& pending = m_pending_data ? *m_pending_data : m_pending_data.emplace();
    if (data.position)
    {
        pending.position = data.position;
    }
    if (data.heading)
    {
        pending.heading = data.heading;
    }
    if (data.speed)
    {
        pending.speed = data.speed;
    }
}
