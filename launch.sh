/usr/bin/python3 shutdown_monitor.py &
sudo ./UVCMatrix --led-cols=64 --led-rows=32 --led-show-refresh --led-chain=12 --led-parallel=3 --led-slowdown-gpio=3 --led-pixel-mapper=U-mapper --led-limit-refresh=300 --led-no-drop-privs --led-pwm-bits=9 --led-scan-mode=1 --led-pwm-dither-bits=1
