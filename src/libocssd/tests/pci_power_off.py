import time
import serial
import serial.tools.list_ports

class HSC(object):
    def __init__(self):
        comports = serial.tools.list_ports.comports()
        self.hsc = []
        for ser in comports:
            try:
                if "HSC" in ser.product:
                    self.hsc.append(serial.Serial(ser.device, 9600, timeout = 5))
            except Exception as e:
                pass

    def __cmd_xfer(self, cmd):
        """
        To xfer a command list to HSC format.
        """
        head = 0xFE
        parity = (head + sum(cmd)) & 0xFF
        tmp = [head] + cmd + [parity]
        newCmd = "".join([chr(i) for i in tmp])
        return newCmd

    def __check_response(self, res):
        """
        To check the COM port response
        """
        tmp = [ord(x) for x in list(res)]
        status = tmp[6]
        if status != 0:
            print "[Error]Return data: %s" % [hex(i) for i in tmp]
            raise Exception("Unexpected return status %d" % status)
        else:
            return tmp

    def __waiting(self, hsc):
        """
        Wait for HSC until response is returned.
        """
        while True:
            if hsc.in_waiting is True:
                time.sleep(0.1)
            else:
                break

    def _test_connection(self, hsc):
        """
        To test HSC connection.
        """
        print "Test HSC connection"
        cmd = self.__cmd_xfer([0x00,0x00,0x00,0x00,0x00,0x00])
        hsc.write(cmd)
        self.__waiting(hsc)
        res = hsc.read(8)
        self.__waiting(hsc)
        self.__check_response(res)
        print "HSC ready"

    def _reboot(self, hsc):
        """
        To reboot HSC
        """
        print "Reboot HSC"
        cmd = self.__cmd_xfer([0x00,0x05,0x00,0x00,0x00,0x00])
        hsc.write(cmd)
        self.__waiting(hsc)
        res = hsc.read(8)
        self.__waiting(hsc)
        self.__check_response(res)
        time.sleep(0.1)

    def _power_on(self, hsc):
        """
        To power on DUT
        """
        print "Power on DUT"
        cmd = self.__cmd_xfer([0x01,0x40,0x01,0x00,0x00,0x00])
        hsc.write(cmd)
        self.__waiting(hsc)
        res = hsc.read(8)
        self.__waiting(hsc)
        self.__check_response(res)

    def _power_off(self, hsc):
        """
        To power off DUT
        """
        print "Power off DUT"
        cmd = self.__cmd_xfer([0x01,0x40,0x00,0x00,0x00,0x00])
        hsc.write(cmd)
        self.__waiting(hsc)
        res = hsc.read(8)
        self.__waiting(hsc)
        self.__check_response(res)

    def test_connection(self):
        for hsc in self.hsc:
            self._test_connection(hsc)
    
    def reboot(self):
        for hsc in self.hsc:
            self._reboot(hsc)

    def power_on(self):
        for hsc in self.hsc:
            self._power_on(hsc)

    def power_off(self):
        for hsc in self.hsc:
            self._power_off(hsc)

if __name__ == "__main__":
    handler = HSC()
    handler.test_connection()
    handler.power_off()
#    handler.power_on()



