/*
 * Calibration
 *
 * Loop A/B/Main Clock Out jacks:
 * -Gate
 * -Trigger
 *
 * LED Brightness for loop LEDs
 *
 * 1V/oct tracking compensation
 *
 * ????Level A/B CV jacks:
 * -Delay Level (default)
 * -Dry/Wet amount
 *
 * ????Time A/B knob:
 * -Quantized normally, unquantized when turned with Reverse held down (default)
 * -Unquantized normally, quantized when turned with Reverse held down
 *
 * ????Time A/B CV jack:
 * -Quantized (default)
 * -Always unquantized (...try this?)
 * -Follows mode of knob (quantized until Time knob is wiggled with Reverse held down)
 *
 *
 * Noisegate on/off
 * Soft clipping on/off
 *
 */

#include "globals.h"
#include "system_settings.h"
#include "adc.h"
#include "params.h"
#include "dig_inouts.h"
#include "flash_user.h"

extern int16_t i_smoothed_potadc[NUM_POT_ADCS];

extern float param[NUM_CHAN][NUM_PARAMS];
extern uint8_t mode[NUM_CHAN][NUM_CHAN_MODES];
extern uint8_t global_mode[NUM_GLOBAL_MODES];
extern uint16_t loop_led_brightness;
extern uint8_t loop_led_state[NUM_CHAN];

extern uint8_t flag_inf_change[2];
extern uint8_t flag_rev_change[2];


uint8_t disable_mode_changes=0;

void set_default_system_settings(void)
{
	param[0][TRACKING_COMP]=1.0;
	param[1][TRACKING_COMP]=1.0;

	loop_led_brightness = 4;
	mode[0][LOOP_CLOCK_GATETRIG] = TRIG_MODE;
	mode[1][LOOP_CLOCK_GATETRIG] = TRIG_MODE;
	mode[0][MAIN_CLOCK_GATETRIG] = TRIG_MODE;

	global_mode[AUTO_MUTE] = 1;
	global_mode[SOFTCLIP] = 1;

}


void check_entering_system_mode(void)
{

	static int32_t ctr=0;

	if (ENTER_SYSMODE_BUTTONS)
	{
		if (ctr==-1) //waiting for button release
		{
			LED_PINGBUT_ON;
			LED_REV1_ON;
			LED_REV2_ON;
			LED_LOOP1_ON;
			LED_LOOP2_ON;
			LED_INF1_ON;
			LED_INF2_ON;
		}
		else ctr++;

		if (ctr>300000)
		{
			if (global_mode[SYSTEM_SETTINGS] == 0)
			{
				global_mode[SYSTEM_SETTINGS] = 1;
				global_mode[CALIBRATE] = 0;
				ctr=-1;
			}
			else
			{
				save_flash_params();
				global_mode[SYSTEM_SETTINGS] = 0;
			}
		}
	}
	else
	{
		if ((ctr>1000) && (ctr<50000) && (global_mode[SYSTEM_SETTINGS] == 1)) //released buttons too early ==> cancel (exit without save)
		{
			global_mode[SYSTEM_SETTINGS] = 0;
		}
		ctr=0;
	}

}


