#include "lamp_state_machine_base.hh"
#include "test.hh"


namespace
{

class LampSwitch : public LampStateMachineBase<>
{
public:
    void TurnOn()
    {
        m_on = true;
    }

    void TurnOff()
    {
        m_on = false;
    }

    // Would be done in OnActivated for a thread
    void DoRunStateMachine()
    {
        RunStateMachine();
    }

private:
    Off::Next Evaluate(Off&) final
    {
        if (m_on)
        {
            return State::kOn;
        }

        return kStay;
    }

    On::Next Evaluate(On&) final
    {
        if (!m_on)
        {
            return State::kOff;
        }
        return kStay;
    }

    Dimmed::Next Evaluate(Dimmed&) final
    {
        return kStay;
    }

    Broken::Next Evaluate(Broken&) final
    {
        return kStay;
    }

    bool m_on {false};
};

} // namespace

TEST_CASE("LampSwitch turns on and off correctly")
{
    LampSwitch lamp;
    lamp.DoRunStateMachine();
    CHECK(lamp.CurrentState() == LampSwitch::State::kOff);

    lamp.TurnOn();
    lamp.DoRunStateMachine();
    CHECK(lamp.CurrentState() == LampSwitch::State::kOn);

    lamp.TurnOff();
    lamp.DoRunStateMachine();
    CHECK(lamp.CurrentState() == LampSwitch::State::kOff);
}
