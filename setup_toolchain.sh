#!/bin/bash
# Install deps
sudo pacman -Syu --needed \
  git cmake ninja base-devel python \
  arm-none-eabi-gcc arm-none-eabi-newlib \
  openocd gdb \
  libusb

# Clone pico sdk
mkdir -p ~/pico
cd ~/pico
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init

# Add pico sdk to path 
echo 'export PICO_SDK_PATH=$HOME/pico/pico-sdk' >> ~/.bashrc
source ~/.bashrc

# Get picotool, build and test it
cd ~/pico
git clone https://github.com/raspberrypi/picotool.git
cd picotool
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
sudo cmake --install .

picotool help

# Setup udev rules
sudo tee /etc/udev/rules.d/99-rp2040.rules >/dev/null <<'EOF'
# RP2040 BOOTSEL device (RPI-RP2)
SUBSYSTEM=="usb", ATTRS{idVendor}=="2e8a", ATTRS{idProduct}=="0003", MODE="0666"
EOF

sudo udevadm control --reload-rules
sudo udevadm trigger

# Get pico examples and test sdk
cd ~/pico
git clone https://github.com/raspberrypi/pico-examples.git
cd pico-examples
mkdir -p build && cd build
cmake -G Ninja ..
ninja 
