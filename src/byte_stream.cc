#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream(uint64_t capacity) : capacity_(capacity) {}

bool Writer::is_closed() const {
    // Your code here.
    return closed_;
}

void Writer::push(string data) {
    // Your code here.
    if (is_closed() || has_error()) {
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
    // Your code here.
    closed_ = true;
}

uint64_t Writer::available_capacity() const {
    // Your code here.
    // return capacity_ - buffer_.size();
    return capacity_ - total_bytes_buffered_;
}

uint64_t Writer::bytes_pushed() const {
    // Your code here.
    return total_bytes_pushed_;
}

bool Reader::is_finished() const {
    // Your code here.
    return closed_ && (total_bytes_buffered_ == 0);
}

uint64_t Reader::bytes_popped() const {
    // Your code here.
    return total_bytes_popped_;
}

string_view Reader::peek() const {
    // Your code here
    return buffer_.empty() ? string_view {} : string_view {buffer_.front()}.substr(poped_prefix);
}

void Reader::pop(uint64_t len) {
    // Your code here.
    if (len > bytes_buffered()) {
        error_ = true;
        return;
    }

    total_bytes_popped_ += len;
    total_bytes_buffered_ -= len;

    while (len > 0 && !buffer_.empty()) {
        auto str_size_left = buffer_.front().size() - poped_prefix;
        if (str_size_left <= len) {
            poped_prefix = 0;
            len -= str_size_left;
            buffer_.pop();
        } else {
            poped_prefix += len;
            break;
        }
    }
}

uint64_t Reader::bytes_buffered() const {
    // Your code here.
    return total_bytes_buffered_;
}
