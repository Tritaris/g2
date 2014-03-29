/*
 * stepper.cpp - stepper motor controls
 * This file is part of the TinyG project
 *
 * Copyright (c) 2010 - 2014 Alden S. Hart, Jr.
 * Copyright (c) 2013 - 2014 Robert Giseburt
 *
 * This file ("the software") is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 as published by the
 * Free Software Foundation. You should have received a copy of the GNU General Public
 * License, version 2 along with the software.  If not, see <http://www.gnu.org/licenses/>.
 *
 * As a special exception, you may use this file as part of a software library without
 * restriction. Specifically, if other files instantiate templates or use macros or
 * inline functions from this file, or you compile this file and link it with  other
 * files to produce an executable, this file does not by itself cause the resulting
 * executable to be covered by the GNU General Public License. This exception does not
 * however invalidate any other reasons why the executable file might be covered by the
 * GNU General Public License.
 *
 * THE SOFTWARE IS DISTRIBUTED IN THE HOPE THAT IT WILL BE USEFUL, BUT WITHOUT ANY
 * WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT
 * SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/* 	This module provides the low-level stepper drivers and some related functions.
 *	See stepper.h for a detailed explanation of this module.
 */

#include "tinyg2.h"
#include "config.h"
#include "stepper.h"
#include "encoder.h"
#include "planner.h"
#include "hardware.h"
#include "text_parser.h"
#include "util.h"

/**** Allocate structures ****/

stConfig_t st_cfg;
stPrepSingleton_t st_pre;
static stRunSingleton_t st_run;

/**** Setup local functions ****/

static void _load_move(void);
static void _request_load_move(void);
static void _set_motor_power_level(const uint8_t motor, const float power_level);

// handy macro
#define _f_to_period(f) (uint16_t)((float)F_CPU / (float)f)

/**** Setup motate ****/

#ifdef __ARM
using namespace Motate;

OutputPin<kGRBL_CommonEnablePinNumber> common_enable;	 // shorter form of the above
OutputPin<kDebug1_PinNumber> dda_debug_pin1;
OutputPin<kDebug2_PinNumber> dda_debug_pin2;
OutputPin<kDebug3_PinNumber> dda_debug_pin3;

// Example with prefixed name::
//Motate::Timer<dda_timer_num> dda_timer(kTimerUpToMatch, FREQUENCY_DDA);// stepper pulse generation
Timer<dda_timer_num> dda_timer(kTimerUpToMatch, FREQUENCY_DDA);			// stepper pulse generation
Timer<dwell_timer_num> dwell_timer(kTimerUpToMatch, FREQUENCY_DWELL);	// dwell timer
Timer<load_timer_num> load_timer;		// triggers load of next stepper segment
Timer<exec_timer_num> exec_timer;		// triggers calculation of next+1 stepper segment

// Motor structures
template<pin_number step_num,			// Setup a stepper template to hold our pins
		 pin_number dir_num, 
		 pin_number enable_num, 
		 pin_number ms0_num, 
		 pin_number ms1_num, 
         pin_number ms2_num,
		 pin_number vref_num>

struct Stepper {
	/* stepper pin assignments */
	
	OutputPin<step_num> step;
	OutputPin<dir_num> dir;
	OutputPin<enable_num> enable;
	OutputPin<ms0_num> ms0;
	OutputPin<ms1_num> ms1;
	OutputPin<ms2_num> ms2;
	PWMOutputPin<vref_num> vref;

	/* stepper default values */

	// sets default pwm freq for all motor vrefs (comment line also sets HiZ)
	Stepper(const uint32_t frequency = 500000) : vref(frequency) {};
//	Stepper(const uint32_t frequency = 100000) : vref(kDriveLowOnly, frequency) {};

	/* functions bound to stepper structures */

	void setMicrosteps(const uint8_t microsteps)
	{
		switch (microsteps) {
			case ( 1): { ms2=0; ms1=0; ms0=0; break; }
			case ( 2): { ms2=0; ms1=0; ms0=1; break; }
			case ( 4): { ms2=0; ms1=1; ms0=0; break; }
			case ( 8): { ms2=0; ms1=1; ms0=1; break; }
			case (16): { ms2=1; ms1=0; ms0=0; break; }
			case (32): { ms2=1; ms1=0; ms0=1; break; }
		}
	};

	void energize(const uint8_t motor)
	{
		if (st_cfg.mot[motor].power_mode != MOTOR_DISABLED) {
			enable.clear();
			st_run.mot[motor].power_state = MOTOR_POWER_TIMEOUT_START;
		}
	};
};

Stepper<kSocket1_StepPinNumber,
		kSocket1_DirPinNumber,
		kSocket1_EnablePinNumber,
		kSocket1_Microstep_0PinNumber,
		kSocket1_Microstep_1PinNumber,
        kSocket1_Microstep_2PinNumber,
		kSocket1_VrefPinNumber> motor_1;

Stepper<kSocket2_StepPinNumber,
		kSocket2_DirPinNumber,
		kSocket2_EnablePinNumber,
		kSocket2_Microstep_0PinNumber,
		kSocket2_Microstep_1PinNumber,
        kSocket2_Microstep_2PinNumber,
		kSocket2_VrefPinNumber> motor_2;

Stepper<kSocket3_StepPinNumber,
		kSocket3_DirPinNumber,
		kSocket3_EnablePinNumber,
		kSocket3_Microstep_0PinNumber,
		kSocket3_Microstep_1PinNumber,
        kSocket3_Microstep_2PinNumber,
		kSocket3_VrefPinNumber> motor_3;

Stepper<kSocket4_StepPinNumber,
		kSocket4_DirPinNumber,
		kSocket4_EnablePinNumber,
		kSocket4_Microstep_0PinNumber,
		kSocket4_Microstep_1PinNumber,
        kSocket4_Microstep_2PinNumber,
		kSocket4_VrefPinNumber> motor_4;

Stepper<kSocket5_StepPinNumber,
		kSocket5_DirPinNumber,
		kSocket5_EnablePinNumber,
		kSocket5_Microstep_0PinNumber,
		kSocket5_Microstep_1PinNumber,
        kSocket5_Microstep_2PinNumber,
		kSocket5_VrefPinNumber> motor_5;
		
Stepper<kSocket6_StepPinNumber,
		kSocket6_DirPinNumber,
		kSocket6_EnablePinNumber,
		kSocket6_Microstep_0PinNumber,
		kSocket6_Microstep_1PinNumber,
        kSocket6_Microstep_2PinNumber,
		kSocket6_VrefPinNumber> motor_6;

#endif // __ARM

/************************************************************************************
 **** CODE **************************************************************************
 ************************************************************************************/
/*
 * stepper_init() - initialize stepper motor subsystem 
 *
 *	Notes:
 *	  - This init requires sys_init() to be run beforehand
 * 	  - microsteps are setup during config_init()
 *	  - motor polarity is setup during config_init()
 *	  - high level interrupts must be enabled in main() once all inits are complete
 */

