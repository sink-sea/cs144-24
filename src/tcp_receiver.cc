#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message) {
    if (writer().is_closed()) {
        return;
    }

    if (message.SYN) {
        zero_point_ = move(message.seqno);
        syn_ = true;
    }

    if (message.RST) {
        reader().set_error();
    }

    reassembler_.insert(message.seqno.unwrap(zero_point_ + !message.SYN, writer().bytes_pushed()),
                        move(message.payload),
                        message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const {
    auto writer_ = this->writer();

    /* abs_seqno = pushed bytes + SYN + FIN, wrap it */
    auto ackno_
      = syn_ ? optional {Wrap32::wrap(writer_.bytes_pushed() + syn_ + writer_.is_closed(), zero_point_)} : nullopt;

    /* window size */
    uint16_t wnd_size_ = writer_.available_capacity() > UINT16_MAX
                           ? static_cast<uint16_t>(UINT16_MAX)
                           : static_cast<uint16_t>(writer_.available_capacity());
    return {ackno_, wnd_size_, writer_.has_error()};
}
