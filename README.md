Tretroller
==========

This project is intended to light up a scooter. It offers a almost non-invasive
speed measurement using a hall sensor and a magnet glued to the wheel,
and lots of WS2812b led strips.

A [cheap blue pill](https://hackaday.com/2017/03/30/the-2-32-bit-arduino-with-debugging/)
STM32F103 microcontroller board is used to control everything.

stolen and adapted from https://github.com/hwhw/stm32-projects @ `a1de68cbee09631836c205106fbc2fd64d2a93ec`.
The [original readme has been preserved here](firmware/readme_orig.md)


Hardware Setup
--------------

You need a strong 5V power supply (1-5 ampere, the LEDs consume lots of power), a
blue pill board (or any other STM32F103 board, actually), and a WS2812b LED strip
(preferably a waterproof one, depending on what you want to do with it).
I use a LM2596 DC/DC-converter to generate 5V from a 3-cell LiPo battery.

Connect the 5V power supply and the GND with the blue pilland with the LED strip.
The LED strip's data in pin goes to PA7, and the tacho input is at PA8.
I use an open-collector hall sensor (A3144, deprecated) with an 1k pull-up.


Building and Flashing
---------------------

```
cd firmware

git submodule update --init
cd libopencm3 && make -j5 && cd ..

cd src
make
stm32flash -w tretroller.bin /dev/ttyUSB0
```

For flashing, you need a USB-serial-converter. Connect its RX/TX pins to PA9/PA10.
(And don't forget GND.)


License
-------

The source code is licensed under the terms of the
[3-clause BSD license](https://opensource.org/licenses/BSD-3-Clause).
This project uses the [libopencm3](https://libopencm3.org/) library, which is
licensed as [GPL3 or any later version](https://www.gnu.org/licenses/lgpl-3.0.en.html),
so same terms apply to the resulting binary.
