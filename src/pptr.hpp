/*
 * Copyright (C) 2019 University of Rochester. All rights reserved.
 * Licenced under the MIT licence. See LICENSE file in the project root for
 * details. 
 */

#ifndef _PPTR_HPP_
#define _PPTR_HPP_

#include <vector>
#include <iostream>
#include <cstddef>
#include <atomic>
#include "pm_config.hpp"

inline bool is_null_pptr(uint64_t off) {
    return off == PPTR_PATTERN_POS;
}

inline bool is_valid_pptr(uint64_t off) {
    return (off & PPTR_PATTERN_MASK) == PPTR_PATTERN_POS;
}
/*
 * class pptr<T>
 * 
 * Description:
 *  Position independent pointer class for type T, which can be applied via
 *  replacing all T* by pptr<T>.
 *  However, for atomic plain pointers, please replace atomic<T*> by
 *  atomic_pptr<T> as a whole.
 * 
 *  The current implementation is off-holder from paper:
 *      Efficient Support of Position Independence on Non-Volatile Memory
 *      Guoyang Chen et al., MICRO'2017
 *
 *  It stores the offset from the instance itself to the object it points to.
 *  The offset can be negative.
 * 
 *  Two kinds of constructors and casting to transient pointer are provided,
 *  as well as dereference, arrow access, assignment, and comparison.
 * 
 *  TODO: implement pptr as RIV which allows cross-region references, while still
 *  keeping the same interface.
 */
template<class T>
class pptr;
/* 
 * class atomic_pptr<T>
 * 
 * Description:
 * This is the atomic version of pptr<T>, whose constructor takes a pointer or
 * pptr<T>.
 *
 * The field *off* stores the offset from the instance of atomic_pptr to 
 * the object it points to.
 *
 * It defines load, store, compare_exchange_weak, and compare_exchange_strong
 * with the same specification of atomic, and returns and/or takes desired and 
 * expected value in type of T*.
 */
template<class T>
class atomic_pptr;

/* 
 * functions to_pptr_off<T> and from_pptr_off<T>
 * 
 * Description:
 * These are functions for conversions between pptr<T>::off and T* 
 */
template<class T>
inline uint64_t to_pptr_off(const T* v, const pptr<T>* p) {
    uint64_t off;
    if(v == nullptr) {
        off = PPTR_PATTERN_POS;
    } else {
        if(v > reinterpret_cast<const T*>(p)) {
            off = ((uint64_t)v) - ((uint64_t)p);
            off = off << PPTR_PATTERN_SHIFT;
            off = off | PPTR_PATTERN_POS;
        } else {
            off = ((uint64_t)p) - ((uint64_t)v);
            off = off << PPTR_PATTERN_SHIFT;
            off = off | PPTR_PATTERN_NEG;
        }
    }
    return off;
}

template<class T>
inline T* from_pptr_off(uint64_t off, const pptr<T>* p) {
    if(!is_valid_pptr(off) || is_null_pptr(off)) { 
        return nullptr;
    } else {
        if(off & 1) { // sign bit is true (negative)
            return (T*)(((int64_t)p) - (off>>PPTR_PATTERN_SHIFT));
        } else {
            return (T*)(((int64_t)p) + (off>>PPTR_PATTERN_SHIFT));
        }
    }
}

template<class T>
inline uint64_t to_pptr_off(const T* v, const atomic_pptr<T>* p) {
    return to_pptr_off(v, reinterpret_cast<const pptr<T>*>(p));
}

template<class T>
inline T* from_pptr_off(uint64_t off, const atomic_pptr<T>* p) {
    return from_pptr_off(off, reinterpret_cast<const pptr<T>*>(p));
}

template<class T>
class pptr{
public:
    uint64_t off;
    pptr(T* v=nullptr) noexcept { //default constructor
        off = to_pptr_off(v, this);
    };
    pptr(const pptr<T> &p) noexcept { //copy constructor
        T* v = static_cast<T*>(p);
        off = to_pptr_off(v, this);
    }

