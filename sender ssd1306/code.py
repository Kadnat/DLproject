import time
import board
from board import *
import busio
import analogio
import digitalio
import displayio
import terminalio
from adafruit_display_text import label
import adafruit_displayio_ssd1306
import rtc
from adafruit_dps310.basic import DPS310
import adafruit_sht4x
import adafruit_rfm9x
from adafruit_ads1x15.analog_in import AnalogIn
from adafruit_seesaw.seesaw import Seesaw
import circuitpython_csv as csv
import os
import microcontroller
import supervisor

import ulab.numpy as np
import adafruit_ads1x15.ads1115 as ADS

# Global variable to store the CSV filename
csv_filename = None

# Function to scan the I2C bus
def i2c_scan(i2c):
    print("Scanning I2C bus...")
    devices = []
    while not i2c.try_lock():
        pass
    try:
        for address in range(0x08, 0x78):
            try:
                i2c.writeto(address, b'')
                devices.append(hex(address))
            except OSError:
                pass
    finally:
        i2c.unlock()
    if devices:
        print("Found I2C device(s):", devices)
    else:
        print("No I2C devices found.")
    return devices

displayio.release_displays()

# Create I2C interfaces
i2c0 = board.STEMMA_I2C()
i2c1 = busio.I2C(scl=board.RX, sda=board.TX)

# Scan I2C bus for devices
i2c_devices = i2c_scan(i2c1)
i2c_test = i2c_scan(i2c0)

# Configure display size
ssd_width = 128
ssd_height = 64

# Configure buttons
b1 = digitalio.DigitalInOut(board.D9)
b1.direction = digitalio.Direction.INPUT
b1.pull = digitalio.Pull.DOWN

b2 = digitalio.DigitalInOut(board.D6)
b2.direction = digitalio.Direction.INPUT
b2.pull = digitalio.Pull.DOWN

b3 = digitalio.DigitalInOut(board.D5)
b3.direction = digitalio.Direction.INPUT
b3.pull = digitalio.Pull.DOWN

b4 = digitalio.DigitalInOut(board.D10)
b4.direction = digitalio.Direction.INPUT
b4.pull = digitalio.Pull.DOWN