void stepper_init()
{
	memset(&st_run, 0, sizeof(st_run));			// clear all values, pointers and status
	stepper_init_assertions();

	// setup DDA timer (see FOOTNOTE)
	dda_timer.setInterrupts(kInterruptOnOverflow | kInterruptOnMatchA | kInterruptPriorityHighest);
	dda_timer.setDutyCycleA(0.25);

	// setup DWELL timer
	dwell_timer.setInterrupts(kInterruptOnOverflow | kInterruptPriorityHighest);

	// setup software interrupt load timer
	load_timer.setInterrupts(kInterruptOnSoftwareTrigger | kInterruptPriorityLow);

	// setup software interrupt exec timer & initial condition
	exec_timer.setInterrupts(kInterruptOnSoftwareTrigger | kInterruptPriorityLowest);
	st_pre.exec_state = PREP_BUFFER_OWNED_BY_EXEC;
	st_pre.segment_ready = false;						// used for diagnostics only

	// setup motor power levels and apply power level to stepper drivers
	for (uint8_t motor=0; motor<MOTORS; motor++) {
		_set_motor_power_level(motor, st_cfg.mot[motor].power_level_scaled);
		st_run.mot[motor].power_level_dynamic = st_cfg.mot[motor].power_level_scaled;
	}
//	motor_1.vref = 0.25; // example of how to set vref duty cycle directly. Freq already set to 500000 Hz.
}

/*	FOOTNOTE: This is the bare code that the Motate timer calls replace.
	NB: requires: #include <component_tc.h>

	REG_TC1_WPMR = 0x54494D00;			// enable write to registers
	TC_Configure(TC_BLOCK_DDA, TC_CHANNEL_DDA, TC_CMR_DDA);
	REG_RC_DDA = TC_RC_DDA;				// set frequency
	REG_IER_DDA = TC_IER_DDA;			// enable interrupts
	NVIC_EnableIRQ(TC_IRQn_DDA);
	pmc_enable_periph_clk(TC_ID_DDA);
	TC_Start(TC_BLOCK_DDA, TC_CHANNEL_DDA);
*/

/*
 * stepper_init_assertions() - test assertions, return error code if violation exists
 * stepper_test_assertions() - test assertions, return error code if violation exists
 */

void stepper_init_assertions()
{
	st_run.magic_end = MAGICNUM;
	st_run.magic_start = MAGICNUM;
	st_pre.magic_end = MAGICNUM;
	st_pre.magic_start = MAGICNUM;
}

stat_t stepper_test_assertions()
{
	if (st_run.magic_end	!= MAGICNUM) return (STAT_STEPPER_ASSERTION_FAILURE);
	if (st_run.magic_start	!= MAGICNUM) return (STAT_STEPPER_ASSERTION_FAILURE);
	if (st_pre.magic_end	!= MAGICNUM) return (STAT_STEPPER_ASSERTION_FAILURE);
	if (st_pre.magic_start	!= MAGICNUM) return (STAT_STEPPER_ASSERTION_FAILURE);
	return (STAT_OK);
}

/*
 * stepper_isbusy() - return TRUE if motors are running or a dwell is running
 */
uint8_t stepper_isbusy()
{
	if (st_run.dda_ticks_downcount == 0) {
		return (false);
	} 
	return (true);
}

/*
 * st_reset() - reset stepper internals
 */

void st_reset()
{
	float zero[] = {0,0,0,0,0,0};
	mp_set_step_counts(zero);

//	en_set_encoders(zero);
//	mp_reset_step_counts();						// step counters are in motor space: resets all step counters

	for (uint8_t motor=0; motor<MOTORS; motor++) {
		st_pre.mot[motor].prev_direction = STEP_INITIAL_DIRECTION;
		st_run.mot[motor].substep_accumulator = 0;	// will become max negative during per-motor setup;
		st_pre.mot[motor].corrected_steps = 0;
	}
}

stat_t st_clc(cmdObj_t *cmd)	// clear diagnostic counters, reset stepper prep
{
	st_reset();
	return(STAT_OK);
}

/*
 * Motor power management functions
 *
 * _energize_motor()		 - apply power to a motor
 * _deenergize_motor()		 - remove power from a motor
 * _set_motor_power_level()	 - set the actual Vref to a specified power level
 *
 * st_energize_motors()		 - apply power to all motors
 * st_deenergize_motors()	 - remove power from all motors
 * st_motor_power_callback() - callback to manage motor power sequencing
 */

static void _energize_motor(const uint8_t motor)
{
#ifdef __AVR
	switch(motor) {
		case (MOTOR_1): { PORT_MOTOR_1_VPORT.OUT &= ~MOTOR_ENABLE_BIT_bm; break; }
		case (MOTOR_2): { PORT_MOTOR_2_VPORT.OUT &= ~MOTOR_ENABLE_BIT_bm; break; }
		case (MOTOR_3): { PORT_MOTOR_3_VPORT.OUT &= ~MOTOR_ENABLE_BIT_bm; break; }
		case (MOTOR_4): { PORT_MOTOR_4_VPORT.OUT &= ~MOTOR_ENABLE_BIT_bm; break; }
	}
	st_run.mot[motor].power_state = MOTOR_POWER_TIMEOUT_START;
#endif
#ifdef __ARM
	// Motors that are not defined are not compiled. Saves some ugly #ifdef code
	//	case (MOTOR_1): { motor_1.energize(MOTOR_1); break; }
	if (!motor_1.enable.isNull()) if (motor == MOTOR_1) motor_1.energize(MOTOR_1);
	if (!motor_2.enable.isNull()) if (motor == MOTOR_2) motor_2.energize(MOTOR_2);
	if (!motor_3.enable.isNull()) if (motor == MOTOR_3) motor_3.energize(MOTOR_3);
	if (!motor_4.enable.isNull()) if (motor == MOTOR_4) motor_4.energize(MOTOR_4);
	if (!motor_5.enable.isNull()) if (motor == MOTOR_5) motor_5.energize(MOTOR_5);
	if (!motor_6.enable.isNull()) if (motor == MOTOR_6) motor_6.energize(MOTOR_6);
#endif
}

static void _deenergize_motor(const uint8_t motor)
{
#ifdef __AVR
	switch (motor) {
		case (MOTOR_1): { PORT_MOTOR_1_VPORT.OUT |= MOTOR_ENABLE_BIT_bm; break; }
		case (MOTOR_2): { PORT_MOTOR_2_VPORT.OUT |= MOTOR_ENABLE_BIT_bm; break; }
		case (MOTOR_3): { PORT_MOTOR_3_VPORT.OUT |= MOTOR_ENABLE_BIT_bm; break; }
		case (MOTOR_4): { PORT_MOTOR_4_VPORT.OUT |= MOTOR_ENABLE_BIT_bm; break; }
	}
	st_run.mot[motor].power_state = MOTOR_OFF;
#endif
#ifdef __ARM
	// Motors that are not defined are not compiled. Saves some ugly #ifdef code
	if (!motor_1.enable.isNull()) if (motor == MOTOR_1) motor_1.enable.set();	// set disables the motor
	if (!motor_2.enable.isNull()) if (motor == MOTOR_2) motor_2.enable.set();
	if (!motor_3.enable.isNull()) if (motor == MOTOR_3) motor_3.enable.set();
	if (!motor_4.enable.isNull()) if (motor == MOTOR_4) motor_4.enable.set();
	if (!motor_5.enable.isNull()) if (motor == MOTOR_5) motor_5.enable.set();
	if (!motor_6.enable.isNull()) if (motor == MOTOR_6) motor_6.enable.set();
	st_run.mot[motor].power_state = MOTOR_OFF;
#endif
}

