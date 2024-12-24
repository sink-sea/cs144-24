#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const {
    return last_byte_sent_ - last_byte_acked_;
}

uint64_t TCPSender::consecutive_retransmissions() const {
    return retransmission_cnt_;
}

void TCPSender::push(const TransmitFunction& transmit) {
    auto& reader_ = static_cast<Reader&>(this->input_);

    /* send as much as possible */
    while (not fin_) {
        auto send_msg {make_empty_message()};
        auto max_payload_len = min((uint64_t)rwnd_, TCPConfig::MAX_PAYLOAD_SIZE) - send_msg.SYN;

        send_msg.payload = reader_.peek().substr(0, max_payload_len);
        reader_.pop(send_msg.payload.size());

        uint64_t seq_len = send_msg.sequence_length();
        rwnd_ = (rwnd_ > seq_len) ? rwnd_ - seq_len : 0;
        send_msg.FIN = rwnd_ != 0 and reader_.is_finished();
        fin_ = send_msg.FIN;
        rwnd_ -= send_msg.FIN;
        seq_len += send_msg.FIN;

        if (seq_len == 0 and not send_msg.RST) {
            return;
        }

        /* first bytes sent, wait for new rwnd */
        if (last_byte_sent_ == 0) {
            rwnd_ = 0;
        }

        last_byte_sent_ += seq_len;
        sending_bytes_.emplace(send_msg);
        transmit(send_msg);

        /* reset message sent */
        if (send_msg.RST) {
            return;
        }
    }
}

TCPSenderMessage TCPSender::make_empty_message() const {
    return {Wrap32::wrap(last_byte_sent_, isn_), last_byte_sent_ == 0, {}, {}, writer().has_error()};
}

void TCPSender::receive(const TCPReceiverMessage& msg) {
    /* has error */
    if (msg.RST) {
        writer().set_error();
    }

    if (not msg.ackno.has_value()) {
        return;
    }

    /* ACK number */
    auto ackno = msg.ackno.value().unwrap(isn_, last_byte_acked_);
    if (ackno > last_byte_sent_) { /* invalid ack */
        return;
    }

    while (!sending_bytes_.empty()) {
        auto seq_len = sending_bytes_.front().sequence_length();
        if (ackno < last_byte_acked_ + seq_len) { /* incomplete ack */
            break;
        }

        /* transmitted successfully, pop and update ackno */
        sending_bytes_.pop();
        last_byte_acked_ += seq_len;

        /* reset timer */
        timer_ = 0;
        retransmission_cnt_ = 0;
        RTO_ms_ = initial_RTO_ms_;
    }

    if (msg.window_size == 0) {
        rwnd_ = 1; // treat window_size 0 as 1
    } else if (msg.window_size > sequence_numbers_in_flight()) {
        rwnd_ = msg.window_size - sequence_numbers_in_flight();
    }

    zero_rwnd_ = msg.window_size == 0; // rwnd ?= 0
}

void TCPSender::tick(uint64_t ms_since_last_tick, const TransmitFunction& transmit) {
    if (sending_bytes_.empty()) { /* no bytes sending */
        return;
    }
    timer_ += ms_since_last_tick;

    if (timer_ >= RTO_ms_) {
        transmit(sending_bytes_.front()); // retransmission
        if (not zero_rwnd_) {
            retransmission_cnt_++;
            RTO_ms_ *= 2;
        }
        timer_ = 0;
    }
}