    template<class F>
    inline operator F*() const{ //cast to transient pointer
        return from_pptr_off(off, this);
    }
    inline T& operator * () { //dereference
        return *static_cast<T*>(*this);
    }
    inline T* operator -> (){ //arrow
        return static_cast<T*>(*this);
    }
    template<class F>
    inline pptr& operator = (const F* v){ //assignment
        off = to_pptr_off(v, this);
        return *this;
    }
    inline pptr& operator = (const pptr &p){ //assignment
        T* v = static_cast<T*>(p);
        off = to_pptr_off(v, this);
        return *this;
    }
    inline T& operator [] (size_t idx) const { // subscript
            return static_cast<T*>(*this)[idx];
    }
    bool is_null() const {
        return off == PPTR_PATTERN_POS;
    }

    bool is_valid() const {
        return (off & PPTR_PATTERN_MASK) == PPTR_PATTERN_POS;
    }
};

template <class T>
inline bool operator==(const pptr<T>& lhs, const std::nullptr_t& rhs){
    return lhs.is_null();
}

template <class T>
inline bool operator==(const pptr<T>& lhs, const pptr<T>& rhs){
    return (T*)lhs == (T*)rhs;
}

template <class T>
inline bool operator!=(const pptr<T>& lhs, const std::nullptr_t& rhs){
    return !lhs.is_null();
}

template <class T>
inline bool operator!=(const pptr<T>& lhs, const pptr<T>& rhs){
    return !((T*)lhs == (T*)rhs);
}

template <class T> 
class atomic_pptr{
public:
    std::atomic<uint64_t> off;
    atomic_pptr(T* v=nullptr) noexcept { //default constructor
        uint64_t tmp_off = to_pptr_off(v, this);
        off.store(tmp_off);
    }
    atomic_pptr(const pptr<T> &p) noexcept { //copy constructor
        T* v = static_cast<T*>(p);
        uint64_t tmp_off = to_pptr_off(v, this);
        off.store(tmp_off);
    }
    inline atomic_pptr& operator = (const atomic_pptr &p){ //assignment
        T* v = p.load();
        uint64_t tmp_off = to_pptr_off(v, this);
        off.store(tmp_off);
        return *this;
    }
    template<class F>
    inline atomic_pptr& operator = (const F* v){ //assignment
        uint64_t tmp_off = to_pptr_off(v, this);
        off.store(tmp_off);
        return *this;
    }
    T* load(std::memory_order order = std::memory_order_seq_cst) const noexcept{
        uint64_t cur_off = off.load(order);
        return from_pptr_off(cur_off, this);
    }
    void store(T* desired, 
        std::memory_order order = std::memory_order_seq_cst ) noexcept{
        uint64_t new_off = to_pptr_off(desired, this);
        off.store(new_off, order);
    }
    bool compare_exchange_weak(T*& expected, T* desired,
        std::memory_order order = std::memory_order_seq_cst ) noexcept{
        uint64_t old_off = to_pptr_off(expected, this);
        uint64_t new_off = to_pptr_off(desired, this);
        bool ret = off.compare_exchange_weak(old_off, new_off, order);
        if(!ret) {
            if(is_null_pptr(old_off)){
                expected = nullptr;
            } else{
                expected = from_pptr_off(old_off, this);
            }
        }
        return ret;
    }
    bool compare_exchange_strong(T*& expected, T* desired,
        std::memory_order order = std::memory_order_seq_cst ) noexcept{
        uint64_t old_off = to_pptr_off(expected, this);
        uint64_t new_off = to_pptr_off(desired, this);
        bool ret = off.compare_exchange_strong(old_off, new_off, order);
        if(!ret) {
            if(is_null_pptr(old_off)){
                expected = nullptr;
            } else{
                expected = from_pptr_off(old_off, this);
            }
        }
        return ret;
    }
};
#endif