/*
 * _set_motor_power_level()	- applies the power level to the requested motor.
 *
 *	The power_level must be a compensated PWM value - presumably one of:
 *		st_cfg.mot[motor].power_level_scaled 
 *		st_run.mot[motor].power_level_dynamic
 */
static void _set_motor_power_level(const uint8_t motor, const float power_level)
{
#ifdef __ARM
	// power_level must be scaled properly for the driver's Vref voltage requirements 
	if (!motor_1.enable.isNull()) if (motor == MOTOR_1) motor_1.vref = power_level;
	if (!motor_2.enable.isNull()) if (motor == MOTOR_2) motor_2.vref = power_level;
	if (!motor_3.enable.isNull()) if (motor == MOTOR_3) motor_3.vref = power_level;
	if (!motor_4.enable.isNull()) if (motor == MOTOR_4) motor_4.vref = power_level;
	if (!motor_5.enable.isNull()) if (motor == MOTOR_5) motor_5.vref = power_level;
	if (!motor_6.enable.isNull()) if (motor == MOTOR_6) motor_6.vref = power_level;
#endif
}

void st_energize_motors()
{
	for (uint8_t motor = MOTOR_1; motor < MOTORS; motor++) {
		if (st_cfg.mot[motor].power_mode != MOTOR_DISABLED)
			_energize_motor(motor);
		st_run.mot[motor].power_state = MOTOR_POWER_TIMEOUT_START;
	}
#ifdef __ARM
	common_enable.clear();			// enable gShield common enable
#endif
}

void st_deenergize_motors()
{
	for (uint8_t motor = MOTOR_1; motor < MOTORS; motor++) {
		_deenergize_motor(motor);
	}
#ifdef __ARM
	common_enable.set();			// disable gShield common enable
#endif
}

/*
 * st_motor_power_callback() - callback to manage motor power sequencing
 *
 *	Handles motor power-down timing, low-power idle, and adaptive motor power
 */
stat_t st_motor_power_callback() 	// called by controller
{
	// manage power for each motor individually
	for (uint8_t motor=MOTOR_1; motor<MOTORS; motor++) {

		switch (st_cfg.mot[motor].power_mode) {
//			case (MOTOR_DISABLED): { _deenergize_motor(motor); break;}
			case (MOTOR_ALWAYS_POWERED): { break;}
			case (MOTOR_POWERED_IN_CYCLE):
			case (MOTOR_POWERED_ONLY_WHEN_MOVING): {
				switch (st_run.mot[motor].power_state) {
					case (MOTOR_POWER_TIMEOUT_START): {
						st_run.mot[motor].power_systick = SysTickTimer_getValue() + (uint32_t)(st_cfg.motor_power_timeout * 1000);
						st_run.mot[motor].power_state = MOTOR_POWER_TIMEOUT_COUNTDOWN;
//						printf ("%lu ",st_run.mot[motor].power_systick);		//+++++ DIAGNOSTIC
						break;
					}
					case (MOTOR_POWER_TIMEOUT_COUNTDOWN): {
						if (SysTickTimer_getValue() > st_run.mot[motor].power_systick ) {
							st_run.mot[motor].power_state = MOTOR_IDLE;
							_deenergize_motor(motor);
						}
					}
					break;
				}
			}
		}
	}
	return (STAT_OK);
}

/******************************
 * Interrupt Service Routines *
 ******************************/

/*
 * Dwell timer interrupt
 */

#ifdef __AVR
ISR(TIMER_DWELL_ISR_vect) {								// DWELL timer interrupt
	if (--st_run.dda_ticks_downcount == 0) {
		TIMER_DWELL.CTRLA = STEP_TIMER_DISABLE;			// disable DWELL timer
//		mp_end_dwell();									// free the planner buffer
		_load_move();
	}
}
#endif
#ifdef __ARM
namespace Motate {			// Must define timer interrupts inside the Motate namespace
MOTATE_TIMER_INTERRUPT(dwell_timer_num)
{
	dwell_timer.getInterruptCause(); // read SR to clear interrupt condition
	if (--st_run.dda_ticks_downcount == 0) {
		dwell_timer.stop();
		_load_move();
	}
}
} // namespace Motate
#endif

/****************************************************************************************
 * ISR - DDA timer interrupt routine - service ticks from DDA timer
 *
 *	This interrupt is really 2 interrupts. It fires on timer overflow and also on match.
 *	Overflow interrupts are used to set step pins, match interrupts clear step pins.
 *	This way the duty cycle of the stepper pulse can be controlled by setting the match value.
 *
 *	Note that the motor_N.step.isNull() tests are compile-time tests, not run-time tests. 
 *	If motor_N is not defined that if{} clause (i.e. that motor) drops out of the complied code.
 */
namespace Motate {			// Must define timer interrupts inside the Motate namespace
MOTATE_TIMER_INTERRUPT(dda_timer_num)
{
//    dda_debug_pin1 = 1;
	uint32_t interrupt_cause = dda_timer.getInterruptCause();	// also clears interrupt condition

	if (interrupt_cause == kInterruptOnOverflow) {

		if (!motor_1.step.isNull() && (st_run.mot[MOTOR_1].substep_accumulator += st_run.mot[MOTOR_1].substep_increment) > 0) {
			motor_1.step.set();		// turn step bit on
			st_run.mot[MOTOR_1].substep_accumulator -= st_run.dda_ticks_X_substeps;
			INCREMENT_ENCODER(MOTOR_1);
		}
		if (!motor_2.step.isNull() && (st_run.mot[MOTOR_2].substep_accumulator += st_run.mot[MOTOR_2].substep_increment) > 0) {
			motor_2.step.set();
			st_run.mot[MOTOR_2].substep_accumulator -= st_run.dda_ticks_X_substeps;
			INCREMENT_ENCODER(MOTOR_2);
		}
		if (!motor_3.step.isNull() && (st_run.mot[MOTOR_3].substep_accumulator += st_run.mot[MOTOR_3].substep_increment) > 0) {
			motor_3.step.set();
			st_run.mot[MOTOR_3].substep_accumulator -= st_run.dda_ticks_X_substeps;
			INCREMENT_ENCODER(MOTOR_3);
		}
		if (!motor_4.step.isNull() && (st_run.mot[MOTOR_4].substep_accumulator += st_run.mot[MOTOR_4].substep_increment) > 0) {
			motor_4.step.set();
			st_run.mot[MOTOR_4].substep_accumulator -= st_run.dda_ticks_X_substeps;
			INCREMENT_ENCODER(MOTOR_4);
		}
		if (!motor_5.step.isNull() && (st_run.mot[MOTOR_5].substep_accumulator += st_run.mot[MOTOR_5].substep_increment) > 0) {
			motor_5.step.set();
			st_run.mot[MOTOR_5].substep_accumulator -= st_run.dda_ticks_X_substeps;
			INCREMENT_ENCODER(MOTOR_5);
		}
		if (!motor_6.step.isNull() && (st_run.mot[MOTOR_6].substep_accumulator += st_run.mot[MOTOR_6].substep_increment) > 0) {
			motor_6.step.set();
			st_run.mot[MOTOR_6].substep_accumulator -= st_run.dda_ticks_X_substeps;
			INCREMENT_ENCODER(MOTOR_6);
		}

	} else if (interrupt_cause == kInterruptOnMatchA) {
//		dda_debug_pin2 = 1;
		motor_1.step.clear();							// turn step bits off
		motor_2.step.clear();
		motor_3.step.clear();
		motor_4.step.clear();
		motor_5.step.clear();
		motor_6.step.clear();

		if (--st_run.dda_ticks_downcount != 0) return;

		// process end of segment
		dda_timer.stop();								// turn it off or it will keep stepping out the last segment
		_load_move();									// load the next move at the current interrupt level
//		dda_debug_pin2 = 0;
	}
//    dda_debug_pin1 = 0;
} // MOTATE_TIMER_INTERRUPT
} // namespace Motate

