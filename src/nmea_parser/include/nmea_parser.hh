#pragma once

#include "hal/i_gps.hh"
#include "nmea_parser_state_machine_base.hh"

#include <etl/string.h>
#include <optional>
#include <string_view>

class NmeaParser : public NmeaParserStateMachineBase<>
{
public:
    // Return the data from all valid GGA, RMC and VTG sentences completed by this call, where
    // later sentences override earlier ones. Speed is in knots, heading in degrees
    std::optional<hal::RawGpsData> PushData(std::string_view data);

private:
    // NmeaParserStateMachineBase, see nmea_parser_state_machine.md
    WaitForDollar::Next Evaluate(WaitForDollar&) final;
    Sentence::Next Evaluate(Sentence&) final;
    Checksum0::Next Evaluate(Checksum0&) final;
    Checksum1::Next Evaluate(Checksum1&) final;
    EndOfLine::Next Evaluate(EndOfLine&) final;
    Complete::Next Evaluate(Complete&) final;

    void Exit(WaitForDollar&) final;
    void Exit(Sentence&) final;
    void Exit(Checksum0&) final;
    void Exit(Checksum1&) final;
    void Exit(EndOfLine&) final;
    void Enter(Complete&) final;

    // The next character, without consuming it
    std::optional<char> PeekChar() const;
    char ConsumeChar();

    std::string_view SentenceView() const;

    // The comma-separated fields of a sentence
    class Fields;

    void ParseSentence(std::string_view sentence);
    void ParseGga(const Fields& fields);
    void ParseRmc(const Fields& fields);
    void ParseVtg(const Fields& fields);

    void Merge(const hal::RawGpsData& data);

    std::optional<char> m_next_char;
    // The characters between $ and *. NMEA limits sentences to 82 characters, but allow some
    // slack for receivers which don't care
    etl::string<128> m_sentence;
    uint8_t m_checksum {0};

    std::optional<hal::RawGpsData> m_pending_data;
};
