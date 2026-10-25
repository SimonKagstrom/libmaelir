#include "brightness_controller.hh"
#include "hal/mock/mock_display_brightness.hh"
#include "mock_time.hh"
#include "sun_elevation.hh"
#include "test.hh"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{

constexpr auto kStockholm = GpsPosition {59.3293f, 18.0686f};
constexpr auto kKiruna = GpsPosition {67.8558f, 20.2253f};
constexpr auto kNewYork = GpsPosition {40.7128f, -74.0060f};
constexpr auto kAuckland = GpsPosition {-36.8485f, 174.7633f};
constexpr auto kNullIsland = GpsPosition {0, 0};

// 00:00 UTC
constexpr uint32_t kSummerSolstice = 1782000000; // 2026-06-21
constexpr uint32_t kEquinox = 1773964800;        // 2026-03-20
constexpr uint32_t kWinterSolstice = 1797811200; // 2026-12-21

constexpr uint32_t kMinute = 60;
constexpr uint32_t kHour = 60 * kMinute;

// Sunrise and sunset are at -0.833°, with refraction and the upper limb of the sun
constexpr float kSunriseElevation = -0.833f;

std::pair<float, float>
MinMaxElevation(uint32_t day_start, const GpsPosition& position)
{
    auto min = std::numeric_limits<float>::max();
    auto max = std::numeric_limits<float>::lowest();

    for (auto t = day_start; t < day_start + 24 * kHour; t += kMinute)
    {
        auto elevation = SunElevation(t, position);
        min = std::min(min, elevation);
        max = std::max(max, elevation);
    }

    return {min, max};
}

// Not the defaults of any display, to see that they are used
constexpr auto kLevels = BrightnessController::Levels {
    .night = 7,
    .dusk = 25,
    .low_sun = 50,
    .day = 90,
};

class Fixture : public TimeFixture
{
public:
    void SetGpsFix(const GpsPosition& position)
    {
        state.Set<AS::position>(GpsData {position, 0, 0});
        state.Set<AS::gps_status>(GpsStatus::kPositionValid);
    }

    ApplicationState state;
    MockDisplayBrightness display;
};

} // namespace

TEST_SUITE_BEGIN("brightness_controller");

TEST_CASE("the sun elevation follows the seasons")
{
    // The noon elevation is 90° - latitude + declination, and midnight declination - (90° - latitude)
    auto [summer_min, summer_max] = MinMaxElevation(kSummerSolstice, kStockholm);
    CHECK(summer_max == doctest::Approx(54.1f).epsilon(0.01));
    CHECK(summer_min == doctest::Approx(-7.2f).epsilon(0.05));

    auto [equinox_min, equinox_max] = MinMaxElevation(kEquinox, kStockholm);
    CHECK(equinox_max == doctest::Approx(30.7f).epsilon(0.02));

    auto [winter_min, winter_max] = MinMaxElevation(kWinterSolstice, kStockholm);
    CHECK(winter_max == doctest::Approx(7.2f).epsilon(0.05));
    CHECK(winter_min == doctest::Approx(-54.1f).epsilon(0.01));

    // The midnight sun
    auto [kiruna_min, kiruna_max] = MinMaxElevation(kSummerSolstice, kKiruna);
    CHECK(kiruna_min > 0);

    // The seasons are reversed in the southern hemisphere
    auto [auckland_june_min, auckland_june_max] = MinMaxElevation(kSummerSolstice, kAuckland);
    CHECK(auckland_june_max == doctest::Approx(29.7f).epsilon(0.02));
    auto [auckland_dec_min, auckland_dec_max] = MinMaxElevation(kWinterSolstice, kAuckland);
    CHECK(auckland_dec_max == doctest::Approx(76.6f).epsilon(0.01));

    // Straight above at the equator on the equinox
    auto [null_island_min, null_island_max] = MinMaxElevation(kEquinox, kNullIsland);
    CHECK(null_island_max == doctest::Approx(90).epsilon(0.01));
    CHECK(null_island_min == doctest::Approx(-90).epsilon(0.01));
}