/****************************************************************************************
 * Exec sequencing code		- computes and prepares next load segment
 * st_request_exec_move()	- SW interrupt to request to execute a move
 * exec_timer interrupt		- interrupt handler for calling exec function
 */

#ifdef __AVR
void st_request_exec_move()
{
	if (st_pre.exec_state == PREP_BUFFER_OWNED_BY_EXEC) {	// bother interrupting
		TIMER_EXEC.PER = EXEC_TIMER_PERIOD;
		TIMER_EXEC.CTRLA = EXEC_TIMER_ENABLE;				// trigger a LO interrupt
	}
}

ISR(TIMER_EXEC_ISR_vect) {								// exec move SW interrupt
	TIMER_EXEC.CTRLA = EXEC_TIMER_DISABLE;				// disable SW interrupt timer

	// exec_move
	if (st_pre.exec_state == PREP_BUFFER_OWNED_BY_EXEC) {
		if (mp_exec_move() != STAT_NOOP) {
			st_pre.exec_state = PREP_BUFFER_OWNED_BY_LOADER; // flip it back
			_request_load_move();
		}
	}
}
#endif // __AVR

#ifdef __ARM
void st_request_exec_move()
{
	if (st_pre.exec_state == PREP_BUFFER_OWNED_BY_EXEC) {	// bother interrupting
		exec_timer.setInterruptPending();
	}
}

namespace Motate {	// Define timer inside Motate namespace
	MOTATE_TIMER_INTERRUPT(exec_timer_num)			// exec move SW interrupt
	{
		exec_timer.getInterruptCause();				// clears the interrupt condition
		if (st_pre.exec_state == PREP_BUFFER_OWNED_BY_EXEC) {
			if (mp_exec_move() != STAT_NOOP) {
				st_pre.exec_state = PREP_BUFFER_OWNED_BY_LOADER; // flip it back
				_request_load_move();
			}
		}
	}
} // namespace Motate

#endif // __ARM

/****************************************************************************************
 * Loader sequencing code
 *
 * _request_load()		- fires a software interrupt (timer) to request to load a move
 *  load_mode interrupt	- interrupt handler for running the loader
 */

#ifdef __AVR
static void _request_load_move()
{
	if (st_run.dda_ticks_downcount == 0) {				// bother interrupting
		TIMER_LOAD.PER = LOAD_TIMER_PERIOD;
		TIMER_LOAD.CTRLA = LOAD_TIMER_ENABLE;			// trigger a HI interrupt
	} 	// else don't bother to interrupt. You'll just trigger an
		// interrupt and find out the load routine is not ready for you
}

ISR(TIMER_LOAD_ISR_vect) {								// load steppers SW interrupt
	TIMER_LOAD.CTRLA = LOAD_TIMER_DISABLE;				// disable SW interrupt timer
	_load_move();
}
#endif // __AVR

#ifdef __ARM
static void _request_load_move()
{
	if (st_run.dda_ticks_downcount == 0) {				// bother interrupting
		load_timer.setInterruptPending();
	} 	// ...else don't bother to interrupt.
		// You'll just trigger an interrupt and find out the loader is not ready
}

namespace Motate {	// Define timer inside Motate namespace
	MOTATE_TIMER_INTERRUPT(load_timer_num)				// load steppers SW interrupt
	{
		load_timer.getInterruptCause();					// read SR to clear interrupt condition
		_load_move();
	}
} // namespace Motate
#endif // __ARM

/****************************************************************************************
 * _load_move() - Dequeue move and load into stepper struct
 *
 *	This routine can only be called be called from an ISR at the same or 
 *	higher level as the DDA or dwell ISR. A software interrupt has been 
 *	provided to allow a non-ISR to request a load (see st_request_load_move())
 *
 *	In aline() code:
 *	 - All axes must set steps and compensate for out-of-range pulse phasing.
 *	 - If axis has 0 steps the direction setting can be omitted
 *	 - If axis has 0 steps the motor must not be enabled to support power mode = 1
 */

