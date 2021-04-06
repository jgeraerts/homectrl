import minimalmodbus
import time
import argparse
import logging

class HomeControl:
    def __init__(self):
        self.instrument = minimalmodbus.Instrument('/dev/tty.usbserial-142320', 1, debug=False)
        self.instrument.serial.baudrate = 9600
        self.number_of_pins = self.instrument.read_register(0, functioncode=4);

    def dump_coils(self):
        for i in range(0, self.number_of_pins):
            val = self.instrument.read_bit(i, functioncode=1);
            print("coil %d: %d" % (i, val));

    def dump_holding_registers(self):
        for i in range(0, 8+15*12):
            val = self.instrument.read_register(i)
            print("holding register %d: %x" % (i, val));

    def dump_input_registers(self):
        for i in range(0, self.number_of_pins*8+8):
            val = self.instrument.read_register(i, functioncode=4)
            print("input register %d: %x" % (i, val));

    def set_pin_mode(self, pinindex, value):
        registerindex = 8 + 16*pinindex + 0;
        logging.debug("setting pin mode register index %d to value %d", registerindex, value)
        self.instrument.write_register(registerindex, value);

    def set_group(self, pinindex, value):
        registerindex = 8 + 16*pinindex + 1;
        logging.debug("setting pin group register index %d to value %d", registerindex, value)
        self.instrument.write_register(registerindex, value);

    def set_single_click_command(self, pinindex, value):
        registerindex = 8 + 16*pinindex + 4;
        logging.debug("setting pin single click command register index %d to value %d", registerindex, value)
        self.instrument.write_register(registerindex, value);

    def set_coil(self,pinindex, value):
        self.instrument.write_bit(pinindex, value);

logging.basicConfig(level=logging.DEBUG);
parser = argparse.ArgumentParser()
parser.add_argument("--dumpcoils", help="dump coils", action="store_true")
parser.add_argument("--dumpholdingregisters", help="dump holding registers", action="store_true");
parser.add_argument("--dumpinputregisters", help="dump input registers", action="store_true");
parser.add_argument("--pin-index", dest='pinindex', help="pin index", type=int)
parser.add_argument("--set-mode", dest='mode', action='store_true')
parser.add_argument("--set-group", dest='group', action='store_true')
parser.add_argument("--set-single-click-command", dest='singleclickcommand', action='store_true')
parser.add_argument("--set-coil", dest='setcoil', action='store_true');
parser.add_argument("val", type=int, nargs='?')
args = parser.parse_args()

homectrl = HomeControl();

if(args.dumpcoils):
    homectrl.dump_coils()
    
if(args.dumpholdingregisters):
    homectrl.dump_holding_registers()
    
if(args.dumpinputregisters):
    homectrl.dump_input_registers()

if(args.singleclickcommand):
    homectrl.set_single_click_command(args.pinindex, args.val)

if(args.group):
    homectrl.set_group(args.pinindex, args.val);

if(args.mode):
    homectrl.set_pin_mode(args.pinindex, args.val)

if(args.setcoil):
    homectrl.set_coil(args.pinindex, args.val);
