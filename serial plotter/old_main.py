import time
import serial
ser = serial.Serial('COM3', 1000000, timeout=0)
ser.set_buffer_size(rx_size=16384,tx_size=16384)
import mido
mido.set_backend('mido.backends.pygame')
import pyqtgraph as pg
from pyqtgraph.Qt import QtCore
import numpy as np
import random
import threading
colors = [ 'y', 'r', 'g', 'b', 'c', 'm']
app = pg.mkQApp("Oscilloscope")
win = pg.GraphicsLayoutWidget(show = True)
BUFFER = 500
plots = []
curves = []
channels = 17
data_channels = [np.zeros(BUFFER) for i in range(channels)]
for i in range(channels):
    if i % 2 != 0:
        win.nextRow()
    title = f"Master" if i == 16 else f"Channel {i + 1}" 
    p = win.addPlot(title = title)
    p.setDownsampling(mode='peak') # Optimiza el dibujo de líneas
    p.setClipToView(True)          # No dibuja lo que está fuera de rango
    p.setRange(xRange=[0, BUFFER], yRange=[-128, 128], padding=0) # Fija el rango desde el inicio
    p.disableAutoRange()           # Evita cálculos innecesarios en cada frame
    c = p.plot(pen='w' if i == 16 else random.choice(colors)) 
    plots.append(p)
    curves.append(c) 
SAMPLE_RATE = 5000     # lo que envía Arduino
FPS = 50               # refresco visual

PERIOD_MS = int(1000 / FPS)

data = np.zeros(BUFFER)

def update_midi():
    while True:
        message = midi_input.poll()
        if message:
            ser.write(bytearray(message.bytes()))
        time.sleep(0.001)
        

def update_graph():
    channels_to_update = set()
    while ser.in_waiting >= 3:
        header = ser.read(1)
        if header[0] != 255:
            continue
        data = ser.read(2)
        channel_id = data[0]
        sample = int.from_bytes([data[1]], byteorder='big', signed=True)
        if channel_id < channels:
            data_channels[channel_id] = np.roll(data_channels[channel_id], -1)
            data_channels[channel_id][-1] = sample
            channels_to_update.add(channel_id)
    for ch in channels_to_update:
        curves[ch].setData(data_channels[ch])


def ready():
    print("Select port for TX")
    avalible_ports = mido.get_input_names()
    if avalible_ports:
        for i in enumerate(avalible_ports):
            print(i)
    selection = int(input("Port number: "))
    port_name = avalible_ports[selection]
    global midi_input
    midi_input = mido.open_input(port_name)
    midi_thread = threading.Thread(target=update_midi, daemon=True)
    midi_thread.start()
    global timer
    timer = QtCore.QTimer()
    timer.timeout.connect(update_graph)
    timer.start(PERIOD_MS)

ready()

if __name__ == '__main__':
    pg.exec()