static void _load_move()
{
	// Be aware that dda_ticks_downcount must equal zero for the loader to run.
	// So the initial load must also have this set to zero as part of initialization
	if (st_run.dda_ticks_downcount != 0) return;						// exit if it's still busy

	if (st_pre.exec_state != PREP_BUFFER_OWNED_BY_LOADER) {				// if there are no moves to load...
		for (uint8_t motor = MOTOR_1; motor < MOTORS; motor++) {
			st_run.mot[motor].power_state = MOTOR_POWER_TIMEOUT_START;	// ...start motor power timeouts
		}
		return;
	}

	if (st_pre.segment_ready != true) {									// trap if prep is not complete
		printf("######## LOADER - SEGMENT NOT READY\n");
	}
	st_pre.segment_ready = false;

	// handle aline() loads first (most common case)  NB: there are no more lines, only alines()
	if (st_pre.move_type == MOVE_TYPE_ALINE) {

		//**** setup the new segment ****

		st_run.dda_ticks_downcount = st_pre.dda_ticks;
		st_run.dda_ticks_X_substeps = st_pre.dda_ticks_X_substeps;
#ifdef __AVR
		TIMER_DDA.PER = st_pre.dda_period;
#endif
		//**** MOTOR_1 LOAD ****

		// These sections are somewhat optimized for execution speed. The whole load operation
		// is supposed to take < 10 uSec (Xmega). Be careful if you mess with this.

		// the following if() statement sets the runtime substep increment value or zeroes it

		if ((st_run.mot[MOTOR_1].substep_increment = st_pre.mot[MOTOR_1].substep_increment) != 0) {

			// NB: If motor has 0 steps the following is all skipped. This ensures that state comparisons
			//	   always operate on the last segment actually run by this motor, regardless of how many
			//	   segments it may have been inactive in between.

			// Apply accumulator correction if the time base has changed since previous segment
			if (st_pre.mot[MOTOR_1].accumulator_correction_flag == true) {
				st_pre.mot[MOTOR_1].accumulator_correction_flag = false;
				st_run.mot[MOTOR_1].substep_accumulator *= st_pre.mot[MOTOR_1].accumulator_correction;
			}

			// Detect direction change and if so:
			//	- Set the direction bit in hardware.
			//	- Compensate for direction change by flipping substep accumulator value about its midpoint.

			if (st_pre.mot[MOTOR_1].direction != st_pre.mot[MOTOR_1].prev_direction) {
				st_pre.mot[MOTOR_1].prev_direction = st_pre.mot[MOTOR_1].direction;
				st_run.mot[MOTOR_1].substep_accumulator = -(st_run.dda_ticks_X_substeps + st_run.mot[MOTOR_1].substep_accumulator);
				st_pre.mot[MOTOR_1].direction == DIRECTION_CW ? motor_1.dir.clear() : motor_1.dir.set(); // set the bit for CCW motion
			}

			// Enable the stepper and start motor power management
			motor_1.enable.clear();										// enable the motor (clear the ~Enable line)
			st_run.mot[MOTOR_1].power_state = MOTOR_RUNNING;
			SET_ENCODER_STEP_SIGN(MOTOR_1, st_pre.mot[MOTOR_1].step_sign);

		} else {  // Motor has 0 steps; might need to energize motor for power mode processing
			if (st_cfg.mot[MOTOR_1].power_mode == MOTOR_POWERED_ONLY_WHEN_MOVING) {
				motor_1.enable.clear();									// energize motor
				st_run.mot[MOTOR_1].power_state = MOTOR_POWER_TIMEOUT_START;
			}
		}
		// accumulate counted steps to the step position and zero out counted steps for the segment currently being loaded
		ACCUMULATE_ENCODER(MOTOR_1);

#if (MOTORS >= 2)
		if ((st_run.mot[MOTOR_2].substep_increment = st_pre.mot[MOTOR_2].substep_increment) != 0) {
			if (st_pre.mot[MOTOR_2].accumulator_correction_flag == true) {
				st_pre.mot[MOTOR_2].accumulator_correction_flag = false;
				st_run.mot[MOTOR_2].substep_accumulator *= st_pre.mot[MOTOR_2].accumulator_correction;
			}
			if (st_pre.mot[MOTOR_2].direction != st_pre.mot[MOTOR_2].prev_direction) {
				st_pre.mot[MOTOR_2].prev_direction = st_pre.mot[MOTOR_2].direction;
				st_run.mot[MOTOR_2].substep_accumulator = -(st_run.dda_ticks_X_substeps + st_run.mot[MOTOR_2].substep_accumulator);
				st_pre.mot[MOTOR_2].direction == DIRECTION_CW ? motor_2.dir.clear() : motor_2.dir.set(); // set the bit for CCW motion
			}
			motor_2.enable.clear(); st_run.mot[MOTOR_2].power_state = MOTOR_RUNNING;
			SET_ENCODER_STEP_SIGN(MOTOR_2, st_pre.mot[MOTOR_2].step_sign);
		} else if (st_cfg.mot[MOTOR_2].power_mode == MOTOR_POWERED_ONLY_WHEN_MOVING) {
			motor_2.enable.clear(); st_run.mot[MOTOR_2].power_state = MOTOR_POWER_TIMEOUT_START;
		}
		ACCUMULATE_ENCODER(MOTOR_2);
#endif
#if (MOTORS >= 3)
		if ((st_run.mot[MOTOR_3].substep_increment = st_pre.mot[MOTOR_3].substep_increment) != 0) {
			if (st_pre.mot[MOTOR_3].accumulator_correction_flag == true) {
				st_pre.mot[MOTOR_3].accumulator_correction_flag = false;
				st_run.mot[MOTOR_3].substep_accumulator *= st_pre.mot[MOTOR_3].accumulator_correction;
			}
			if (st_pre.mot[MOTOR_3].direction != st_pre.mot[MOTOR_3].prev_direction) {
				st_pre.mot[MOTOR_3].prev_direction = st_pre.mot[MOTOR_3].direction;
				st_run.mot[MOTOR_3].substep_accumulator = -(st_run.dda_ticks_X_substeps + st_run.mot[MOTOR_3].substep_accumulator);
				st_pre.mot[MOTOR_3].direction == DIRECTION_CW ? motor_3.dir.clear() : motor_3.dir.set(); // set the bit for CCW motion
			}
			motor_3.enable.clear(); st_run.mot[MOTOR_3].power_state = MOTOR_RUNNING;
			SET_ENCODER_STEP_SIGN(MOTOR_3, st_pre.mot[MOTOR_3].step_sign);
		} else if (st_cfg.mot[MOTOR_3].power_mode == MOTOR_POWERED_ONLY_WHEN_MOVING) {
			motor_3.enable.clear(); st_run.mot[MOTOR_3].power_state = MOTOR_POWER_TIMEOUT_START;
		}
		ACCUMULATE_ENCODER(MOTOR_3);
#endif
#if (MOTORS >= 4)
		if ((st_run.mot[MOTOR_4].substep_increment = st_pre.mot[MOTOR_4].substep_increment) != 0) {
			if (st_pre.mot[MOTOR_4].accumulator_correction_flag == true) {
				st_pre.mot[MOTOR_4].accumulator_correction_flag = false;
				st_run.mot[MOTOR_4].substep_accumulator *= st_pre.mot[MOTOR_4].accumulator_correction;
			}
			if (st_pre.mot[MOTOR_4].direction != st_pre.mot[MOTOR_4].prev_direction) {
				st_pre.mot[MOTOR_4].prev_direction = st_pre.mot[MOTOR_4].direction;
				st_run.mot[MOTOR_4].substep_accumulator = -(st_run.dda_ticks_X_substeps + st_run.mot[MOTOR_4].substep_accumulator);
				st_pre.mot[MOTOR_4].direction == DIRECTION_CW ? motor_4.dir.clear() : motor_4.dir.set(); // set the bit for CCW motion
			}
			motor_4.enable.clear(); st_run.mot[MOTOR_4].power_state = MOTOR_RUNNING;
			SET_ENCODER_STEP_SIGN(MOTOR_4, st_pre.mot[MOTOR_4].step_sign);
		} else if (st_cfg.mot[MOTOR_4].power_mode == MOTOR_POWERED_ONLY_WHEN_MOVING) {
			motor_4.enable.clear(); st_run.mot[MOTOR_4].power_state = MOTOR_POWER_TIMEOUT_START;
		}
		ACCUMULATE_ENCODER(MOTOR_4);
#endif
#if (MOTORS >= 5)
		if ((st_run.mot[MOTOR_5].substep_increment = st_pre.mot[MOTOR_5].substep_increment) != 0) {
			if (st_pre.mot[MOTOR_5].accumulator_correction_flag == true) {
				st_pre.mot[MOTOR_5].accumulator_correction_flag = false;
				st_run.mot[MOTOR_5].substep_accumulator *= st_pre.mot[MOTOR_5].accumulator_correction;
			}
			if (st_pre.mot[MOTOR_5].direction != st_pre.mot[MOTOR_5].prev_direction) {
				st_pre.mot[MOTOR_5].prev_direction = st_pre.mot[MOTOR_5].direction;
				st_run.mot[MOTOR_5].substep_accumulator = -(st_run.dda_ticks_X_substeps + st_run.mot[MOTOR_5].substep_accumulator);
				st_pre.mot[MOTOR_5].direction == DIRECTION_CW ? motor_5.dir.clear() : motor_5.dir.set(); // set the bit for CCW motion
			}
			motor_5.enable.clear(); st_run.mot[MOTOR_5].power_state = MOTOR_RUNNING;
			SET_ENCODER_STEP_SIGN(MOTOR_5, st_pre.mot[MOTOR_5].step_sign);
		} else if (st_cfg.mot[MOTOR_5].power_mode == MOTOR_POWERED_ONLY_WHEN_MOVING) {
			motor_5.enable.clear(); st_run.mot[MOTOR_5].power_state = MOTOR_POWER_TIMEOUT_START;
		}
		ACCUMULATE_ENCODER(MOTOR_5);
#endif
#if (MOTORS >= 6)
		if ((st_run.mot[MOTOR_6].substep_increment = st_pre.mot[MOTOR_6].substep_increment) != 0) {
			if (st_pre.mot[MOTOR_6].accumulator_correction_flag == true) {
				st_pre.mot[MOTOR_6].accumulator_correction_flag = false;
				st_run.mot[MOTOR_6].substep_accumulator *= st_pre.mot[MOTOR_6].accumulator_correction;
			}
			if (st_pre.mot[MOTOR_6].direction != st_pre.mot[MOTOR_6].prev_direction) {
				st_pre.mot[MOTOR_6].prev_direction = st_pre.mot[MOTOR_6].direction;
				st_run.mot[MOTOR_6].substep_accumulator = -(st_run.dda_ticks_X_substeps + st_run.mot[MOTOR_6].substep_accumulator);
				st_pre.mot[MOTOR_6].direction == DIRECTION_CW ? motor_6.dir.clear() : motor_6.dir.set(); // set the bit for CCW motion
			}
			motor_6.enable.clear(); st_run.mot[MOTOR_6].power_state = MOTOR_RUNNING;
			SET_ENCODER_STEP_SIGN(MOTOR_6, st_pre.mot[MOTOR_6].step_sign);
		} else if (st_cfg.mot[MOTOR_6].power_mode == MOTOR_POWERED_ONLY_WHEN_MOVING) {
			motor_6.enable.clear(); st_run.mot[MOTOR_6].power_state = MOTOR_POWER_TIMEOUT_START;
		}
		ACCUMULATE_ENCODER(MOTOR_6);
#endif

		//**** do this last ****

		dda_timer.start();									// start the DDA timer if not already running

	// handle dwells
	} else if (st_pre.move_type == MOVE_TYPE_DWELL) {
		st_run.dda_ticks_downcount = st_pre.dda_ticks;
		dwell_timer.start();
	}

	// all cases drop to here - such as Null moves queued by MCodes
	st_prep_null();											// needed to shut off timers if no moves left
	st_pre.exec_state = PREP_BUFFER_OWNED_BY_EXEC;			// flip it back
	st_request_exec_move();									// exec and prep next move
//dda_debug_pin1 = 0;
}

