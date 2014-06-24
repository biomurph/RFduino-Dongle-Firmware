RFduino-Dongle-Firmware
=======================
Works to upload program to target Arduino UNO over the paired RFduino Device and Host.

Host must disconnect the reset pin from the FTDI board (or else you will program the Host...)

Target arduino must be manually reset and release timed with IDE upload (old school style)

GOALS:
  attach FTDI DTR pin to GPIO on Host and have HOST send message to DEVICE to automatically reset target Arduino
  
  test against other Arduino targets (Duemilinova, Tiny, Pro, Flora, etc)
  
  build into a library for easy portage
