#pragma once
#include <cstddef>
#include <type_traits>
#include <memory>
#include <utility>

struct control_block {
    control_block();

    void release_shared();
    void release_weak();

    void inc_shared();
    void inc_weak();

    size_t get_shared_cnt();
    size_t get_weak_cnt();

    virtual ~control_block() = default;
    virtual void delete_object() = 0;

 private:
    size_t shared_cnt;
    size_t weak_cnt;
};


template <typename Y, typename Deleter>
struct cb_separate : control_block, Deleter {
    explicit cb_separate(Y* ptr, Deleter d) noexcept
        : ptr(ptr), Deleter(std::move(d))  {}

    void delete_object() override {
        assert(get_shared_cnt() == 0);
        static_cast<Deleter&>(*this)(ptr);
    }

 private:
    Y* ptr;
};


template <typename Y>
struct cb_inplace : control_block {
    template <typename ...Args>
    explicit cb_inplace(Args&& ...args) {
        new (&data) Y(std::forward<Args>(args)...);
    }

    void delete_object() override {
        assert(get_shared_cnt() == 0);
        reinterpret_cast<Y*>(&data)->~Y();
    }

    Y* get() noexcept {
        return reinterpret_cast<Y*>(&data);
    }

 private:
    typename std::aligned_storage_t<sizeof(Y), alignof(Y)>::type data;
};

template <typename T>
struct weak_ptr;

template <typename T>
struct shared_ptr {
    shared_ptr() noexcept
        : ptr(nullptr), cb(nullptr) {}

    shared_ptr(std::nullptr_t) noexcept
        : shared_ptr() {}

    template<class Y>
    explicit shared_ptr(Y* ptr)
        : shared_ptr(ptr, std::default_delete<Y>()) {}

    template<class Y, class Deleter>
    shared_ptr(Y* ptr, Deleter d)
    try : ptr(ptr), cb(new cb_separate<Y, Deleter>(ptr, d)) {
        cb->inc_shared();
    } catch (...) {
        d(ptr);
        throw;
    }


    template<class Y>
    shared_ptr(const shared_ptr<Y>& r, T* ptr) noexcept
        : ptr(ptr), cb(r.cb) {
        if (cb != nullptr) {
            cb->inc_shared();
        }
    }

    shared_ptr(const shared_ptr& r) noexcept
        : shared_ptr(r, r.ptr) {}

    template<class Y>
    shared_ptr(const shared_ptr<Y>& r) noexcept
        : shared_ptr(r, r.ptr) {}

    shared_ptr(shared_ptr&& r) noexcept
        : ptr(r.ptr), cb(r.cb) {
        r.ptr = nullptr;
        r.cb = nullptr;
    }

    template<class Y>
    shared_ptr(shared_ptr<Y>&& r) noexcept
        : ptr(r.ptr), cb(r.cb) {
        r.ptr = nullptr;
        r.cb = nullptr;
    }

    ~shared_ptr() {
        if (cb != nullptr) {
            ptr = nullptr;
            cb->release_shared();
            if (cb->get_shared_cnt() == 0) {
                cb->delete_object();
            }
            if (cb->get_shared_cnt() == 0 && cb->get_weak_cnt() == 0) {
                delete cb;
                cb = nullptr;
            }
        }
    }

    shared_ptr& operator=(const shared_ptr& r) noexcept {
        if (this != &r) {
            shared_ptr(r).swap(*this);
        }
        return *this;
    }

    template<class Y>
    shared_ptr& operator=(const shared_ptr<Y>& r) noexcept {
        shared_ptr<Y>(r).swap(*this);
        return *this;
    }

    shared_ptr& operator=(shared_ptr&& r) noexcept {
        if (this != &r) {
            shared_ptr(std::move(r)).swap(*this);
        }
        return *this;
    }

    template<class Y>
    shared_ptr& operator=(shared_ptr<Y>&& r) noexcept {
        shared_ptr<Y>(std::move(r)).swap(*this);
        return *this;
    }

    void reset() noexcept {
        shared_ptr().swap(*this);
    }

    template<class Y>
    void reset(Y* ptr) {
        shared_ptr<T>(ptr).swap(*this);
    }

    template<class Y, class Deleter>
    void reset(Y* ptr, Deleter d) {
        shared_ptr<T>(ptr, d).swap(*this);
    }

    void swap(shared_ptr& r) noexcept {
        using std::swap;
        swap(ptr, r.ptr);
        swap(cb, r.cb);
    }

    T* get() const noexcept {
        return ptr;
    }

    T& operator*() const noexcept {
        return *get();
    }

    T* operator->() const noexcept {
        return get();
    }

    [[nodiscard]] size_t use_count() const noexcept {
        return cb == nullptr ? 0 : cb->get_shared_cnt();
    }

    explicit operator bool() const noexcept {
        return get() != nullptr;
    }

    template<class Y, class... Args>
    friend shared_ptr<Y> make_shared(Args&&... args);

    template <class Y, class U>
    friend bool operator==(const shared_ptr<Y>& lhs,
                           const shared_ptr<U>& rhs) noexcept;

