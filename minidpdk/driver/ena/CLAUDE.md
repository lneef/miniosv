# ENA reference directory
This directory holds the ENA-driver reference implementations taken from DPDK.

## Structure
```
base/ Hardware Layer directly interacting with the device
base/ena_defs/ Structure Definitions/Descriptor Layouts for ENA
base/ena_plt_dpdk.h shim layer for the driver mapping MiniDPDK/DPDK functionality to excepted driver API
ena_ethdev.c Device Semantics/User expected behavior of Data and Control path
ena_ethdev.h Higher level structures for device management in the runtime/kernel
ena_logs.h Definition of driver specific logging
ena_rss.c RSS-setup and management
ena-platform.h platform specific assert 
```

