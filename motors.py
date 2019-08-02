#!/usr/bin/python3

r'''Reads commands on STDIN, and commands the motors

Each line of text on STDIN is a command. Each command is a list of
whitespace-separated integers. The number of integers defines a different type
of command.

0 integers:

  If an empty line is received, we release all the motors


1 integer:

  0. Value in [0,3]: the motor index

  We release the requested motor


2 integers:

  0. Value in [0,3]: the motor index
  1. Value in [-4095,4095]: the velocity.

  Velocity 0 means "power hold".


4 integers:

  0. Value in [-4095,4095]: the velocity of motor 0
  1. Value in [-4095,4095]: the velocity of motor 1
  2. Value in [-4095,4095]: the velocity of motor 2
  3. Value in [-4095,4095]: the velocity of motor 3

  Velocity 0 means "power hold".
'''


import sys
import smbus


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
    #
    # I have the configuration in Fig 15 of the PCA9685 datasheet (see the LE357
    # datasheet): my pins are driving the cathode of an LED. So I want OEbar = 0
    # (physical connection) and INVRT=1, OUTDRV=0 in MODE2. I set MODE2 here.
    # Everything else (below) needs reverse polarities
    bus.write_i2c_block_data(device_addr, 0, [0x21])
    bus.write_i2c_block_data(device_addr, 1, [0x10])

def motor_to(bus, i_motor, pwm = None):
    '''Command a motor

    - abs(pwm) is in [0,4095]. Higher = faster.
    - sign(pwm) controls the direction.
    - pwm = 0 means "motor brake"
    - pwm = None means "float"

    See the comment in init for description about why all the polarities in this
    function are backwards

    '''

    if not (i_motor >= 0 and i_motor < 4):
        raise Exception("i_motor out of bounds: {}".format(i_motor))
    if not (pwm is None or abs(pwm) < 4095):
        raise Exception("pwm out of bounds: {}".format(pwm))

    register0 = base_register_for_motor(i_motor)
    if pwm is None:
        # floating
        bus.write_i2c_block_data(device_addr,
                               register0,
                               [
                                   # PWM
                                   0, 0, 0, 0,

                                   # IN1; need "0" to float
                                    0, 0, 0, 0xff,

                                   # IN2; need "0" to float
                                    0, 0, 0, 0xff,
                               ])
    elif pwm == 0:
        # braking
        bus.write_i2c_block_data(device_addr,
                               register0,
                               [
                                   # PWM
                                   0, 0, 0, 0,

                                   # IN1; need "1" to brake
                                   0, 0xff, 0, 0,

                                   # IN2; need "1" to brake
                                   0, 0xff, 0, 0,
                               ])
    elif pwm >= 0:
        pwm = 4095 - pwm # flip polarity because OEbar = 0
        bus.write_i2c_block_data(device_addr,
                               register0,
                               [
                                   # PWM
                                   0, 0, pwm & 0xff, pwm >> 8,

                                   # IN1; need "0" to move "forward"
                                   0, 0, 0, 0xff,

                                   # IN2; need "1" to move "forward"
                                   0, 0xff, 0, 0,
                               ])
    else:
        pwm = -pwm
        pwm = 4095 - pwm # flip polarity because OEbar = 0
        bus.write_i2c_block_data(device_addr,
                               register0,
                               [
                                   # PWM
                                   0, 0, pwm & 0xff, pwm >> 8,

                                   # IN1; need "1" to move "backward"
                                   0, 0xff, 0, 0,

                                   # IN2; need "0" to move "backward"
                                   0, 0, 0, 0xff
                               ])


i2c_bus     = 1
device_addr = 0x40
bus         = smbus.SMBus(i2c_bus)

init(bus)

for line in sys.stdin:

    f = line.split()
    if len(f) == 0:
        for i_motor in range(4):
            motor_to(bus,i_motor)
        continue

    if len(f) == 1:
        i_motor = int(f[0])
        try:
            motor_to(bus, i_motor, None)
        except Exception as e:
            sys.stderr.write(str(e) + "\n")
        continue

    if len(f) == 2:
        i_motor = int(f[0])
        try:
            motor_to(bus, i_motor, int(f[1]))
        except Exception as e:
            sys.stderr.write(str(e) + "\n")
        continue

    if len(f) == 4:
        for i_motor in range(4):
            motor_to(bus, i_motor, int(f[i_motor]))
        continue

    sys.stderr.write("I only know about 1-value, 2-value and 4-value commands. Got '{}'\n".format(line))
    continue

# done. stop all
    motor_to(bus,i_motor)

