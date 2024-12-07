#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive(TCPSenderMessage message) {
    if (message.SYN) {
        zero_point_ = move(message.seqno);
        syn_ = true;
    }

    if (message.RST) {
        reader().set_error();
    }

    reassembler_.insert(
      message.seqno.unwrap(zero_point_ + !message.SYN, writer().bytes_pushed()), message.payload, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const {
    auto writer_ = this->writer();
    auto ackno_
      = syn_ ? optional {Wrap32::wrap(writer_.bytes_pushed() + syn_ + writer_.is_closed(), zero_point_)} : nullopt;
    uint16_t wnd_size_
      = writer_.available_capacity() > UINT16_MAX ? UINT16_MAX : (uint16_t)writer_.available_capacity();
    return {ackno_, wnd_size_, writer_.has_error()};
}
