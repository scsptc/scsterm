# SCS Term
Simple terminal programm for the SCS modems.

## Prerequisites
On a Debian or Debian based system simply do
```
sudo apt update
sudo apt install build-essential libusb-1.0-0-dev
```

In general you need gcc, make and libusb_1.0.

## Get the source

```
git clone https://github.com/scsptc/scsterm.git
```

## Build
Change directory to the scsterm source
```
cd scsterm
```
and simply enter
```
make
```

Start with
```
./scsterm
```

Or you may copy scsterm to /usr/local/bin
```
sudo cp scsterm /usr/local/bin/
```

## Function
SCS Term searches for SCS modems (max. 8) on USB.
If more than on modem is found SCS Term presents you a list of the modems.
```
More than one SCS modem found! Plaese choose:
1: /dev/ttyUSB0     P4dragon DR-7400
2: /dev/ttyUSB1     PTC-IIIusb
Enter a number: 
```
Enter a number from the list and press enter.

If you don't want the automatic search, you can enter the device and baudrate as arguments:
```
./scsterm /dev/ttyUSB0 38400
```

**Hint:** if you get a *permission denied* error, you normally have to add the user to the group dialout!
```
sudo adduser $USER dialout
```
replace *username* with your user name.

**You must log out and back in before these group changes come into effect!**


Tested on:
- Debian 10
- Ubuntu 20.04
- Raspberry Pi OS Lite May 7th 2021
