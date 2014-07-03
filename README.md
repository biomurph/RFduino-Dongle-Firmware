RFduino-Dongle-Firmware
=======================
Works to upload program to target Arduino UNO over the paired RFduino Device and Host.

Host must disconnect the reset pin from the FTDI board, or else you will program the Host.

Connect FTDI DTR pin to GPIO6 on Host RFduino.
Device RFduino GPIO6 will mirror state of Host GPIO6.
Connect GPIO6 to Arduino UNO reset pin through a 1uF cap.

GOALS:
  
  test against other Arduino targets (Duemilinova, Tiny, Pro, Flora, etc)
  
  build into a library for easy portage
