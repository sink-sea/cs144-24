#pragma once

#include <string>

#include "file_descriptor.hh"

//! A FileDescriptor to a [Linux TUN/TAP](https://www.kernel.org/doc/Documentation/networking/tuntap.txt) device
class TunTapFD : public FileDescriptor {
  public:
    //! Open an existing persistent [TUN or TAP
    //! device](https://www.kernel.org/doc/Documentation/networking/tuntap.txt).
    explicit TunTapFD(const std::string& devname, bool is_tun);
};

//! A FileDescriptor to a [Linux TUN](https://www.kernel.org/doc/Documentation/networking/tuntap.txt) device
class TunFD : public TunTapFD {
  public:
    //! Open an existing persistent [TUN device](https://www.kernel.org/doc/Documentation/networking/tuntap.txt).
    explicit TunFD(const std::string& devname) : TunTapFD(devname, true) {}
};

//! A FileDescriptor to a [Linux TAP](https://www.kernel.org/doc/Documentation/networking/tuntap.txt) device
class TapFD : public TunTapFD {
  public:
    //! Open an existing persistent [TAP device](https://www.kernel.org/doc/Documentation/networking/tuntap.txt).
    explicit TapFD(const std::string& devname) : TunTapFD(devname, false) {}
};
