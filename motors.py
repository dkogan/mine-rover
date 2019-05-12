#!/usr/bin/python3

import sys
import smbus


i2c_bus     = 1
device_addr = 0x40

bus = smbus.SMBus(i2c_bus)


# I have 16 pwm channels.
# Each set of 4 channels corresponds to each motor.
# In each set of 4 channels the connections (in order) are:
#
# - PWM
# - IN1
# - IN2
#
# The IN1,IN2 signals aren't PWM at all, so I tell the controller to set them
# on/off

def base_register_for_motor(i_motor):
    i_channel = i_motor*4
    # See the PCA9685 docs
    return 6 + i_channel*4

def init(bus):
    # auto-increment registers, turn on the oscillator
    # totem-pole output
    b.write_i2c_block_data(device_addr, 0, [0x21, 0x4])

def motor_to(i_motor, pwm = None):
    '''Command a motor

    - abs(pwm) is in [0,4095].
    - sign(pwm) controls the direction.
    - pwm = 0 means "motor brake"
    - pwm = None menas "float"'''

    register0 = base_register_for_motor(i_motor)
    if pwm is None:
        b.write_i2c_block_data(device_addr,
                               register0,
                               [
                                   # PWM
                                   0, 0, 0, 0,

                                   # IN1; need "1" to float
                                   0, 0, 0, 0xff,

                                   # IN2; need "1" to float
                                   0, 0, 0, 0xff
                               ])
    else if pwm >= 0:
        b.write_i2c_block_data(device_addr,
                               register0,
                               [
                                   # PWM
                                   0, 0, pwm & 0xff, pwm >> 8,

                                   # IN1; need "0" to move "forward"
                                   0, 0xff, 0, 0,

                                   # IN2; need "1" to move "forward"
                                   0, 0, 0, 0xff
                               ])
    else:
        pwm = -pwm
        b.write_i2c_block_data(device_addr,
                               register0,
                               [
                                   # PWM
                                   0, 0, pwm & 0xff, pwm >> 8,

                                   # IN1; need "1" to move "backward"
                                   0, 0, 0, 0xff

                                   # IN2; need "0" to move "backward"
                                   0, 0xff, 0, 0,
                               ])