TEST_CASE("the sun rises and sets at the equator on the equinox")
{
    // Day and night are 12 hours each, centered on noon (12:07 UTC with the equation of time)
    CHECK(SunElevation(kEquinox + 5 * kHour + 45 * kMinute, kNullIsland) < kSunriseElevation);
    CHECK(SunElevation(kEquinox + 6 * kHour + 30 * kMinute, kNullIsland) > kSunriseElevation);
    CHECK(SunElevation(kEquinox + 17 * kHour + 45 * kMinute, kNullIsland) > kSunriseElevation);
    CHECK(SunElevation(kEquinox + 18 * kHour + 30 * kMinute, kNullIsland) < kSunriseElevation);
}

TEST_CASE("the sun sets in Auckland on midsummer")
{
    // Sunset at 20:42 local summer time (UTC+13)
    CHECK(SunElevation(kWinterSolstice + 7 * kHour + 15 * kMinute, kAuckland) > kSunriseElevation);
    CHECK(SunElevation(kWinterSolstice + 8 * kHour + 15 * kMinute, kAuckland) < kSunriseElevation);
}

TEST_CASE("the sun rises and sets in Stockholm on midsummer")
{
    // Sunrise at 03:31, sunset at 22:08 (local summer time, UTC+2)
    CHECK(SunElevation(kSummerSolstice + 1 * kHour + 15 * kMinute, kStockholm) < kSunriseElevation);
    CHECK(SunElevation(kSummerSolstice + 1 * kHour + 45 * kMinute, kStockholm) > kSunriseElevation);
    CHECK(SunElevation(kSummerSolstice + 20 * kHour, kStockholm) > kSunriseElevation);
    CHECK(SunElevation(kSummerSolstice + 20 * kHour + 30 * kMinute, kStockholm) <
          kSunriseElevation);
}

TEST_CASE("the sun elevation is sane also for extreme input")
{
    for (auto utc : {0u, 1u, kSummerSolstice, std::numeric_limits<uint32_t>::max()})
    {
        for (auto position : {GpsPosition {90, 0},
                              GpsPosition {-90, 0},
                              GpsPosition {0, 180},
                              GpsPosition {0, -180},
                              kStockholm})
        {
            auto elevation = SunElevation(utc, position);
            CHECK(std::isfinite(elevation));
            CHECK(elevation >= -90);
            CHECK(elevation <= 90);
        }
    }

    // At the poles, the elevation is the declination
    CHECK(SunElevation(kSummerSolstice, GpsPosition {90, 0}) ==
          doctest::Approx(23.4f).epsilon(0.01));
    CHECK(SunElevation(kSummerSolstice, GpsPosition {-90, 0}) ==
          doctest::Approx(-23.4f).epsilon(0.01));
}

TEST_CASE("the auto level without a current level is taken from the elevation")
{
    CHECK(AutoBrightnessLevel(-20, true, std::nullopt) == BrightnessLevel::kNight);
    CHECK(AutoBrightnessLevel(-3, true, std::nullopt) == BrightnessLevel::kDusk);
    CHECK(AutoBrightnessLevel(5, false, std::nullopt) == BrightnessLevel::kLowSun);
    CHECK(AutoBrightnessLevel(45, false, std::nullopt) == BrightnessLevel::kDay);
}

TEST_CASE("the auto level only moves in the direction of the sun")
{
    GIVEN("a setting sun")
    {
        THEN("the level goes down")
        {
            CHECK(AutoBrightnessLevel(5, false, BrightnessLevel::kDay) == BrightnessLevel::kLowSun);
            CHECK(AutoBrightnessLevel(-10, false, BrightnessLevel::kDay) ==
                  BrightnessLevel::kNight);
        }
        AND_THEN("never up")
        {
            CHECK(AutoBrightnessLevel(45, false, BrightnessLevel::kNight) ==
                  BrightnessLevel::kNight);
            CHECK(AutoBrightnessLevel(-3, false, BrightnessLevel::kNight) ==
                  BrightnessLevel::kNight);
        }
    }

    GIVEN("a rising sun")
    {
        THEN("the level goes up")
        {
            CHECK(AutoBrightnessLevel(-3, true, BrightnessLevel::kNight) == BrightnessLevel::kDusk);
            CHECK(AutoBrightnessLevel(45, true, BrightnessLevel::kNight) == BrightnessLevel::kDay);
        }
        AND_THEN("never down")
        {
            CHECK(AutoBrightnessLevel(-20, true, BrightnessLevel::kDay) == BrightnessLevel::kDay);
            CHECK(AutoBrightnessLevel(5, true, BrightnessLevel::kDay) == BrightnessLevel::kDay);
        }
    }
}

