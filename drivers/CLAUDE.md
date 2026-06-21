This is OSv's driver directory. 

## Structure
- `nic.hh`, `nic.cc` setup and attach for a NIC driver
    - concrete driver setup flows are handled by minidpdk
    - nic driver interface in this directory serves as delegator
