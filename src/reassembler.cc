#include "reassembler.hh"

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring) {
    uint64_t asize = Writer {output_}.available_capacity();
    /* data sent before or beyond capacityl, drop it */
    if (first_index + data.size() < expected_begin_ or expected_begin_ + asize < first_index) {
        return;
    }

    if (is_last_substring) {
        last_index_ = first_index + data.size();
    }

    /* cut off bytes beyond available capacity, if necessary */
    storage_.push({first_index, move(data).substr(0, expected_begin_ + asize - first_index)});

    /* expected bytes received */
    if (storage_.top().first <= expected_begin_) {
        output();
    }
}

void Reassembler::output() {
    uint64_t asize {};
    Writer& writer_ = static_cast<Writer&>(output_);
    while (!storage_.empty()) {
        auto IndexData = storage_.top();
        auto& first_index_ = IndexData.first;
        auto& data_ = IndexData.second;
        asize = writer_.available_capacity();

        /* minimum first_index is beyond expected */
        if (first_index_ > expected_begin_) {
            break;
        }

        storage_.pop();

        /* data sent before */
        if (expected_begin_ > data_.size() + first_index_) {
            continue;
        }

        if (data_.size() + first_index_ <= expected_begin_ + asize) { // within capacity
            writer_.push(data_.substr(expected_begin_ - first_index_));
            expected_begin_ += data_.size() - expected_begin_ + first_index_;
            if (first_index_ + data_.size() >= last_index_) {
                writer_.close();
            }
        } else { // beyond capacity
            writer_.push(data_.substr(expected_begin_ - first_index_, asize));
            expected_begin_ += asize;
            // if (expected_begin_ < data_.size() + first_index_) {
            //     storage_.push({expected_begin_, data_.substr(expected_begin_ - first_index_)});
            // }
            // break;
        }
    }
}

uint64_t Reassembler::bytes_pending() const {
    uint64_t total_bytes_stored_ {};
    uint64_t prev_index_ {};
    queue<IndexString> temp {};
    uint64_t asize = Writer {output_}.available_capacity();

    /* well, just mutable */
    while (!storage_.empty()) {
        auto IndexData = storage_.top();
        auto& first_index_ = IndexData.first;
        auto& data_ = IndexData.second;
        storage_.pop();

        if (first_index_ + data_.size() < expected_begin_) {
            continue;
        }

        if (first_index_ > expected_begin_ + asize) {
            break;
        }

        if (prev_index_ == 0) {
            prev_index_ = first_index_;
        }

        if (first_index_ + data_.size() >= prev_index_) {
            uint64_t offset = first_index_ >= prev_index_ ? 0 : prev_index_ - first_index_;
            if (first_index_ < expected_begin_) {
                if (data_.size() + first_index_ > asize + expected_begin_) {
                    total_bytes_stored_ += asize;
                }
                total_bytes_stored_ += data_.size() - expected_begin_ + first_index_;
            } else {
                if (data_.size() + first_index_ > asize + expected_begin_) {
                    total_bytes_stored_ += asize + expected_begin_ - first_index_;
                }
                total_bytes_stored_ += data_.size() - offset;
            }
            prev_index_ = first_index_ + data_.size();
        }
        temp.push({first_index_, move(data_)});
    }

    while (!temp.empty()) {
        auto IndexData = temp.front();
        auto& first_index_ = IndexData.first;
        auto& data_ = IndexData.second;
        temp.pop();
        storage_.push({first_index_, move(data_)});
    }
    return total_bytes_stored_;
}
