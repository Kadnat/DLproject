# Author: Nathanael Blavo Ballarin
# Date: July 24, 2024

import board
import busio
import digitalio
import adafruit_rfm9x
import time

# Define radio parameters
RADIO_FREQ_MHZ = 915.0

# Define pins connected to the chip
CS = digitalio.DigitalInOut(board.RFM_CS)
RESET = digitalio.DigitalInOut(board.RFM_RST)
WAKE_PIN = digitalio.DigitalInOut(board.D5)  # Define a wake pin for Arduino

# Initialize SPI bus
spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)

# Initialize RFM radio
rfm9x = adafruit_rfm9x.RFM9x(spi, CS, RESET, RADIO_FREQ_MHZ)

# Set the transmit power
rfm9x.tx_power = 23

# Set node addresses
rfm9x.node = 1  # This node is now node 1 (receiver)
rfm9x.destination = 255  # Set to broadcast address to receive from any node

print("Waiting for packets...")

# Initialize I2C bus with retry mechanism for pull-up resistor check
def initialize_i2c():
    i2c = None
    while i2c is None:
        try:
            i2c = busio.I2C(board.RX, board.TX)
            while not i2c.try_lock():
                pass
            i2c.unlock()
            print("I2C bus initialized")
        except RuntimeError:
            print("I2C bus initialization failed, retrying in 10 seconds...")
            time.sleep(10)
    return i2c

# Initialize I2C bus
i2c = initialize_i2c()

# I2C address of the Arduino Zero
I2C_ADDRESS = 0x08

# Set the direction of the wake pin
WAKE_PIN.direction = digitalio.Direction.OUTPUT

while True:
    # Receive packets from RFM radio
    packet = rfm9x.receive(with_header=True)
    if packet is not None:
        # Extract packet information
        packet_text = str(packet[4:], "utf-8")  # Exclude the first 4 bytes of the packet which are the header
        sending_node = packet[1]  # The second byte in the header is the sender address
        print(f"Received (raw payload) from node {sending_node}: {packet_text}")

        try:
            # Parse the packet for date, time, dendrometer, pressure, temperature, humidity, and moisture data
            components = packet_text.split(",")

            # Extract date and time
            date_time_str = components[0].strip()
            date_str, time_str = date_time_str.split(" ")
            date_components = date_str.split("/")
            time_components = time_str.split(":")

            year = int(date_components[0])
            month = int(date_components[1])
            day = int(date_components[2])
            hour = int(time_components[0])
            minute = int(time_components[1])
            second = int(time_components[2])

            # Extract values from the packet text
            dendro0 = float(components[1].split(": ")[1])
            dendro1 = float(components[2].split(": ")[1])
            dendro2 = float(components[3].split(": ")[1])
            dendro3 = float(components[4].split(": ")[1])
            press = float(components[5].split(": ")[1])
            temp = float(components[6].split(": ")[1])
            hum = float(components[7].split(": ")[1])
            moisture = float(components[8].split(": ")[1])

            # Create a string with the parsed data
            data_to_send = f"{sending_node},{year},{month},{day},{hour},{minute},{second},{temp},{hum},{press},{dendro0},{dendro1},{dendro2},{dendro3},{moisture}"
            print(f"Parsed data: Node={sending_node}, Date={date_str}, Time={time_str}, Temp={temp}, Hum={hum}, Press={press}, Dendro={dendro0}, Dendro={dendro1}, Dendro={dendro2}, Dendro={dendro3}, Moisture={moisture}")

            # Wake up the Arduino
            WAKE_PIN.value = True
            time.sleep(0.1)  # Wait for a short period to ensure the Arduino is awake

            # Send data over I2C
            try:
                while not i2c.try_lock():
                    pass
                i2c.writeto(I2C_ADDRESS, bytes(data_to_send, 'utf-8'))
                i2c.unlock()
                print("Data sent over I2C")
            except OSError:
                print("I2C write failed. Reinitializing I2C bus.")
                i2c = initialize_i2c()  # Reinitialize I2C bus if write fails
            finally:
                WAKE_PIN.value = False  # Set the wake pin to low after sending data

        except (ValueError, IndexError) as e:
            print(f"Received packet format error: {e}")

        # Print the RSSI value
        print("RSSI: {0} dB".format(rfm9x.rssi))

        # Send acknowledgement
        rfm9x.send(bytes(f"Acknowledgement from node {rfm9x.node} to node {sending_node}", "UTF-8"))
