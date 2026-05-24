.. _greybus-basic-sample:

Greybus Basic Sample
####################

Overview
********

This sample demonstrates a minimal setup for using the **Greybus service** in Zephyr.
Greybus provides a modular protocol framework originally developed for Project Ara and
is now used in embedded systems for abstracting peripheral communication.

The **Greybus Basic** sample shows how to initialize and use Greybus with different
transport backends, such as dummy and TCP/IP transports.

Building and Running
********************

This sample supports multiple transport configurations through ``EXTRA_CONF_FILE``.
You can select the desired configuration by passing the appropriate argument at build time.

Supported Transports
====================

1. **Dummy Transport**

   A simple in-memory transport used for testing Greybus functionality without requiring
   any physical transport layer. Useful for checking greybus manifest.

   .. code-block:: bash

      west build -b beagleconnect_freedom/cc1352p7 samples/greybus/basic \
          -- -DEXTRA_CONF_FILE="transport-dummy.conf"

2. **TCP/IP Transport**

   A network-based transport allowing Greybus communication over TCP/IP.  
   This configuration also includes the optional ``802154-subg.conf`` overlay for
   enabling sub-GHz IEEE 802.15.4 communication if supported.

   .. code-block:: bash

      west build -b beagleconnect_freedom/cc1352p7 samples/greybus/basic \
          -- -DEXTRA_CONF_FILE="transport-tcpip.conf;802154-subg.conf"

Requirements
************

- Zephyr
- Greybus subsystem enabled
- A supported platform, currently:
  
  - ``beagleconnect_freedom/cc1352p7``

Testing
*******

The following test configurations are defined in :file:`sample.yaml`:

- ``sample.greybus.basic.transport.dummy``  
  Builds the sample using ``transport-dummy.conf``.
- ``sample.greybus.basic.transport.tcpip``  
  Builds the sample using ``transport-tcpip.conf`` and ``802154-subg.conf``.

These are build-only tests verified on the ``beagleconnect_freedom/cc1352p7`` platform.

References
**********

- `Greybus subsystem documentation <https://docs.zephyrproject.org/latest/services/greybus/index.html>`_
- `BeagleConnect Freedom board documentation <https://docs.beagleboard.org/latest/boards/beagleconnect/freedom.html>`_