void update_system_settings(void)
{
	uint8_t switch1, switch2;
	static uint32_t buttons_down=0;


	switch1=TIMESW_CH1;
	switch2=TIMESW_CH2;

	//
	// Switches: Up | Up
	// Entry point and Save settings mode
	//

	if (switch1==SWITCH_UP && switch2==SWITCH_UP)
	{
		disable_mode_changes=0;
	}

	//
	// Switches: Center | Center
	// Set Tracking Compensation (1V/oct) using LEVEL pots ("Delay Feed")
	//
	if (switch1==SWITCH_CENTER && switch2==SWITCH_CENTER)
	{
		disable_mode_changes=0;

		param[0][TRACKING_COMP]=((i_smoothed_potadc[LEVEL_POT*2]/8192.0f) + 0.75);
		param[1][TRACKING_COMP]=((i_smoothed_potadc[LEVEL_POT*2+1]/8192.0f) + 0.75);

		param[0][LEVEL] = 1.0;
		param[1][LEVEL] = 1.0;

	}

	//
	// Switches: Up | Down
	// Select Trig/Gate using INF/REV buttons
	// Set LED brightness for loop LEDs using REGEN A pot
	//
	if (switch1==SWITCH_UP && switch2==SWITCH_DOWN)
	{
		disable_mode_changes=1;
		loop_led_brightness=((i_smoothed_potadc[REGEN_POT*2]/137) + 1);

		if (flag_rev_change[0])
		{
			if (mode[0][LOOP_CLOCK_GATETRIG] == GATE_MODE)
				mode[0][LOOP_CLOCK_GATETRIG] = TRIG_MODE;
			else
				mode[0][LOOP_CLOCK_GATETRIG] = GATE_MODE;

			flag_rev_change[0]=0;
		}

		if (flag_rev_change[1])
		{
			if (mode[1][LOOP_CLOCK_GATETRIG] == GATE_MODE)
				mode[1][LOOP_CLOCK_GATETRIG] = TRIG_MODE;
			else
				mode[1][LOOP_CLOCK_GATETRIG] = GATE_MODE;

			flag_rev_change[1]=0;
		}

		if (flag_inf_change[0])
		{
			if (mode[0][MAIN_CLOCK_GATETRIG] == GATE_MODE)
				mode[0][MAIN_CLOCK_GATETRIG] = TRIG_MODE;
			else
				mode[0][MAIN_CLOCK_GATETRIG] = GATE_MODE;

			flag_inf_change[0]=0;
		}
	}

	//
	// Switches: Down | Up
	// Set Auto-Mute and Soft Clipping
	//

	if (switch1==SWITCH_DOWN && switch2==SWITCH_UP)
	{
		disable_mode_changes=1;

		if (flag_rev_change[0])
		{
			if (global_mode[AUTO_MUTE])
				global_mode[AUTO_MUTE] = 0;
			else
				global_mode[AUTO_MUTE] = 1;

			flag_rev_change[0]=0;
		}

		if (flag_rev_change[1])
		{
			if (global_mode[SOFTCLIP])
				global_mode[SOFTCLIP] = 0;
			else
				global_mode[SOFTCLIP] = 1;

			flag_rev_change[1]=0;
		}

	}

}


//100 * 0.2ms = 20ms
#define TRIG_CNTS 200
//1665 * 0.2ms = 333ms
#define GATE_CNTS 1665

void update_system_settings_leds(void)
{
	static uint32_t led_flasher=0;
	uint8_t switch1, switch2;

	switch1=TIMESW_CH1;
	switch2=TIMESW_CH2;

	if (switch1==SWITCH_UP)
	{
		led_flasher++;

		if (led_flasher < TRIG_CNTS)
		{
			loop_led_state[0]=1;
			LED_PINGBUT_OFF;
			loop_led_state[1]=0;
		}
		else if (led_flasher < GATE_CNTS)
		{
			if (mode[0][LOOP_CLOCK_GATETRIG]==TRIG_MODE)
				loop_led_state[0]=0;
		}
		else if (led_flasher < (GATE_CNTS + TRIG_CNTS))
		{
			loop_led_state[0]=0;
			LED_PINGBUT_ON;
			loop_led_state[1]=0;
		}
		else if (led_flasher < (GATE_CNTS*2))
		{
			if (mode[0][MAIN_CLOCK_GATETRIG]==TRIG_MODE)
				LED_PINGBUT_OFF;
		}
		else if (led_flasher < ((GATE_CNTS*2) + TRIG_CNTS))
		{
			loop_led_state[0]=0;
			LED_PINGBUT_OFF;
			loop_led_state[1]=1;
		}
		else if (led_flasher < (GATE_CNTS*3))
		{
			if (mode[1][LOOP_CLOCK_GATETRIG]==TRIG_MODE)
				loop_led_state[1]=0;
		}
		else
			led_flasher=0;
	}

}
