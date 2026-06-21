Use this Skill WHENEVER porting or integrating a NIC-driver into MiniDPDK

You port a driver originally developed for DPDK (C-Code) to MiniDPDK -- a shim layer for DPDK style direct hardware access with   
custom extensions in OSv(C++20). The process consists of two steps.

# Step 1
Indentify the hardware/device facing layer of driver. This is often in a `base/` within the driver src. 
Integrate this layer such that no code in this layer causes any compilation errors.
Goal: Hardware facing layer compiles without error. All remaining errors are outside of this layer.
## Guidelines
- rewire the driver into osv's build system! 
- Only change code section where C++ requires stricter semantics/constructs for compilation
    - Log code sections where you had to change to comply with CPP
    - File name: `port-{driver_name}.md`
- Before changing anything identify expected behavior of the code section
- The order of operations of the code section you change is invariant. Preserve:
    - mmio access order
    - volatile qualifiers
    - byte order
    - barriers/fences (it should compile on x64 as well as aarch)
- Ensure any includes are outside of `extern "C"` blocks
- if there is a platform file (e.g. `*_plat.h`, `*_osdep.h`) acting as shim for the device facing layer, the layer h
as to compile solely with the functionality present in the shim

# Step 2
Port the application interface:
- datapath/control path
- logging
- RSS
## Guidelines
- Do not change anything in device facing files. Only if there are compilation errors make it adhere to the cpp standard/extesions
- Remove Configurability via cmdline args (keep the default args)
- OSv single process. Collpase DPDK Multiprocessing into the primary process's path 
    - strip the rte_mp IPC and secondary-process branches 
    - inline each proxied op to its direct primary call
- in case the driver uses interrupts (control/rx) proceed as state below
- in `drv_flags` on `RTE_PCI_DRV_NEED_MAPPING` is relevant, you can drop the rest
- some DPDK concepts are defined via using:
    - `rte_mempool, rte_mbuf, rte_pktmbuf_pool`
    - integrate them without a preceeding `struct`
- `rte_pci_device` is replaced by `minidpdk::pci_device *pdev`
- remove occurances of `rte_mempool_cache_flush`
- replace `rte_strerror` with `std::sterror` and libc `errno` instead of `rte_errno`
- do not set `tx_pkt_prepare` (should be `nullptr`), remove the corresponding unused prep_pkts-function
- remove any `RTE_MBUF_DYNFIELD` related code. (Fail these paths with an error and a log message)
- remove an configurability via cmdline args, keep the default values  
- remove any `RTE_PMD_REGISTER_PCI_TABLE`, `RTE_PMD_REGISTER_KMOD_DEP`
- remove any leftover build files from DPDK(meson.build)
Goal: The driver compiles only with MiniDPDK includes, i.e. no DPDK includes are left
Generate a short report listing changes in the application facing layer.
- We do not support NIC-Flows remove these files. Make sure these offloads are not advertised
- PCI-tables, KMOD-Deps can be removed 
- Remove any unused includes

### Interrupts
- MiniDPDK features real hw interrupts, DPDK only has event-fds
- Before starting to change anything make yourself familiar with the intr subsystem of MiniDPDK
- You need to change rx-queue setup functions to accept `intr_config` structs
    - `intr_config` for each rx-queue holds the handler and context
#### Workflow
- use MiniDPDK interrupts directly
    - rewrite the respective code sections
    - replace the DPDK efd based interrupts propagation completely by using MiniDPDK's handler registration interface
- msix interrupts need to be enabled upfront for the device (`dev->msix_enable`, `dev->msix_disable`)
    - only enable them before setting up the control path
    - second one is redundant
- All msix-vectors needed to be request before they can be used
    - ctrl and datapath vector are separated in MiniDPDK
    - Request Ctrl Path vectors for the ctrl path and Datapath vectors for the datapath
- register the callback
- set affinity(ctrl: mainlcore, datapath: Round Robin(pass Queue ID))
- unmask the interrupts


# Notes
- If any feature is missing from DPDK state it in (missing_{drv}.md)
- Keep functionality even if it is only a stub in MiniDPDK
