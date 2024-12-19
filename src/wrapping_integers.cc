#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap(uint64_t n, Wrap32 zero_point) {
    return Wrap32 {static_cast<uint32_t>(n) + zero_point.raw_value_};
}

uint64_t Wrap32::unwrap(Wrap32 zero_point, uint64_t checkpoint) const {
    constexpr uint64_t Seq_Offset_ = 1ul << 32;
    uint64_t abs_seqno {this->raw_value_ - zero_point.raw_value_};
    auto bias = (abs_seqno > checkpoint) ? abs_seqno - checkpoint : checkpoint - abs_seqno;
    auto factor = (bias / (Seq_Offset_ / 2)) % 2;
    if (abs_seqno > checkpoint) {
        abs_seqno -= bias / Seq_Offset_ * Seq_Offset_;
        abs_seqno -= (abs_seqno > Seq_Offset_) ? factor * Seq_Offset_ : 0;
    } else {
        abs_seqno += bias / Seq_Offset_ * Seq_Offset_;
        abs_seqno += (abs_seqno + Seq_Offset_ > abs_seqno) ? factor * Seq_Offset_ : 0;
    }

    return abs_seqno;
}