/***********************************************************************************
 * st_prep_line() - Prepare the next move for the loader
 *
 *	This function does the math on the next pulse segment and gets it ready for the loader. 
 *	It deals with all the DDA optimizations and timer setups so that loading can be performed 
 *	as rapidly as possible. It works in joint space (motors) and it works in steps, not length 
 *	units. All args are provided as floats and converted to their appropriate integer types for the loader. 
 *
 * Args:
 *	  - travel_steps[] are signed relative motion in steps for each motor. Steps are floats
 *		that typically have fractional values (fractional steps). The sign indicates
 *		direction. Motors that are not in the move should be 0 steps on input.
 *
 *	  - following_error[] is a vector of measured errors to the step count. Used for correction.
 *
 *	  - segment_time - how many minutes the segment should run. If timing is not
 *		100% accurate this will affect the move velocity, but not the distance traveled.
 *
 * NOTE:  Many of the expressions are sensitive to casting and execution order to avoid long-term
 *		  accuracy errors due to floating point round off. One earlier failed attempt was:
 *		    dda_ticks_X_substeps = (uint32_t)((microseconds/1000000) * f_dda * dda_substeps);
 */

stat_t st_prep_line(float travel_steps[], float following_error[], float segment_time)
{
	// trap conditions that would prevent queueing the line
	if (st_pre.exec_state != PREP_BUFFER_OWNED_BY_EXEC) { return (cm_hard_alarm(STAT_INTERNAL_ERROR));	// never supposed to happen
	} else if (isinf(segment_time)) { return (cm_hard_alarm(STAT_PREP_LINE_MOVE_TIME_IS_INFINITE));		// ever supposed to happen
	} else if (isnan(segment_time)) { return (cm_hard_alarm(STAT_PREP_LINE_MOVE_TIME_IS_NAN));			// ever supposed to happen
	}

	// setup segment parameters
	// - dda_ticks is the integer number of DDA clock ticks needed to play out the segment
	// - ticks_X_substeps is the maximum depth of the DDA accumulator (as a negative number)

	st_pre.dda_period = _f_to_period(FREQUENCY_DDA);				// NB: AVR only (non Motate)
	st_pre.dda_ticks = (int32_t)(segment_time * 60 * FREQUENCY_DDA);// NB: converts minutes to seconds
	st_pre.dda_ticks_X_substeps = st_pre.dda_ticks * DDA_SUBSTEPS;

	// setup motor parameters

	float correction_steps;
	for (uint8_t motor=0; motor<MOTORS; motor++) {	// I want to remind myself that this is motors, not axes

		// Skip this motor if there are no new steps. Leave all other values intact.
		if (fp_ZERO(travel_steps[motor])) { st_pre.mot[motor].substep_increment = 0; continue;}

		// Setup the direction, compensating for polarity.
		// Set the step_sign which is used by the stepper ISR to accumulate step position

		if (travel_steps[motor] >= 0) {					// positive direction
			st_pre.mot[motor].direction = DIRECTION_CW ^ st_cfg.mot[motor].polarity;
			st_pre.mot[motor].step_sign = 1;
		} else {
			st_pre.mot[motor].direction = DIRECTION_CCW ^ st_cfg.mot[motor].polarity;
			st_pre.mot[motor].step_sign = -1;
		}

		// Detect segment time changes and setup the accumulator correction factor and flag.
		// Putting this here computes the correct factor even if the motor was dormant for some
		// number of previous moves. Correction is computed based on the last segment time actually used.

		if (fabs(segment_time - st_pre.mot[motor].prev_segment_time) > 0.0000001) { // highly tuned FP != compare
			if (fp_NOT_ZERO(st_pre.mot[motor].prev_segment_time)) {					// special case to skip first move
				st_pre.mot[motor].accumulator_correction_flag = true;
				st_pre.mot[motor].accumulator_correction = segment_time / st_pre.mot[motor].prev_segment_time;
			}
			st_pre.mot[motor].prev_segment_time = segment_time;
		}

#ifdef __STEP_CORRECTION
		// 'Nudge' correction strategy. Inject a single, scaled correction value then hold off

		if ((--st_pre.mot[motor].correction_holdoff < 0) &&
			(fabs(following_error[motor]) > STEP_CORRECTION_THRESHOLD)) {

			st_pre.mot[motor].correction_holdoff = STEP_CORRECTION_HOLDOFF;
			correction_steps = following_error[motor] * STEP_CORRECTION_FACTOR;

			if (correction_steps > 0) {
				correction_steps = min3(correction_steps, fabs(travel_steps[motor]), STEP_CORRECTION_MAX);
			} else {
				correction_steps = max3(correction_steps, -fabs(travel_steps[motor]), -STEP_CORRECTION_MAX);
			}
			st_pre.mot[motor].corrected_steps += correction_steps;
			travel_steps[motor] -= correction_steps;
		}
#endif
		// Compute substeb increment. The accumulator must be *exactly* the incoming
		// fractional steps times the substep multiplier or positional drift will occur.
		// Rounding is performed to eliminate a negative bias in the int32 conversion
		// that results in long-term negative drift. (fabs/round order doesn't matter)

		st_pre.mot[motor].substep_increment = round(fabs(travel_steps[motor] * DDA_SUBSTEPS));


	}
	st_pre.move_type = MOVE_TYPE_ALINE;
	st_pre.segment_ready = true;
	return (STAT_OK);
}

