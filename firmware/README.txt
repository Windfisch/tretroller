stolen and adapted from https://github.com/hwhw/stm32-projects @ a1de68cbee09631836c205106fbc2fd64d2a93ec

git submodule update --init
cd libopencm3 && make -j5 && cd..

cd ws2812
make
stm32flash -w ws2812.bin /dev/ttyUSB0
