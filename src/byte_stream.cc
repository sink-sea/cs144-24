#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity) {}

bool Writer::is_closed() const {
    return closed_;
}

void Writer::push(string data) {
    if (is_closed() or has_error()) {
        return;
    }
    
    auto asize = min(available_capacity(), data.size());
    if (asize == 0) {
        return;
    }

    total_bytes_pushed_ += asize;
    buffer_.push(data.substr(0, asize));
    total_bytes_buffered_ += asize;
}

void Writer::close() {
    closed_ = true;
}

uint64_t Writer::available_capacity() const {
    return capacity_ - total_bytes_buffered_;
}

uint64_t Writer::bytes_pushed() const {
    return total_bytes_pushed_;
}

bool Reader::is_finished() const {
    return closed_ and (total_bytes_buffered_ == 0);
}

uint64_t Reader::bytes_popped() const {
    return total_bytes_popped_;
}

string_view Reader::peek() const {
    return buffer_.empty() ? string_view {} : string_view {buffer_.front()}.substr(prefix_bytes_poped_);
}

void Reader::pop(uint64_t len) {
    if (len > bytes_buffered()) {
        return;
    }

    total_bytes_popped_ += len;
    total_bytes_buffered_ -= len;

    /* lazy pop, record the poped prefix length */
    while (len > 0) {
        auto str_size_left = buffer_.front().size() - prefix_bytes_poped_;
        if (str_size_left <= len) {
            prefix_bytes_poped_ = 0;
            len -= str_size_left;
            buffer_.pop();
        } else {
            prefix_bytes_poped_ += len;
            break;
        }
    }
}

uint64_t Reader::bytes_buffered() const {
    return total_bytes_buffered_;
}