/* 
 * st_prep_null() - Keeps the loader happy. Otherwise performs no action
 *
 *	Used by M codes, tool and spindle changes
 */

void st_prep_null()
{
	st_pre.move_type = MOVE_TYPE_NULL;
}

/* 
 * st_prep_dwell() 	 - Add a dwell to the move buffer
 */

void st_prep_dwell(float microseconds)
{
	st_pre.move_type = MOVE_TYPE_DWELL;
	st_pre.dda_ticks = (uint32_t)((microseconds/1000000) * FREQUENCY_DWELL);
	st_pre.dda_period = _f_to_period(FREQUENCY_DWELL);	// only needed by AVR
}

/*
 * _set_hw_microsteps() - set microsteps in hardware
 *
 *	For now the microsteps is the same as the microsteps (1,2,4,8)
 *	This may change if microstep morphing is implemented.
 */

static void _set_hw_microsteps(const uint8_t motor, const uint8_t microsteps)
{
#ifdef __ARM
	switch (motor) {
		if (!motor_1.enable.isNull()) case (MOTOR_1): { motor_1.setMicrosteps(microsteps); break; }
		if (!motor_2.enable.isNull()) case (MOTOR_2): { motor_2.setMicrosteps(microsteps); break; }
		if (!motor_3.enable.isNull()) case (MOTOR_3): { motor_3.setMicrosteps(microsteps); break; }
		if (!motor_4.enable.isNull()) case (MOTOR_4): { motor_4.setMicrosteps(microsteps); break; }
		if (!motor_5.enable.isNull()) case (MOTOR_5): { motor_5.setMicrosteps(microsteps); break; }
		if (!motor_6.enable.isNull()) case (MOTOR_6): { motor_6.setMicrosteps(microsteps); break; }
	}
#endif //__ARM
#ifdef __AVR
	if (microsteps == 8) {
		hw.st_port[motor]->OUTSET = MICROSTEP_BIT_0_bm;
		hw.st_port[motor]->OUTSET = MICROSTEP_BIT_1_bm;
	} else if (microsteps == 4) {
		hw.st_port[motor]->OUTCLR = MICROSTEP_BIT_0_bm;
		hw.st_port[motor]->OUTSET = MICROSTEP_BIT_1_bm;
	} else if (microsteps == 2) {
		hw.st_port[motor]->OUTSET = MICROSTEP_BIT_0_bm;
		hw.st_port[motor]->OUTCLR = MICROSTEP_BIT_1_bm;
	} else if (microsteps == 1) {
		hw.st_port[motor]->OUTCLR = MICROSTEP_BIT_0_bm;
		hw.st_port[motor]->OUTCLR = MICROSTEP_BIT_1_bm;
	}
#endif // __AVR
}

/***********************************************************************************
 * CONFIGURATION AND INTERFACE FUNCTIONS
 * Functions to get and set variables from the cfgArray table
 ***********************************************************************************/

/* HELPERS
 * _get_motor() - helper to return motor number as an index or -1 if na
 */

static int8_t _get_motor(const index_t index)
{
	char_t *ptr;
	char_t motors[] = {"123456"};
	char_t tmp[TOKEN_LEN+1];

	strcpy_P(tmp, cfgArray[index].group);
	if ((ptr = strchr(motors, tmp[0])) == NULL) {
		return (-1);
	}
	return (ptr - motors);
}

/*
 * _set_motor_steps_per_unit() - what it says
 * This function will need to be rethought if microstep morphing is implemented
 */

static void _set_motor_steps_per_unit(cmdObj_t *cmd) 
{
	uint8_t m = _get_motor(cmd->index);
	st_cfg.mot[m].units_per_step = (st_cfg.mot[m].travel_rev * st_cfg.mot[m].step_angle) / (360 * st_cfg.mot[m].microsteps);
	st_cfg.mot[m].steps_per_unit = 1 / st_cfg.mot[m].units_per_step;
}

/* PER-MOTOR FUNCTIONS
 * st_set_sa() - set motor step angle
 * st_set_tr() - set travel per motor revolution
 * st_set_mi() - set motor microsteps
 * st_set_pm() - set motor power mode
 * st_set_pl() - set motor power level
 */

stat_t st_set_sa(cmdObj_t *cmd)			// motor step angle
{ 
	set_flt(cmd);
	_set_motor_steps_per_unit(cmd); 
	return(STAT_OK);
}

stat_t st_set_tr(cmdObj_t *cmd)			// motor travel per revolution
{ 
	set_flu(cmd);
	_set_motor_steps_per_unit(cmd); 
	return(STAT_OK);
}

stat_t st_set_mi(cmdObj_t *cmd)			// motor microsteps
{
	if (fp_NE(cmd->value,1) && fp_NE(cmd->value,2) && fp_NE(cmd->value,4) && fp_NE(cmd->value,8)) {
		cmd_add_conditional_message((const char_t *)"*** WARNING *** Setting non-standard microstep value");
	}
	set_ui8(cmd);						// set it anyway, even if it's unsupported
	_set_motor_steps_per_unit(cmd);
	_set_hw_microsteps(_get_motor(cmd->index), (uint8_t)cmd->value);
	return (STAT_OK);
}

