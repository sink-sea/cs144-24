#include "reassembler.hh"

using namespace std;

void Reassembler::insert(uint64_t first_index, string data, bool is_last_substring) {
    auto asize = Writer {output_}.available_capacity();

    /* data sent before or beyond capacity, drop it */
    if (first_index + data.size() < expected_begin_ or expected_begin_ + asize < first_index) {
        return;
    }

    /* last substring, store the end position */
    if (is_last_substring) {
        last_index_ = first_index + data.size();
    }

    /* cut off bytes beyond available capacity, if necessary */
    storage_.emplace(pair {first_index, move(data).substr(0, expected_begin_ + asize - first_index)});

    /* expected bytes received, output */
    if (storage_.top().first <= expected_begin_) {
        output();
    }
}

void Reassembler::output() {
    Writer& writer_ = static_cast<Writer&>(output_);

    while (not storage_.empty()) {
        auto [first_index_, data_] = storage_.top();

        /* minimum first_index is beyond expected, break */
        if (first_index_ > expected_begin_) {
            break;
        }

        storage_.pop();

        /* data sent before, skip */
        if (expected_begin_ > first_index_ + data_.size()) {
            continue;
        }

        /* data should be within capacity, just push */
        writer_.push(data_.substr(expected_begin_ - first_index_));
        expected_begin_ += data_.size() - expected_begin_ + first_index_;

        /* last substring, close the writer */
        if (first_index_ + data_.size() >= last_index_) {
            writer_.close();
        }
    }
}

uint64_t Reassembler::bytes_pending() const {
    uint64_t total_bytes_stored_ {};
    uint64_t prev_index_ {};
    vector<IndexString> temp {};

    /* well, just make storage queue mutable... */
    while (not storage_.empty()) {
        auto [first_index_, data_] = storage_.top();
        storage_.pop();

        if (prev_index_ == 0) {
            prev_index_ = first_index_;
        }

        /* completely covered by another byte */
        if (first_index_ + data_.size() < prev_index_) {
            continue;
        }

        auto offset {first_index_ > prev_index_ ? 0 : prev_index_ - first_index_};

        /* beginning of storage */
        if (first_index_ < expected_begin_) {
            offset = expected_begin_ - first_index_;
        }

        total_bytes_stored_ += data_.size() - offset;
        prev_index_ = first_index_ + data_.size();

        temp.push_back({first_index_, move(data_)});
    }

    for (auto& IndexData : temp) {
        storage_.emplace(move(IndexData));
    }
    return total_bytes_stored_;
}
