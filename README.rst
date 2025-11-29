.. _greybus_for_zephyr:

******************
Greybus for Zephyr
******************

Overview
########
This repository contains a `Greybus <https://lwn.net/Articles/715955/>`_
`module <https://docs.zephyrproject.org/latest/guides/modules.html>`_ for the
`Zephyr Real-Time Operating System <https://zephyrproject.org/>`_.

Adding Greybus to Zephyr
#########################

First, ensure that all required tools are installed by following Zephyr's
`Getting Started Guide <https://docs.zephyrproject.org/latest/getting_started/index.html>`_.

Add following entry to west.yml file in manifest/projects subtree of Zephyr:

.. code-block::

    # Greybus repository
    - name: Greybus-Zephyr
      path: modules/lib/greybus
      revision: main
      url: https://github.com/beagleboard/greybus-zephyr

Then, clone the repository by running

.. code-block:: bash

    west update

Building and Running
####################

The current testing has been performed using mainline Linux kernel (6.12) on `BeaglePlay <www.beagleboard.org/boards/beagleplay>`_ with cc1352p7 running `greybus-host firmware <https://github.com/Ayush1325/cc1352-firmware>`_ and `BeagleConnect Freedom <https://www.beagleboard.org/boards/beagleconnect-freedom>`_ running greybus-for-zephyr.

.. code-block:: bash

    west build -b beagleconnect_freedom --sysbuild -p modules/lib/greybus/samples/basic -- -DEXTRA_CONF_FILE="transport-tcpip.conf;802154-subg.conf"


The sample uses `MCUboot <https://docs.mcuboot.com/>`_ to provide OTA support. The following commands can be used to merge mcuboot and application firmware, and flash to beagleconnect freedom:

.. code-block:: bash

    cp build/mcuboot/zephyr/zephyr.bin zephyr.bin
    dd conv=notrunc bs=1024 seek=56 if=build/basic/zephyr/zephyr.signed.bin of=zephyr.bin
    cc1352_flasher --bcf zephyr.bin
    rm zephyr.bin

Using Greybus for I/O
#####################

At this point, we should be ready to perform some I/O on our remote devices
using Greybus. Currently, this module supports the protocols below. 

+---------------------------+--------+---------+-----------------+
|                           | Good   | Partial | Broken/Missing  |
+===========================+========+=========+=================+
| Audio                     |        |         | x               |
+---------------------------+--------+---------+-----------------+
| Camera                    |        |         | x               |
+---------------------------+--------+---------+-----------------+
| Component Authentication  |        |         | x               |
+---------------------------+--------+---------+-----------------+
| Firmware Download         |        | x       |                 |
+---------------------------+--------+---------+-----------------+
| Firmware Management       |        | x       |                 |
+---------------------------+--------+---------+-----------------+
| HID                       |        |         | x               |
+---------------------------+--------+---------+-----------------+
| Lights                    |        | x       |                 |
+---------------------------+--------+---------+-----------------+
| Log                       | x      |         |                 |
+---------------------------+--------+---------+-----------------+
| Loopback                  | x      |         |                 |
+---------------------------+--------+---------+-----------------+
| Power Supply              |        |         | x               |
+---------------------------+--------+---------+-----------------+
| Raw                       |        | x       |                 |
+---------------------------+--------+---------+-----------------+
| Vibrator                  | x      |         |                 |
+---------------------------+--------+---------+-----------------+
| USB                       |        |         | x               |
+---------------------------+--------+---------+-----------------+
| `GPIO <doc/gpio.rst>`_    | x      |         |                 |
+---------------------------+--------+---------+-----------------+
| SPI                       |        | x       |                 |
+---------------------------+--------+---------+-----------------+
| UART                      |        | x       |                 |
+---------------------------+--------+---------+-----------------+
| PWM                       |        | x       |                 |
+---------------------------+--------+---------+-----------------+
| `I2C <doc/i2c.rst>`_      | x      |         |                 |
+---------------------------+--------+---------+-----------------+
| SDIO                      |        |         | x               |
+---------------------------+--------+---------+-----------------+

Additional Information
#######################

A compiled version of the `Greybus Specification <https://github.com/projectara/greybus-spec>`_
is available `here <doc/GreybusSpecification.pdf>`_.

Other Media
************

- Greg Kroah-Hartman, Project Ara DC 2015: https://www.youtube.com/watch?v=UzRq8jAHAxU
- Alexandre Baillon, ELCE 2016
        - Slides: https://elinux.org/images/c/cb/Using_Greybus_for_IoT.pdf
        - Video: https://www.youtube.com/watch?v=7H50pv-4YXw
- Alexandre Baillon, LPC 2019
        - Slides: https://linuxplumbersconf.org/event/4/contributions/375/attachments/335/561/greybus_for_iot.pdf
        - Video: https://www.youtube.com/watch?v=bVQ_mpvTCIM
- Jason Kridner, LPC 2019
        - Slides: https://linuxplumbersconf.org/event/4/contributions/437/attachments/341/568/beagledust.pdf
        - Video: https://www.youtube.com/watch?v=bVQ_mpvTCIM&t=9393
- Christopher Friedt, LPC 2020 
        - Materials: https://linuxplumbersconf.org/event/7/contributions/814/
        - Video: https://youtu.be/n4yiCF2wYeo?t=11683
- Zephyr Project: Using Linux, Zephyr, & Greybus for IoT (October 15, 2020): https://www.zephyrproject.org/using-linux-zephyr-greybus-for-iot/ 
