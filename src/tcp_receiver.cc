#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message) {
    if (writer().is_closed()) {
        return;
    }

    if (message.RST) {
        reader().set_error();
        return;
    }

    if (not zero_point_.has_value()) {
        if (not message.SYN) {
            return;
        }
        zero_point_.emplace(message.seqno);
    }

    reassembler_.insert(message.seqno.unwrap(zero_point_.value() + !message.SYN, writer().bytes_pushed()),
                        move(message.payload),
                        message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const {
    auto writer_ = this->writer();

    auto ackno_ = zero_point_.has_value()
                    ? optional {Wrap32::wrap(writer_.bytes_pushed() + 1 + writer_.is_closed(), zero_point_.value())}
                    : nullopt;

    /* window size */
    uint16_t wnd_size_ = writer_.available_capacity() > UINT16_MAX
                           ? static_cast<uint16_t>(UINT16_MAX)
                           : static_cast<uint16_t>(writer_.available_capacity());
    return {ackno_, wnd_size_, writer_.has_error()};
}
