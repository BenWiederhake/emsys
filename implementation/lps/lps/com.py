# -*- coding: utf-8 -*-

import threading
import time
import queue

import bluetooth

from lps.event import Event

COLOR_MAP = {
    1: ('red', 0.95),
    2: ('blue', 0.54)
}

VICTIM_HUE = 0.39

MODE_ALONE = 0
MODE_FULL = 1
MODE_RHR = 2
MODE_VICDIR = 3


class TinBot:
    def __init__(self, name, address, controller):
        self.name = name.replace('e-puck_', 'tin_bot_')
        self.number = None
        self.color = None
        self.hue = None
        self.address = address
        self.controller = controller
        self.position = None

        self.on_package = Event()

        self.socket = bluetooth.BluetoothSocket()
        try:
            self.socket.connect((address, 1))
        except bluetooth.BluetoothError:
            return

        self.controller.devices[self.address] = self

        self.queue = queue.Queue()

        self.thread_receive = threading.Thread(target=self.receive)
        self.thread_receive.start()

        self.thread_sending = threading.Thread(target=self.sending)
        self.thread_sending.start()

        # say hello
        self.send(0x01)

    def send(self, command, payload=b''):
        self.queue.put((command, payload))

    def cmd_start(self):
        self.send(0x03)

    def cmd_lps_position(self, x, y, phi):
        self.position = (x, y, phi)
        x = int(x * 100).to_bytes(2, 'big')
        y = int(y * 100).to_bytes(2, 'big')
        phi = int(phi * 1000).to_bytes(2, 'big')
        self.send(0x02, x + y + phi)

    def cmd_debug_motors(self, left_speed, right_speed):
        left_speed = int(left_speed * 1000 + 32768).to_bytes(2, 'big')
        right_speed = int(right_speed * 1000 + 32768).to_bytes(2, 'big')
        self.send(0x13, left_speed + right_speed)

    def cmd_debug_leds(self, bitmask):
        self.send(0x10, bytes([bitmask]))

    def cmd_victim_phi(self, phi):
        self.send(0x04, int(phi * 1000).to_bytes(2, 'big'))

    def cmd_reset(self):
        self.send(0x05)

    def cmd_set_mode(self, mode):
        assert 0 <= mode <= 3
        self.send(0x06, bytes([mode]))

    def enable_debug(self):
        self.send(0x11, b'\x01')

    def disable_debug(self):
        self.send(0x11, b'\x00')

    def request_debug(self):
        self.send(0x12)

    def sending(self):
        time.sleep(2)
        while True:
            command, payload = self.queue.get()
            packet = b'\x00\x00' + bytes([command, len(payload)]) + payload
            # print(packet)
            self.socket.send(packet)

    def receive(self):
        time.sleep(2)
        buffer = b''
        try:
            while True:
                while len(buffer) < 4:
                    buffer += self.socket.recv(1024)
                source, target, command, length = buffer[:4]
                buffer = buffer[4:]
                while len(buffer) < length:
                    buffer += self.socket.recv(1024)
                payload = buffer[:length]
                buffer = buffer[length:]
                if command == 0x01:
                    self.number = source
                    self.color, self.hue = COLOR_MAP[self.number]
                    self.controller.device_new.fire(self)
                elif command == 0x20:
                    # TODO: implement victim phi correction
                    pass
                self.on_package.fire(self, source, target, command, payload)
        except bluetooth.btcommon.BluetoothError:
            pass
        finally:
            del self.controller.devices[self.address]
            if self.color:
                self.controller.device_deleted.fire(self)


class Controller(threading.Thread):
    def __init__(self, detector):
        super().__init__()
        self.detector = detector
        self.detector.new_data += self.on_new_data

        self.device_new = Event()
        self.device_deleted = Event()

        self.devices_visible = Event()

        self.devices = {}

        self.victim_position = None

    def on_new_data(self, _, positions):
        for device in self.devices.values():
            if device.hue in positions:
                position = positions[device.hue]
                device.cmd_lps_position(position['x'], position['y'], position['phi'])
        if VICTIM_HUE in positions:
            position = positions[VICTIM_HUE]
            self.victim_position = (position['x'], position['y'], position['phi'])

    def run(self):
        while True:
            devices = bluetooth.discover_devices()
            for address in devices:
                name = bluetooth.lookup_name(address)
                if name and name.startswith('e-puck_'):
                    if address not in self.devices:
                        self.devices_visible.fire(address)
                        TinBot(name, address, self)
            time.sleep(10)
