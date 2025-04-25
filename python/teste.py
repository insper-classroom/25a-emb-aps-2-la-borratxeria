import serial

ser = serial.Serial('/dev/rfcomm0',9600)

while True:
	data = ser.readline()
	print(data.decode())