TEST_CASE_FIXTURE(Fixture, "the manual brightness modes are constant")
{
    BrightnessController controller(display, state, kLevels, kStockholm);
    state.Set<AS::clock>(kWinterSolstice);

    state.Set<AS::brightness_mode>(BrightnessMode::kLow);
    {
        REQUIRE_CALL(display, SetBrightness(kLevels.dusk));
        CHECK(controller.OnActivation() == 1min);
    }

    state.Set<AS::brightness_mode>(BrightnessMode::kMedium);
    {
        REQUIRE_CALL(display, SetBrightness(kLevels.low_sun));
        controller.OnActivation();
    }

    state.Set<AS::brightness_mode>(BrightnessMode::kMax);
    REQUIRE_CALL(display, SetBrightness(kLevels.day));
    controller.OnActivation();
}

TEST_CASE_FIXTURE(Fixture, "auto brightness is max until the time is known")
{
    state.Set<AS::brightness_mode>(BrightnessMode::kAuto);
    BrightnessController controller(display, state, kLevels, kStockholm);

    {
        REQUIRE_CALL(display, SetBrightness(kLevels.day));
        controller.OnActivation();
    }

    // Midnight in Stockholm
    state.Set<AS::clock>(kWinterSolstice);
    REQUIRE_CALL(display, SetBrightness(kLevels.night));
    controller.OnActivation();
}

TEST_CASE_FIXTURE(Fixture, "auto brightness follows the sun through the day")
{
    state.Set<AS::brightness_mode>(BrightnessMode::kAuto);
    BrightnessController controller(display, state, kLevels, kStockholm);

    std::vector<uint8_t> changes;
    ALLOW_CALL(display, SetBrightness(_)).LR_SIDE_EFFECT({
        if (changes.empty() || changes.back() != _1)
        {
            changes.push_back(_1);
        }
    });

    // Noon to noon on midsummer in Stockholm, where the sun is just below -6° around midnight
    const auto noon = kSummerSolstice + 12 * kHour;
    for (auto t = noon; t < noon + 24 * kHour; t += kMinute)
    {
        state.Set<AS::clock>(t);
        controller.OnActivation();
    }

    // Down and up through each level only once
    CHECK(changes == std::vector<uint8_t> {kLevels.day,
                                           kLevels.low_sun,
                                           kLevels.dusk,
                                           kLevels.night,
                                           kLevels.dusk,
                                           kLevels.low_sun,
                                           kLevels.day});
}

TEST_CASE_FIXTURE(Fixture,
                  "auto brightness uses the fallback position, and the GPS position when known")
{
    state.Set<AS::brightness_mode>(BrightnessMode::kAuto);
    // 13:00 in New York, night in Stockholm
    state.Set<AS::clock>(kWinterSolstice + 18 * kHour);

    WHEN("there is no GPS fix")
    {
        BrightnessController controller(display, state, kLevels, kNewYork);

        REQUIRE_CALL(display, SetBrightness(kLevels.day));
        controller.OnActivation();
    }

    WHEN("there is a GPS fix")
    {
        SetGpsFix(kNewYork);
        BrightnessController controller(display, state, kLevels, kStockholm);

        REQUIRE_CALL(display, SetBrightness(kLevels.day));
        controller.OnActivation();

        AND_WHEN("the fix is lost")
        {
            state.Set<AS::gps_status>(GpsStatus::kNoFix);

            THEN("the last GPS position is still used")
            {
                REQUIRE_CALL(display, SetBrightness(kLevels.day));
                controller.OnActivation();
            }
        }
    }
}