# Create labels for display
instruction_label1 = label.Label(terminalio.FONT)
instruction_label1.anchor_point = (0.5, 0.5)
instruction_label1.anchored_position = (ssd_width // 2, ssd_height // 2 - 10)
instruction_label1.scale = 1
instruction_label1.color = 0xFFFFFF
instruction_label1.text = "B1: Change date"

instruction_label2 = label.Label(terminalio.FONT)
instruction_label2.anchor_point = (0.5, 0.5)
instruction_label2.anchored_position = (ssd_width // 2, ssd_height // 2 + 10)
instruction_label2.scale = 1
instruction_label2.color = 0xFFFFFF
instruction_label2.text = "B2: Start MES"

clock_label = label.Label(terminalio.FONT)
clock_label.anchor_point = (0.5, 0.5)
clock_label.anchored_position = (ssd_width // 2, ssd_height // 2-10)
clock_label.scale = 1
clock_label.color = 0xFFFFFF

date_label = label.Label(terminalio.FONT)
date_label.anchor_point = (0.5, 0.5)
date_label.anchored_position = (ssd_width // 2, ssd_height // 2+10)
date_label.scale = 1
date_label.color = 0xFFFFFF

text_label = label.Label(terminalio.FONT)
text_label.anchor_point = (0.5, 0.5)
text_label.anchored_position = (ssd_width // 2, ssd_height // 2+10)
text_label.scale = 1
text_label.color = 0xFFFFFF

# Create a DisplayIO group
layer = displayio.Group()

# Initialize RTC
rtc_instance = rtc.RTC()

# Time variables
hours = 0
minutes = 0
seconds = 0
year = 0
month = 0
day = 0

# Initialize LoRa
RADIO_FREQ_MHZ = 915.0
CS = digitalio.DigitalInOut(RFM_CS)  # Replace RFM_CS with the correct pin
RESET = digitalio.DigitalInOut(RFM_RST)  # Replace RFM_RST with the correct pin
spi = busio.SPI(board.SCK, MOSI=board.MOSI, MISO=board.MISO)
try:
    rfm9x = adafruit_rfm9x.RFM9x(spi, CS, RESET, RADIO_FREQ_MHZ)
    rfm9x.tx_power = 23
    rfm9x.node = 2
    rfm9x.destination = 1
except Exception as e:
    print(f"Error initializing RFM9x: {e}")

# Set window size for moving average filter
window_size = 10
adc_values = []

# Clear the display
def clear_display():
    while len(layer) > 0:
        layer.pop()

# Display variables
enable_menu = True

# Initialize display
try:
    ssd_bus = displayio.I2CDisplay(i2c1, device_address=int(i2c_devices[0], 16) if i2c_devices else 0x3C)
    display = adafruit_displayio_ssd1306.SSD1306(ssd_bus, width=ssd_width, height=ssd_height)
except Exception as e:
    print(f"Error initializing display: {e}")
    display = None


# Update display with current time
def update_display():
    if display:
        time_str = "{:02}:{:02}:{:02}".format(hours, minutes, seconds)
        clock_label.text = time_str
        if clock_label not in layer:
            layer.append(clock_label)

        time_str2 = "{:02}/{:02}/{:02}".format(year, month, day)
        date_label.text = time_str2
        if date_label not in layer:
            layer.append(date_label)
        display.show(layer)
    else:
        print("Display not initialized.")

# Update RTC with current time
def update_rtc():
    global hours, minutes, seconds, year, month, day
    rtc_instance.datetime = time.struct_time((year, month, day, hours, minutes, seconds, 0, -1, -1))

# Set time mode to adjust time
def set_time_mode():
    global hours, minutes, seconds, year, month, day
    setting_hours = True
    date_mode = False
    clear_display()
    layer.append(clock_label)
    update_display()
    display.show(layer)
    while True:
        # Read buttons
        if date_mode == False:
            if b1.value:
                hours = (hours + 1) % 24
                update_display()
                time.sleep(0.2)  # Debounce delay

            if b2.value:
                minutes = (minutes + 1) % 60
                update_display()
                time.sleep(0.2)  # Debounce delay

            if b3.value:
                seconds = (seconds + 1) % 60
                update_display()
                time.sleep(0.2)  # Debounce delay

            if b4.value:
                date_mode = True
                time.sleep(0.5) # Debounce delay

        if date_mode:
            if b1.value:
                year = (year + 1) % 100
                year += 2000
                update_display()
                time.sleep(0.2)  # Debounce delay

            if b2.value:
                month = (month + 1) % 13
                if month == 0 :
                    month =1
                update_display()
                time.sleep(0.2)  # Debounce delay

            if b3.value:
                if month == 2:
                    day = (day + 1) % 29 #TODO add bisextiles years
                    if day == 0:
                        day = 1
                    update_display()
                    time.sleep(0.2)  # Debounce delay
                elif month ==4 or month ==6 or month ==9 or month ==11:
                    day = (day + 1) % 31
                    if day == 0:
                        day = 1
                    update_display()
                    time.sleep(0.2)  # Debounce delay
                else:
                    day = (day + 1) % 32
                    if day == 0:
                        day = 1
                    update_display()
                    time.sleep(0.2)  # Debounce delay

            if b4.value:
                date_mode = False
                update_rtc()
                time.sleep(0.2)  # Debounce delay
                break
    clear_display()

# Initialize soil moisture sensor
try:
    ss = Seesaw(i2c0, addr=0x36)
except Exception as e:
    print(f"Error initializing soil moisture sensor: {e}")
    ss = None

# Read moisture level from soil moisture sensor
def read_moisture():
    if ss:
        try:
            return ss.moisture_read()
        except Exception as e:
            print(f"Error reading soil moisture: {e}")
            return 0
    else:
        return 0

# Test DPS310 sensor for temperature and pressure
def test_dps310():
    try:
        dps310 = DPS310(i2c0)
        return dps310.temperature, dps310.pressure
    except Exception as e:
        print(f"Error reading DPS310: {e}")
        return 0, 0  # Return 0 in case of error

# Test SHT41 sensor for temperature and humidity
def test_sht41():
    try:
        sht = adafruit_sht4x.SHT4x(i2c0)
        return sht.measurements
    except Exception as e:
        print(f"Error reading SHT41: {e}")
        return 0, 0  # Return 0 in case of error

# Initialize ADS1115 for ADC readings
try:
    ads = ADS.ADS1115(i2c0)
    adc = AnalogIn(ads, ADS.P0)
except Exception as e:
    print(f"Error initializing ADS1115: {e}")
    ads = None
    adc = None

# Read ADC values
def read_adc():
    if adc:
        try:
            adc_value = adc.value
            if adc_value < 275:
                adc_value = 275
            voltage = ((adc_value - 275) / 65535.0) * (3.3 - 0.012)
            microns = voltage / (3.3 - 0.012) * 25400
            return voltage, microns
        except Exception as e:
            print(f"Error reading ADC: {e}")
            return 0, 0
    else:
        return 0, 0

# Calculate moving average of ADC values over 100 readings
def mean_adc():
    microns_values = []
    voltages_values = []
    for i in range(100):
        voltage, microns = read_adc()
        if voltage is not None and microns is not None:
            microns_values.append(microns)
            voltages_values.append(voltage)
        time.sleep(0.1)  # Adjust this delay based on measurement speed
    if microns_values and voltages_values:
        return sum(microns_values) / len(microns_values), sum(voltages_values) / len(voltages_values)
    else:
        return 0, 0

# Send data without waiting for acknowledgement
def send_data_without_ack(data):
    try:
        rfm9x.send(data)
        print("Data sent without waiting for acknowledgement")
    except Exception as e:
        print(f"Error sending data: {e}")

# Send data with retry and acknowledgement
def send_data_with_retry(data, retries=5):
    for attempt in range(retries):
        try:
            rfm9x.send(data)
            print("Data sent, waiting for acknowledgement...")

            # Wait for acknowledgement for a certain time (e.g., 2 seconds)
            ack = rfm9x.receive(timeout=2.0)
            if ack is not None:
                print("Acknowledgement received.")
                return True
            else:
                print(f"No acknowledgement received. Retry {attempt + 1}/{retries}")
        except Exception as e:
            print(f"Error sending data on attempt {attempt + 1}/{retries}: {e}")

    print("Failed to send data after maximum retries.")
    return False

# Set CSV filename based on current date and time
def set_csv_filename():
    global csv_filename
    current_time = time.localtime()
    csv_filename = "/data_log_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}.csv".format(
        current_time.tm_year, current_time.tm_mon, current_time.tm_mday,
        current_time.tm_hour, current_time.tm_min, current_time.tm_sec
    )

# Check if a file exists
def file_exists(filename):
    try:
        with open(filename, 'r') as f:
            pass
        return True
    except OSError:
        return False

# Save data to CSV file
def save_to_csv(data):
    global csv_filename
    try:
        # Set filename if not already set
        if csv_filename is None:
            set_csv_filename()

        # Check if file already exists
        file_exists_flag = file_exists(csv_filename)

        with open(csv_filename, "a",) as csvfile:
            writer = csv.writer(csvfile)
            # Write header if file doesn't exist yet
            if not file_exists_flag:
                writer.writerow([
                    "Year", "Month", "Day", "Hour", "Minute", "Second",
                    "Dendrometer(uM)", "Pressure(hPa)", "Temp SHT41(C)",
                    "Humidity(%)", "Moisture Level"
                ])
            writer.writerow(data)
    except Exception as e:
        print(f"Error writing to CSV: {e}")

# Main function to start measurement mode
def start_mes_mode():
    global csv_filename
    start_time = time.monotonic()
    interval = 20  # 5 minutes
    while True:
        current_time = time.monotonic()

        # If 30-minute interval has passed
        if current_time - start_time >= interval:
            mean_microns, mean_voltages = mean_adc()
            print('mean microns=' + str(mean_microns))

            temperature_dps310, pressure = test_dps310()
            print('temperature_dps310=' + str(temperature_dps310))
            print('pressure=' + str(pressure))

            temperature_sht41, humidity = test_sht41()
            print('temperature_sht41=' + str(temperature_sht41))
            print('humidity=' + str(humidity))

            moisture = read_moisture()
            print("moisture =" + str(moisture))

            # Update and display current time
            current_time_struct = time.localtime()
            hours = current_time_struct.tm_hour
            minutes = current_time_struct.tm_min
            seconds = current_time_struct.tm_sec

            year = current_time_struct.tm_year
            month = current_time_struct.tm_mon
            day = current_time_struct.tm_mday

            data = [year, month, day, hours, minutes, seconds, mean_microns, pressure, temperature_sht41, humidity, moisture]
            save_to_csv(data)  # Save data to CSV file

            data = f"{year}/{month}/{day} {hours}:{minutes}:{seconds},Dendro: {mean_microns}, Press: {pressure}, Temp: {temperature_sht41}, Hum: {humidity}, Moisture: {moisture}"
            #send_data_without_ack(bytes(data, "UTF-8"))
            send_data_with_retry(bytes(data, "UTF-8"))
            print(f"Filtered value: {mean_microns}")

            # Reset start time for next 30-minute interval
            start_time = current_time

        # Check for stop measurement mode
        if b4.value:
            display.wake()
            clear_display()
            text_str = "B4 to stop measure"
            text_label.text = text_str

            layer.append(text_label)
            display.show(layer)

            time.sleep(1)

            # Wait for confirmation
            confirmation_time = time.monotonic()
            while time.monotonic() - confirmation_time < 20:
                if b4.value:
                    # Stop measurements
                    clear_display()
                    os.rename('/boot.py', '/boot.bak')
                    microcontroller.reset()
                    return 0
                if b1.value:
                    # Continue measurements
                    display.sleep()
                    clear_display()
                    break
                time.sleep(0.1)
            else:
                # Continue measurements after timeout
                display.sleep()
                clear_display()

        # Wait for a short period before next check
        time.sleep(1)

# Place the rest of the code needed to initialize sensors, display, LoRa, etc.

# Global variables for initial setup
first_start = time.monotonic()
timeout_menu = False

while True:
    if enable_menu:
        clear_display()
        layer.append(instruction_label1)
        layer.append(instruction_label2)
        display.show(layer)

        if time.monotonic() - first_start >= 60:
            timeout_menu = True

    if b1.value:
        clear_display()
        enable_menu = False
        set_time_mode()
        enable_menu = True

    if b2.value or timeout_menu:
        clear_display()
        enable_menu = False
        display.sleep()
        start_mes_mode()
        first_start = time.monotonic()
        enable_menu = True

    # Update and display current time
    current_time = time.localtime()
    hours = current_time.tm_hour
    minutes = current_time.tm_min
    seconds = current_time.tm_sec

    year = current_time.tm_year
    month = current_time.tm_mon
    day = current_time.tm_mday

    time.sleep(1)