# hello_world

Print "Hello World" to the RTT console. Adapted from zephyr's included sample hello_world.

## Building and running

You must have a J-Link debugger connected to the Nucleo board's MIPI20 connector (CN1), and the SEGGER J-Link tools on your `PATH`.

To load into RAM and run on board:

```console
$ west build -p always
$ python3 ../../scripts/jlink.py load
```

N657 Note: The RTT control and buffer blocks are placed in AXISRAM1, thus J-Link's RTT auto-detection may be used in the RTT viewer/client.

## Expected output

Using the CLI RTT client:

```console
$ JLinkRTTClient
*** Booting Zephyr OS build v4.4.0-17350-g1ee3b93134be ***
Hello World! nucleo_n657x0_q/stm32n657xx
```
