#include <shared_ptr.h>

#include <cassert>

control_block::control_block()
    : shared_cnt(0), weak_cnt(0) {}

void control_block::release_shared() {
    assert(shared_cnt > 0);
    --shared_cnt;
}

void control_block::release_weak() {
    assert(weak_cnt > 0);
    --weak_cnt;
}

void control_block::inc_shared() {
    ++shared_cnt;
}

void control_block::inc_weak() {
    ++weak_cnt;
}

size_t control_block::get_shared_cnt() {
    return shared_cnt;
}

size_t control_block::get_weak_cnt() {
    return weak_cnt;
}
