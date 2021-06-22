#!/usr/bin/env python3
import minimalmodbus
import time
import argparse
import logging

SYSTEM_SETTING_SIZE = 8

class DigitalPinSetting:
    SETTINGS_SIZE = 16
    SETTINGS = ["mode", "group", "output_value",
                "click_command", "single_click_command", "long_click_command",
                "double_click_command", "signal_rise_command",
                "signal_fall_command", "release_command",
                "pwm_on_value"]

    def __init__(self, instrument, pinindex):
        self.instrument=instrument
        self.pinindex=pinindex
        self.address = SYSTEM_SETTING_SIZE + pinindex * DigitalPinSetting.SETTINGS_SIZE

    def set(self, setting, value):
        idx = DigitalPinSetting.SETTINGS.index(setting);
        self.instrument.write_register(self.address + idx, value)

    def print(self):
        val = self.instrument.read_registers(self.address, DigitalPinSetting.SETTINGS_SIZE, functioncode=3)
        for i, setting in enumerate(DigitalPinSetting.SETTINGS): 
            print("pin %d %s: %x" % (self.pinindex, setting, val[i]));

class ButtonCounters:
    COUNTER_OFFSET=8
    COUNTER_SIZE=8
    COUNTERS = ["pin_number", "click_cnt", "single_click_cnt", "double_click_cnt", "long_press_cnt", "release_cnt"]

    def __init__(self, instrument, pinindex):
        self.instrument=instrument
        self.pinindex=pinindex
        self.address = ButtonCounters.COUNTER_OFFSET + pinindex * ButtonCounters.COUNTER_SIZE

    def print(self):
        val = self.instrument.read_registers(self.address, ButtonCounters.COUNTER_SIZE, functioncode=4)
        for i, setting in enumerate(ButtonCounters.COUNTERS): 
            print("pin %d %s: %x" % (self.pinindex, setting, val[i]));


class HomeControl:
    def __init__(self, debug=False):
        self.instrument = minimalmodbus.Instrument('/dev/tty.usbserial-143320', 1, debug=debug)
        self.instrument.serial.baudrate = 19200
        self.number_of_pins = self.instrument.read_register(0, functioncode=4);

    def uptime(self):
        print("uptime: %d seconds" % (self.instrument.read_long(1, functioncode=4, signed=False, byteorder=minimalmodbus.BYTEORDER_LITTLE_SWAP) / 1000))

    def dump_coils(self):
        vals = self.instrument.read_bits(0, self.number_of_pins,functioncode=1);
        for i, val in enumerate(vals):
            print("coil %d: %d" % (i, val));

    def dump_holding_registers(self):
        for i in range(0, self.number_of_pins):
            DigitalPinSetting(self.instrument, i).print()

    def dump_input_registers(self):
        for i in range(0, 8):
            val = self.instrument.read_register(i, functioncode=4)
            print("input register %d: %x" % (i, val));
        for i in range(0, self.number_of_pins):
            ButtonCounters(self.instrument, i).print()

    def pinsetting(self, pinindex):
        if (pinindex >= self.number_of_pins or pinindex < 0):
            raise ValueError("pinindex out of range");
        return DigitalPinSetting(self.instrument, pinindex);

    def set_coil(self,pinindex, value):
        self.instrument.write_bit(pinindex, value);

logging.basicConfig(level=logging.DEBUG);
parser = argparse.ArgumentParser()
parser.add_argument("--dumpcoils", help="dump coils", action="store_true")
parser.add_argument("--dumpholdingregisters", help="dump holding registers", action="store_true")
parser.add_argument("--dumpinputregisters", help="dump input registers", action="store_true")
parser.add_argument("--pin-index", dest='pinindex', help="pin index", type=int)
parser.add_argument("--uptime", help="print uptime", action="store_true")
parser.add_argument("--debug", help="debug", action="store_true", default=False)
group = parser.add_mutually_exclusive_group();
group.add_argument("--pin-setting", dest='pinsetting')
group.add_argument("--set-coil", dest='setcoil', action='store_true');
parser.add_argument("val", type=int, nargs='?')
args = parser.parse_args()

homectrl = HomeControl(args.debug);

if(args.dumpcoils):
    homectrl.dump_coils()
    
if(args.dumpholdingregisters):
    homectrl.dump_holding_registers()
    
if(args.dumpinputregisters):
    homectrl.dump_input_registers()

if(args.pinsetting):
    homectrl.pinsetting(args.pinindex).set(args.pinsetting, args.val)

if(args.setcoil):
    homectrl.set_coil(args.pinindex, args.val);

if(args.uptime):
    homectrl.uptime()
