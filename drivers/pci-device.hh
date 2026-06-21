/*
 * Copyright (C) 2013 Cloudius Systems, Ltd.
 *
 * This work is open source software, licensed under the terms of the
 * BSD license as described in the LICENSE file in the top-level directory.
 */

#ifndef PCI_DEVICE_H
#define PCI_DEVICE_H

#include <osv/types.h>
#include "pci-function.hh"

namespace pci {

    class device: public function {
    public:
        using iterator = std::map<int, bar *>::const_iterator;
        device(u8 bus, u8 device, u8 func);
        virtual ~device();

        // Parse configuration space of pci_device
        virtual bool parse_pci_config();

        u16 get_subsystem_id();
        u16 get_subsystem_vid();

        virtual void dump_config();

        iterator begin() const { return _bars.begin(); }
        iterator end() const { return _bars.end(); }

    protected:

        u16 _subsystem_vid;
        u16 _subsystem_id;

    };


}

#endif
