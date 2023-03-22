/*
 * statemachine.h
 */

#ifndef STATEMACHINE_H_
#define STATEMACHINE_H_

#include <time.h>

class StateMachine {
public:
	StateMachine();

	void Run();

private:
	time_t m_nTime;
	uint8_t m_nIndexSensor { 0 };
};

#endif /* STATEMACHINE_H_ */
