/*
 * statemachine.cpp
 */

#ifdef NDEBUG
# undef NDEBUG
#endif

#include <cstdint>
#include <time.h>
#include <cassert>

#include "statemachine.h"
#include "rdmsensors.h"

#include "debug.h"

enum class State {
	WAITING,
	READING
};

static State s_State;

static constexpr time_t SAMPLE_TIME = 2; // Every 2 seconds

StateMachine::StateMachine(): m_nTime(time(nullptr)) {
	DEBUG_ENTRY
	DEBUG_PRINTF("Sensor count: %u", RDMSensors::Get()->GetCount());

	DEBUG_EXIT
}

void StateMachine::Run() {
	if (__builtin_expect((s_State == State::WAITING), 1)) {
		if (__builtin_expect(((time(nullptr) - m_nTime) < SAMPLE_TIME), 1)) {
			return;
		}
		s_State = State::READING;
	}

	if (s_State == State::READING) {
		const auto *pDefinition = RDMSensors::Get()->GetDefintion(m_nIndexSensor);
		const auto *pValues = RDMSensors::Get()->GetValues(m_nIndexSensor);

		DEBUG_PRINTF("Sensor=%u %.*s -> %d [%d:%d]", m_nIndexSensor, pDefinition->nLength, pDefinition->description, pValues->present,  pValues->lowest_detected,  pValues->highest_detected);

		m_nIndexSensor++;

		if (m_nIndexSensor >= RDMSensors::Get()->GetCount()) {
			m_nIndexSensor = 0;
			s_State = State::WAITING;
			m_nTime = time(nullptr);
			return;
		}

		return;
	}

	return;
}
