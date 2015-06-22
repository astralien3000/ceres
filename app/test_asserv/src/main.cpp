#include <stm32f4_discovery.h>
#include <stm32f4xx_hal.h>

extern "C" {

#include "init.h"
#include "encoder.h"
#include "motor.h"
#include "uart.h"
#include "scheduler.h"
#include "gp2.h"

}

#include <base/integer.hpp>
#include <device/input.hpp>
#include <device/output.hpp>

#include <filter/pid_filter.hpp>
#include <filter/diff_filter.hpp>
#include <filter/quadramp_filter.hpp>

#include <device/controller/motor_controller.hpp>

#include <device/eirbot2014/odometer.hpp>
#include <device/controller/robot_controller.hpp>

extern "C" {
__extension__ typedef int __guard __attribute__((mode (__DI__)));

int __cxa_guard_acquire(__guard *g) __attribute__((weak));
int __cxa_guard_acquire(__guard *g) {return !*(char *)(g);}

void __cxa_guard_release (__guard *g) __attribute__((weak));
void __cxa_guard_release (__guard *g) {*(char *)g = 1;}

void __cxa_guard_abort (__guard *) __attribute__((weak));
void __cxa_guard_abort (__guard *) {}

void __cxa_pure_virtual(void) __attribute__((weak));
void __cxa_pure_virtual(void) {}
}


class LEncoder : public Input<s32> {
public:
	LEncoder(void) {}

	s32 getValue(void) {
		return encoder2_get_value_short();
	}
};

class REncoder : public Input<s32> {
public:
	REncoder(void) {}

	s32 getValue(void) {
		return encoder1_get_value_short();
	}
};

class LMotor: public Output<s32> {
public:
	bool lock = false;

	LMotor(void) {}

	void setValue(s32 val) {
		if(lock) {
			motor1_set_value(0);
		}
		else {
			motor1_set_value(val);
		}
	}
};

class RMotor: public Output<s32> {
public:
	bool lock = false;

	RMotor(void) {}

	void setValue(s32 val) {
		if(lock) {
			motor2_set_value(0);
		}
		else {
			motor2_set_value(val);
		}
	}


};


RMotor mot_r;
LMotor mot_l;
REncoder enc_r;
LEncoder enc_l;

// Utils
PidFilter id;

// Asserv mot
DiffFilter diff_l;
DiffFilter diff_r;

PidFilter pid_l;
PidFilter pid_r;

s32 cmd_l = 0;
s32 cmd_r = 0;

// Asserv AD
s32 angle = 0;
s32 dist = 0;

QuadrampFilter qramp_a;
QuadrampFilter qramp_d;

PidFilter pid_a;
PidFilter pid_d;

void asserv_tick(void) {
	mot_l.setValue(pid_l.doFilter(cmd_l - diff_l.doFilter(enc_l.getValue())));
	mot_r.setValue(pid_r.doFilter(cmd_r - diff_r.doFilter(enc_r.getValue())));
}

void odo_tick(void) {
    dist = (s32)((s32)enc_l.getValue() + 1.2*(s32)enc_r.getValue());
    angle = (s32)(1.2*enc_r.getValue() - enc_l.getValue());
}

s32 get_dist(void) {
	return dist * 0.55;
}

s32 get_angle(void) {
	return angle * 0.7;
}

s32 deg2angle(s32 deg) {
	return deg * 16;
}

s32 cmd_a = 0;
s32 cmd_d = 0;
bool lock = false;

void asserv_ad_tick(void) {
	if(!lock) {
		s32 corr_a = pid_a.doFilter(qramp_a.doFilter(cmd_a) - get_angle());
		s32 corr_d = pid_d.doFilter(qramp_d.doFilter(cmd_d) - get_dist());

		cmd_l = corr_d - corr_a/2;
		cmd_r = corr_d + corr_a/2;
	}
}


int main(void) {

	// Config asserv
	id.setGains(1,0,0);

	//// Motor
	diff_l.setDelta(1);
	diff_r.setDelta(1);

	pid_l.setGains(160, 1, 0);
	pid_l.setMaxIntegral(2000);
	pid_l.setOutShift(4);

	pid_r.setGains(135, 0, 0);
	pid_r.setMaxIntegral(0);
	pid_r.setOutShift(4);

	//// Angle-Dist
	qramp_a.setFirstOrderLimit(200,200);
	qramp_a.setSecondOrderLimit(100,100);

	qramp_d.setFirstOrderLimit(50,50);
	qramp_d.setSecondOrderLimit(10, 10);

	pid_a.setGains(200, 1, 0);
	pid_a.setMaxIntegral(40000);
	pid_a.setOutShift(9);

	pid_d.setGains(3, 0, 0);
	pid_d.setMaxIntegral(0);
	pid_d.setOutShift(5);

	// Init system
	init();
	scheduler_init();
	encoder1_init();
	encoder2_init();
	motor_init();
	//uart_init(330); // 9600 baud ?!?
	gp2_init();



	// Code
	BSP_LED_On(LED4);



	//int count = 0;
	//char c = 0;
	//cmd_d = 20000;
	uint32_t rf = 0;
	uint32_t lf = 0;
	uint32_t rb = 0;
	uint32_t lb = 0;

	//cmd_a = deg2angle(90);
	cmd_d = 100000;
	while(1) {

		rf = gp2_get_rf();
		lf = gp2_get_lf();
		rb = gp2_get_rb();
		lb = gp2_get_lb();

		if(rf > 2500 || rb > 2500 || lb > 2500) {
			mot_l.lock = true;
			mot_r.lock = true;
			lock = true;
		}
		else {
			mot_l.lock = false;
			mot_r.lock = false;
			lock = false;
		}

		//short enc1 = encoder1_get_value_short();
		//short enc2 = encoder2_get_value_short();
		//motor2_set_value(-30*enc1);
		//motor1_set_value(-30*enc2);

		//motor2_set_value(right);
		//motor1_set_value(left);

		//uart_write_int(enc2);
		//uart_putchar('\n');
	}

	return 0;
}
