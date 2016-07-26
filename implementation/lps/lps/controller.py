# -*- coding: utf-8 -*-

# Copyright (C) 2016, Maximilian Köhl <mail@koehlma.de>

import threading
import time

import bluetooth

from lps.constants import VICTIM_HUE, BROADCAST_ADDRESS
from lps.commands import Commands
from lps.event import Event
from lps.tinbot import TinBot
from lps.victim import Victim
from lps.map import Map
from lps.utils import log, ERROR


class Controller:
    def __init__(self, detector):
        self.detector = detector
        self.detector.data_event += self.on_data

        self.device_new_event = Event()
        self.device_new_event += self.on_device_new
        self.device_deleted_event = Event()

        self.devices_visible_event = Event()

        self.map_updated_event = Event()

        self.devices = {}

        self.broadcast_lock = threading.Lock()
        self.discover_lock = threading.Lock()

        self.victim = Victim()

        self.thread = threading.Thread(target=self.run)

        self.map = Map()

    def start(self):
        self.thread.start()

    def broadcast(self, command, payload=b'', source=None, target=None):
        with self.broadcast_lock:
            for device in self.devices.values():
                device.send(command, payload, source, target)
            if command == Commands.T2T_UPDATE_MAP:
                x, y, *patch = Commands.T2T_UPDATE_MAP.decode(payload)
                self.map.patch(x, y, patch)
        if command == Commands.T2T_UPDATE_MAP:
            self.map_updated_event.fire()

    def reset_map(self):
        self.map = Map()
        self.map_updated_event.fire()

    def discover(self):
        with self.discover_lock:
            devices = bluetooth.discover_devices()
            for address in devices:
                name = bluetooth.lookup_name(address)
                if name and name.startswith('e-puck_'):
                    if address not in self.devices:
                        self.devices_visible_event.fire(address)
                        TinBot(name, address, self)

    # event handlers
    def on_device_new(self, device):
        device.package_event += self.on_package

    def on_package(self, device, source, target, command, payload):
        if target == BROADCAST_ADDRESS:
            self.broadcast(command, payload, source, target)

    def on_data(self, _, positions):
        for device in self.devices.values():
            if device.hue in positions:
                position = positions[device.hue]
                device.position = (position['x'], position['y'], position['phi'])
        if VICTIM_HUE in positions:
            position = positions[VICTIM_HUE]
            self.victim.position = (position['x'], position['y'], position['phi'])

    # main loop
    def run(self):
        try:
            while True:
                self.discover()
                time.sleep(10)
        except bluetooth.BluetoothError as error:
            log('Bluetooth Error: {}'.format(error), ERROR)