    template <class Y, class U>
    friend bool operator!=(const shared_ptr<Y>& lhs,
                           const shared_ptr<U>& rhs) noexcept;

    template<class Y>
    friend bool operator==(const std::shared_ptr<Y>& lhs, std::nullptr_t) noexcept;

    template<class Y>
    friend bool operator==(std::nullptr_t, const std::shared_ptr<Y>& rhs) noexcept;


    template<class Y>
    friend bool operator!=(const std::shared_ptr<Y>& lhs, std::nullptr_t) noexcept;

    template<class Y>
    friend bool operator!=(std::nullptr_t, const std::shared_ptr<Y>& rhs) noexcept;

private:
    T* ptr;
    control_block* cb;

    template <typename U>
    friend struct shared_ptr;

    friend class weak_ptr<T>;

    template <typename Y>
    shared_ptr(Y* ptr, control_block* cb)
        : ptr(ptr), cb(cb) {}
};

template<class Y, class... Args>
shared_ptr<Y> make_shared(Args&&... args) {
    auto p = new cb_inplace<Y>(args...);
    p->inc_shared();
    return shared_ptr<Y>(p->get(), static_cast<control_block*>(p));
}

template <class Y, class U>
bool operator==(const shared_ptr<Y>& lhs,
                const shared_ptr<U>& rhs) noexcept {
    return lhs.get() == rhs.get();
}

template <class Y, class U>
bool operator!=(const shared_ptr<Y>& lhs,
                const shared_ptr<U>& rhs) noexcept {
    return lhs.get() != rhs.get();
}

template<class Y>
bool operator==(const shared_ptr<Y>& lhs, std::nullptr_t) noexcept {
    return !lhs;
}

template<class Y>
bool operator==(std::nullptr_t, const shared_ptr<Y>& rhs) noexcept {
    return !rhs;
}

template<class Y>
bool operator!=(const shared_ptr<Y>& lhs, std::nullptr_t) noexcept {
    return (bool)lhs;
}

template<class Y>
bool operator!=(std::nullptr_t, const shared_ptr<Y>& rhs) noexcept {
    return (bool)rhs;
}


template <typename T>
struct weak_ptr {
    weak_ptr() noexcept
        : ptr(nullptr), cb(nullptr) {}

    weak_ptr(const weak_ptr& r) noexcept
        : ptr(r.ptr), cb(r.cb) {
        if (cb != nullptr) {
            cb->inc_weak();
        }
    }

    template<class Y>
    weak_ptr(const weak_ptr<Y>& r) noexcept
        : ptr(r.ptr), cb(r.cb) {
        if (cb != nullptr) {
            cb->inc_weak();
        }
    }

    template<class Y>
    weak_ptr(const shared_ptr<Y>& r) noexcept
        : ptr(r.ptr), cb(r.cb) {
        if (cb != nullptr) {
            cb->inc_weak();
        }
    }

    weak_ptr(weak_ptr&& r) noexcept
        : ptr(r.ptr), cb(r.cb) {
        r.ptr = nullptr;
        r.cb = nullptr;
    }

    template<class Y>
    weak_ptr(weak_ptr<Y>&& r) noexcept
        : ptr(r.ptr), cb(r.cb) {
        r.ptr = nullptr;
        r.cb = nullptr;
    }

    weak_ptr& operator=(const weak_ptr& r) noexcept {
        if (this != &r) {
            weak_ptr(r).swap(*this);
        }
        return *this;
    }

    template<class Y>
    weak_ptr& operator=(const weak_ptr<Y>& r) noexcept {
        weak_ptr<Y>(r).swap(*this);
        return *this;
    }

    template<class Y>
    weak_ptr& operator=(const shared_ptr<Y>& r) noexcept {
        weak_ptr<Y>(r).swap(*this);
        return *this;
    }

    weak_ptr& operator=(weak_ptr&& r) noexcept {
        if (this != &r) {
            weak_ptr(std::move(r)).swap(*this);
        }
        return *this;
    }

    template<class Y>
    weak_ptr& operator=(weak_ptr<Y>&& r) noexcept {
        weak_ptr(std::move(r)).swap(*this);
        return *this;
    }

    ~weak_ptr() {
        if (cb != nullptr) {
            ptr = nullptr;
            cb->release_weak();
            if (cb->get_shared_cnt() == 0 && cb->get_weak_cnt() == 0) {
                delete cb;
                cb = nullptr;
            }
        }
    }

    void reset() noexcept {
        weak_ptr().swap(*this);
    }

    void swap(weak_ptr& r) noexcept {
        using std::swap;
        swap(ptr, r.ptr);
        swap(cb, r.cb);
    }

    shared_ptr<T> lock() const noexcept {
        if (cb == nullptr || cb->get_shared_cnt() == 0) {
            return shared_ptr<T>();
        } else {
            cb->inc_shared();
            return shared_ptr<T>(ptr, cb);
        }
    }

 private:
    T* ptr;
    control_block* cb;

    template <typename U>
    friend struct shared_ptr;
};