stat_t st_set_pm(cmdObj_t *cmd)			// motor power mode
{
	if (cmd->value >= MOTOR_POWER_MODE_MAX_VALUE) return (STAT_INPUT_VALUE_UNSUPPORTED);
	set_ui8(cmd);

	if (fp_ZERO(cmd->value)) { // people asked this setting take effect immediately, hence:
		_energize_motor(_get_motor(cmd->index));
	} else {
		_deenergize_motor(_get_motor(cmd->index));
	}
	return (STAT_OK);
}
/*
stat_t st_set_pl(cmdObj_t *cmd)			// motor power level
{
	if (cmd->value < (float)0) cmd->value = 0.000;
	if (cmd->value > (float)1) cmd->value = 1.000;
	set_flt(cmd);						// set the value in the motor config struct (st)

	uint8_t motor = _get_motor(cmd->index);
	st_run.mot[motor].power_level_dynamic = cmd->value;
	_set_motor_power_level(motor, cmd->value);
	return(STAT_OK);
}
*/
/*
 * st_set_pl() - set motor power level
 *
 *	Input value may vary from 0.000 to 1.000 The setting is scaled to allowable PWM range.
 *	This function sets both the scaled and dynamic power levels, and applies the 
 *	scaled value to the vref.
 */ 
stat_t st_set_pl(cmdObj_t *cmd)	// motor power level
{
	if (cmd->value < (float)0.0) cmd->value = 0.0;
	if (cmd->value > (float)1.0) {
		if (cmd->value > (float)100) cmd->value = 1;
 		cmd->value /= 100;		// accommodate old 0-100 inputs
	}
	set_flt(cmd);	// set power_setting value in the motor config struct (st)
	
	uint8_t motor = _get_motor(cmd->index);
	st_cfg.mot[motor].power_level_scaled = (cmd->value * POWER_LEVEL_SCALE_FACTOR);
	st_run.mot[motor].power_level_dynamic = (st_cfg.mot[motor].power_level_scaled);
	_set_motor_power_level(motor, st_cfg.mot[motor].power_level_scaled);
	return(STAT_OK);
}
/*
stat_t st_set_pl(cmdObj_t *cmd)	// motor power level
{
	if (cmd->value < (float)0) cmd->value = 0;
	if (cmd->value > (float)100) cmd->value = 100;
	set_flt(cmd);	// set power_setting value in the motor config struct (st)
	
	uint8_t motor = _get_motor(cmd->index);
	st_cfg.mot[motor].power_level_scaled = (cmd->value * POWER_LEVEL_SCALE_FACTOR);
	st_run.mot[motor].power_level_dynamic = (st_cfg.mot[motor].power_level_scaled);
	_set_motor_power_level(motor, st_cfg.mot[motor].power_level_scaled);
	return(STAT_OK);
}
*/

/* GLOBAL FUNCTIONS (SYSTEM LEVEL)
 *
 * st_set_mt() - set motor timeout in seconds
 * st_set_md() - disable motor power
 * st_set_me() - enable motor power
 *
 * Calling me or md with NULL will enable or disable all motors
 * Setting a value of 0 will enable or disable all motors
 * Setting a value from 1 to MOTORS will enable or disable that motor only
 */

stat_t st_set_mt(cmdObj_t *cmd)
{
	st_cfg.motor_power_timeout = min(POWER_TIMEOUT_SECONDS_MAX, max(cmd->value, POWER_TIMEOUT_SECONDS_MIN));
	return (STAT_OK);
}

stat_t st_set_md(cmdObj_t *cmd)	// Make sure this function is not part of initialization --> f00
{
	if (((uint8_t)cmd->value == 0) || (cmd->objtype == TYPE_NULL)) {
		st_deenergize_motors();
	} else {
		_deenergize_motor((uint8_t)cmd->value-1);
	}
	return (STAT_OK);
}

stat_t st_set_me(cmdObj_t *cmd)	// Make sure this function is not part of initialization --> f00
{
	if (((uint8_t)cmd->value == 0) || (cmd->objtype == TYPE_NULL)) {
		st_energize_motors();
	} else {
		_energize_motor((uint8_t)cmd->value-1);
	}
	return (STAT_OK);
}

/***********************************************************************************
 * TEXT MODE SUPPORT
 * Functions to print variables from the cfgArray table
 ***********************************************************************************/

#ifdef __TEXT_MODE

static const char msg_units0[] PROGMEM = " in";	// used by generic print functions
static const char msg_units1[] PROGMEM = " mm";
static const char msg_units2[] PROGMEM = " deg";
static const char *const msg_units[] PROGMEM = { msg_units0, msg_units1, msg_units2 };
#define DEGREE_INDEX 2

static const char fmt_me[] PROGMEM = "motors energized\n";
static const char fmt_md[] PROGMEM = "motors de-energized\n";
static const char fmt_mt[] PROGMEM = "[mt]  motor idle timeout%14.2f Sec\n";
static const char fmt_0ma[] PROGMEM = "[%s%s] m%s map to axis%15d [0=X,1=Y,2=Z...]\n";
static const char fmt_0sa[] PROGMEM = "[%s%s] m%s step angle%20.3f%s\n";
static const char fmt_0tr[] PROGMEM = "[%s%s] m%s travel per revolution%10.4f%s\n";
static const char fmt_0mi[] PROGMEM = "[%s%s] m%s microsteps%16d [1,2,4,8]\n";
static const char fmt_0po[] PROGMEM = "[%s%s] m%s polarity%18d [0=normal,1=reverse]\n";
static const char fmt_0pm[] PROGMEM = "[%s%s] m%s power management%10d [0=disabled,1=always on,2=in cycle,3=when moving]\n";
static const char fmt_0pl[] PROGMEM = "[%s%s] m%s motor power level%13.3f [0.000=minimum, 1.000=maximum]\n";

void st_print_mt(cmdObj_t *cmd) { text_print_flt(cmd, fmt_mt);}
void st_print_me(cmdObj_t *cmd) { text_print_nul(cmd, fmt_me);}
void st_print_md(cmdObj_t *cmd) { text_print_nul(cmd, fmt_md);}

static void _print_motor_ui8(cmdObj_t *cmd, const char *format)
{
	fprintf_P(stderr, format, cmd->group, cmd->token, cmd->group, (uint8_t)cmd->value);
}

static void _print_motor_flt_units(cmdObj_t *cmd, const char *format, uint8_t units)
{
	fprintf_P(stderr, format, cmd->group, cmd->token, cmd->group, cmd->value, GET_TEXT_ITEM(msg_units, units));
}

static void _print_motor_flt(cmdObj_t *cmd, const char *format)
{
	fprintf_P(stderr, format, cmd->group, cmd->token, cmd->group, cmd->value);
}

void st_print_ma(cmdObj_t *cmd) { _print_motor_ui8(cmd, fmt_0ma);}
void st_print_sa(cmdObj_t *cmd) { _print_motor_flt_units(cmd, fmt_0sa, DEGREE_INDEX);}
void st_print_tr(cmdObj_t *cmd) { _print_motor_flt_units(cmd, fmt_0tr, cm_get_units_mode(MODEL));}
void st_print_mi(cmdObj_t *cmd) { _print_motor_ui8(cmd, fmt_0mi);}
void st_print_po(cmdObj_t *cmd) { _print_motor_ui8(cmd, fmt_0po);}
void st_print_pm(cmdObj_t *cmd) { _print_motor_ui8(cmd, fmt_0pm);}
void st_print_pl(cmdObj_t *cmd) { _print_motor_flt(cmd, fmt_0pl);}

#endif // __TEXT_MODE
