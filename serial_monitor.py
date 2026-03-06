import serial, time, sys
port = sys.argv[1] if len(sys.argv) > 1 else '/dev/cu.usbserial-130'
baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
dur = int(sys.argv[3]) if len(sys.argv) > 3 else 20
s = serial.Serial(port, baud, timeout=1)
t = time.time()
while time.time() - t < dur:
    d = s.readline()
    if d:
        print(d.decode('utf-8', 'ignore'), end='')
s.close()