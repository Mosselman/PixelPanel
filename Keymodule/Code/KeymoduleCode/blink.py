from machine import Pin
from neopixel import NeoPixel

pin = Pin(18, Pin.OUT)   # set GPIO0 to output to drive NeoPixels
np = NeoPixel(pin, 5)   # create NeoPixel driver on GPIO0 for 5 pixels
np[0] = (100, 100, 100) # set the first pixel to white
np.write()              # write data to all pixels
r, g, b = np[0]         # get first pixel colour