#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point) {
    return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
    constexpr uint64_t Wrap32_Interval = 1ul << 32;
    uint64_t abs_seqno {this->raw_value_ - zero_point.raw_value_};
    auto bias = (abs_seqno > checkpoint) ? abs_seqno - checkpoint : checkpoint - abs_seqno;
    auto factor = (bias / (Wrap32_Interval / 2)) % 2;
    if (abs_seqno > checkpoint) {
        abs_seqno -= bias / Wrap32_Interval * Wrap32_Interval;
        abs_seqno -= (abs_seqno > Wrap32_Interval) ? factor * Wrap32_Interval : 0;
    } else {
        abs_seqno += bias / Wrap32_Interval * Wrap32_Interval;
        abs_seqno += (abs_seqno + Wrap32_Interval > abs_seqno) ? factor * Wrap32_Interval : 0;
    }

    return abs_seqno;
}
