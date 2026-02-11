import serial, time
s = serial.Serial('/dev/cu.usbserial-130', 115200, timeout=1)
t = time.time()
while time.time() - t < 20:
    d = s.readline()
    if d:
        print(d.decode('utf-8', 'ignore'), end='')
s.close()