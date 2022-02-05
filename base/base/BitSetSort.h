/** https://github.com/minjaehwang/bitsetsort
  *
  * Licensed under the Apache License, Version 2.0 (the "License");
  * you may not use this file except in compliance with the License.
  * You may obtain a copy of the License at
  *
  * http://www.apache.org/licenses/LICENSE-2.0
  *
  * Unless required by applicable law or agreed to in writing, software
  * distributed under the License is distributed on an "AS IS" BASIS,
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  * Bitset Sort is a variant of quick sort, specifically BlockQuickSort.
  * Bitset Sort uses a carefully written partition function to let the compiler generates
  * SIMD instructions without actually writing SIMD intrinsics in the loop.
  * Bitset Sort is 3.4x faster (or spends 71% less time) than libc++ std::sort when sorting uint64s and 1.58x faster (or spends 37% less time)
  * when sorting std::string.
  * Bitset Sort uses multiple techniques to improve runtime performance of sort. This includes sorting networks,
  * a variant of merge sort called Bitonic Order Merge Sort that is faster for small N, and pattern recognitions.
  */

#pragma clang diagnostic ignored "-Wreserved-identifier"
#pragma clang diagnostic ignored "-Wreserved-macro-identifier"
#pragma clang diagnostic ignored "-Wunused-local-typedef"

#ifndef _LIBCPP___BITSETSORT
#define _LIBCPP___BITSETSORT

#include <algorithm>
#include <cstdint>
#include <iterator>

namespace stdext {  //_LIBCPP_BEGIN_NAMESPACE_STD

/// Implementation from LLVM Path https://reviews.llvm.org/D93233

namespace __sorting_network {

template <class _RandomAccessIterator, class _Compare>
class __conditional_swap {
public:
  typedef typename _VSTD::__comp_ref_type<_Compare>::type _Comp_ref;

  _LIBCPP_CONSTEXPR_AFTER_CXX11 _Comp_ref get() const { return comp_; }
  _LIBCPP_CONSTEXPR_AFTER_CXX11 __conditional_swap(const _Comp_ref __comp) : comp_(__comp) {}
  _LIBCPP_CONSTEXPR_AFTER_CXX11 inline void operator()(_RandomAccessIterator __x, _RandomAccessIterator __y) {
    typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type value_type;
    bool __result = comp_(*__y, *__x);
    // Expect a compiler would short-circuit the following if-block.
    // 4 * sizeof(size_t) is a magic number. Expect a compiler to use SIMD
    // instruction on them.
    if (_VSTD::is_trivially_copy_constructible<value_type>::value &&
        _VSTD::is_trivially_copy_assignable<value_type>::value && sizeof(value_type) <= 4 * sizeof(size_t)) {
      value_type __min = __result ? _VSTD::move(*__y) : _VSTD::move(*__x);
      *__y = __result ? _VSTD::move(*__x) : _VSTD::move(*__y);
      *__x = _VSTD::move(__min);
    } else {
      if (__result) {
        _VSTD::iter_swap(__x, __y);
      }
    }
  }

private:
  _Comp_ref comp_;
};

template <class _RandomAccessIterator, class _Compare>
class __reverse_conditional_swap {
  typedef typename _VSTD::__comp_ref_type<_Compare>::type _Comp_ref;
  _Comp_ref comp_;

public:
  _LIBCPP_CONSTEXPR_AFTER_CXX11 _Comp_ref get() const { return comp_; }
  _LIBCPP_CONSTEXPR_AFTER_CXX11
  __reverse_conditional_swap(const _Comp_ref __comp) : comp_(__comp) {}
  inline void operator()(_RandomAccessIterator __x, _RandomAccessIterator __y) {
    typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type value_type;
    bool __result = !comp_(*__x, *__y);
    // Expect a compiler would short-circuit the following if-block.
    if (_VSTD::is_trivially_copy_constructible<value_type>::value &&
        _VSTD::is_trivially_copy_assignable<value_type>::value && sizeof(value_type) <= 4 * sizeof(size_t)) {
      value_type __min = __result ? _VSTD::move(*__x) : _VSTD::move(*__y);
      *__y = __result ? _VSTD::move(*__y) : _VSTD::move(*__x);
      *__x = _VSTD::move(__min);
    } else {
      if (!__result) {
        _VSTD::iter_swap(__x, __y);
      }
    }
  }
};

template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __sort2(_RandomAccessIterator __a, _ConditionalSwap __cond_swap) {
  __cond_swap(__a + 0, __a + 1);
}

template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __sort3(_RandomAccessIterator __a, _ConditionalSwap __cond_swap) {
  __cond_swap(__a + 1, __a + 2);
  __cond_swap(__a + 0, __a + 1);
  __cond_swap(__a + 1, __a + 2);
}

template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __sort4(_RandomAccessIterator __a, _ConditionalSwap __cond_swap) {
  __cond_swap(__a + 0, __a + 1);
  __cond_swap(__a + 2, __a + 3);
  __cond_swap(__a + 0, __a + 2);
  __cond_swap(__a + 1, __a + 3);
  __cond_swap(__a + 1, __a + 2);
}

template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __sort5(_RandomAccessIterator __a, _ConditionalSwap __cond_swap) {
  __cond_swap(__a + 0, __a + 1);
  __cond_swap(__a + 3, __a + 4);
  __cond_swap(__a + 2, __a + 3);
  __cond_swap(__a + 3, __a + 4);
  __cond_swap(__a + 0, __a + 3);
  __cond_swap(__a + 1, __a + 4);
  __cond_swap(__a + 0, __a + 2);
  __cond_swap(__a + 1, __a + 3);
  __cond_swap(__a + 1, __a + 2);
}

template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __sort6(_RandomAccessIterator __a, _ConditionalSwap __cond_swap) {
  __cond_swap(__a + 1, __a + 2);
  __cond_swap(__a + 4, __a + 5);
  __cond_swap(__a + 0, __a + 1);
  __cond_swap(__a + 3, __a + 4);
  __cond_swap(__a + 1, __a + 2);
  __cond_swap(__a + 4, __a + 5);
  __cond_swap(__a + 0, __a + 3);
  __cond_swap(__a + 1, __a + 4);
  __cond_swap(__a + 2, __a + 5);
  __cond_swap(__a + 2, __a + 4);
  __cond_swap(__a + 1, __a + 3);
  __cond_swap(__a + 2, __a + 3);
}
template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __sort7(_RandomAccessIterator __a, _ConditionalSwap __cond_swap) {
  __cond_swap(__a + 1, __a + 2);
  __cond_swap(__a + 3, __a + 4);
  __cond_swap(__a + 5, __a + 6);
  __cond_swap(__a + 0, __a + 1);
  __cond_swap(__a + 3, __a + 5);
  __cond_swap(__a + 4, __a + 6);
  __cond_swap(__a + 1, __a + 2);
  __cond_swap(__a + 4, __a + 5);
  __cond_swap(__a + 0, __a + 4);
  __cond_swap(__a + 1, __a + 5);
  __cond_swap(__a + 2, __a + 6);
  __cond_swap(__a + 0, __a + 3);
  __cond_swap(__a + 2, __a + 5);
  __cond_swap(__a + 1, __a + 3);
  __cond_swap(__a + 2, __a + 4);
  __cond_swap(__a + 2, __a + 3);
}

template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __sort8(_RandomAccessIterator __a, _ConditionalSwap __cond_swap) {
  __cond_swap(__a + 0, __a + 1);
  __cond_swap(__a + 2, __a + 3);
  __cond_swap(__a + 4, __a + 5);
  __cond_swap(__a + 6, __a + 7);
  __cond_swap(__a + 0, __a + 2);
  __cond_swap(__a + 1, __a + 3);
  __cond_swap(__a + 4, __a + 6);
  __cond_swap(__a + 5, __a + 7);
  __cond_swap(__a + 1, __a + 2);
  __cond_swap(__a + 5, __a + 6);
  __cond_swap(__a + 0, __a + 4);
  __cond_swap(__a + 1, __a + 5);
  __cond_swap(__a + 2, __a + 6);
  __cond_swap(__a + 3, __a + 7);
  __cond_swap(__a + 1, __a + 4);
  __cond_swap(__a + 3, __a + 6);
  __cond_swap(__a + 2, __a + 4);
  __cond_swap(__a + 3, __a + 5);
  __cond_swap(__a + 3, __a + 4);
}

template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __sort1to8(_RandomAccessIterator __a,
                                      typename _VSTD::iterator_traits<_RandomAccessIterator>::difference_type __len,
                                      _ConditionalSwap __cond_swap) {
  switch (__len) {
  case 0:
  case 1:
    return;
  case 2:
    __sort2(__a, __cond_swap);
    return;
  case 3:
    __sort3(__a, __cond_swap);
    return;
  case 4:
    __sort4(__a, __cond_swap);
    return;
  case 5:
    __sort5(__a, __cond_swap);
    return;
  case 6:
    __sort6(__a, __cond_swap);
    return;
  case 7:
    __sort7(__a, __cond_swap);
    return;
  case 8:
    __sort8(__a, __cond_swap);
    return;
  }
  // ignore
}
template <class _RandomAccessIterator, class _ConditionalSwap>
_LIBCPP_CONSTEXPR_AFTER_CXX11 _LIBCPP_HIDE_FROM_ABI void __sort3(_RandomAccessIterator __a0, _RandomAccessIterator __a1,
                                                                 _RandomAccessIterator __a2,
                                                                 _ConditionalSwap __cond_swap) {
  __cond_swap(__a1, __a2);
  __cond_swap(__a0, __a2);
  __cond_swap(__a0, __a1);
}

// stable, 2-3 compares, 0-2 swaps

template <class _Compare, class _ForwardIterator>
_LIBCPP_CONSTEXPR_AFTER_CXX11 _LIBCPP_HIDE_FROM_ABI unsigned
__sort3_with_number_of_swaps(_ForwardIterator __x, _ForwardIterator __y, _ForwardIterator __z, _Compare __c) {
  unsigned __r = 0;
  if (!__c(*__y, *__x)) // if x <= y
  {
    if (!__c(*__z, *__y)) // if y <= z
      return __r;         // x <= y && y <= z
                          // x <= y && y > z
    swap(*__y, *__z);     // x <= z && y < z
    __r = 1;
    if (__c(*__y, *__x)) // if x > y
    {
      swap(*__x, *__y); // x < y && y <= z
      __r = 2;
    }
    return __r; // x <= y && y < z
  }
  if (__c(*__z, *__y)) // x > y, if y > z
  {
    swap(*__x, *__z); // x < y && y < z
    __r = 1;
    return __r;
  }
  swap(*__x, *__y);    // x > y && y <= z
  __r = 1;             // x < y && x <= z
  if (__c(*__z, *__y)) // if y > z
  {
    swap(*__y, *__z); // x <= y && y < z
    __r = 2;
  }
  return __r;
}

} // namespace __sorting_network

namespace __bitonic {
class __detail {
public:
  enum {
    __batch = 8,
    __bitonic_batch = __batch * 2,
    __small_sort_max = __bitonic_batch * 2,
  };
};

template <class _RandomAccessIterator, class _ConditionalSwap, class _ReverseConditionalSwap>
_LIBCPP_HIDE_FROM_ABI void __enforce_order(_RandomAccessIterator __first, _RandomAccessIterator __last,
                                           _ConditionalSwap __cond_swap, _ReverseConditionalSwap __reverse_cond_swap) {
  _RandomAccessIterator __i = __first;
  while (__detail::__bitonic_batch <= __last - __i) {
    __sorting_network::__sort8(__i, __cond_swap);
    __sorting_network::__sort8(__i + __detail::__batch, __reverse_cond_swap);
    __i += __detail::__bitonic_batch;
  }
  if (__detail::__batch <= __last - __i) {
    __sorting_network::__sort8(__i, __cond_swap);
    __i += __detail::__batch;
    __sorting_network::__sort1to8(__i, __last - __i, __reverse_cond_swap);
  } else {
    __sorting_network::__sort1to8(__i, __last - __i, __cond_swap);
  }
}

class __construct {
public:
  template <class _Type1, class _Type2>
  static inline void __op(_Type1* __result, _Type2&& __val) {
    new (static_cast<void*>(__result)) _Type1(_VSTD::move(__val));
  }
};

class __move_assign {
public:
  template <class _Type1, class _Type2>
  static inline void __op(_Type1 __result, _Type2&& __val) {
    *__result = _VSTD::move(__val);
  }
};

template <class _Copy, class _Compare, class _InputIterator, class _OutputIterator>
_LIBCPP_HIDE_FROM_ABI void __forward_merge(_InputIterator __first, _InputIterator __last, _OutputIterator __result,
                                           _Compare __comp) {
  --__last;
  // The len used here is one less than the actual length.  This is so that the
  // comparison is carried out against 0.  The final move is done
  // unconditionally at the end.
  typename _VSTD::iterator_traits<_InputIterator>::difference_type __len = __last - __first;
  for (; __len > 0; __len--) {
    if (__comp(*__last, *__first)) {
      _Copy::__op(__result, _VSTD::move(*__last));
      --__last;
    } else {
      _Copy::__op(__result, _VSTD::move(*__first));
      ++__first;
    }
    ++__result;
  }
  _Copy::__op(__result, _VSTD::move(*__first));
}

template <class _Copy, class _Compare, class _InputIterator, class _OutputIterator>
_LIBCPP_HIDE_FROM_ABI void __backward_merge(_InputIterator __first, _InputIterator __last, _OutputIterator __result,
                                            _Compare __comp) {
  --__last;
  __result += __last - __first;
  // The len used here is one less than the actual length.  This is so that the
  // comparison is carried out against 0.  The final move is done
  // unconditionally at the end.
  typename _VSTD::iterator_traits<_InputIterator>::difference_type __len = __last - __first;
  for (; __len > 0; __len--) {
    if (__comp(*__first, *__last)) {
      _Copy::__op(__result, _VSTD::move(*__first));
      ++__first;
    } else {
      _Copy::__op(__result, _VSTD::move(*__last));
      --__last;
    }
    --__result;
  }
  _Copy::__op(__result, _VSTD::move(*__first));
}

template <class _RandomAccessIterator, class _ConditionalSwap, class _ReverseConditionalSwap>
inline _LIBCPP_HIDE_FROM_ABI bool
__small_sort(_RandomAccessIterator __first,
             typename _VSTD::iterator_traits<_RandomAccessIterator>::difference_type __len,
             typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type* __buff, _ConditionalSwap __cond_swap,
             _ReverseConditionalSwap __reverse_cond_swap) {
  typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type value_type;
  typedef typename _ConditionalSwap::_Comp_ref _Comp_ref;
  if (__len > __detail::__small_sort_max) {
    return false;
  }
  _RandomAccessIterator __last = __first + __len;
  __enforce_order(__first, __last, __cond_swap, __reverse_cond_swap);
  if (__len <= __detail::__batch) {
    // sorted.
    return true;
  }
  const _Comp_ref __comp = __cond_swap.get();
  if (__len <= __detail::__bitonic_batch) {
    // single bitonic order merge.
    __forward_merge<__construct, _Comp_ref>(__first, __last, __buff, _Comp_ref(__comp));
    _VSTD::copy(_VSTD::make_move_iterator(__buff), _VSTD::make_move_iterator(__buff + __len), __first);
    for (auto __iter = __buff; __iter < __buff + __len; __iter++) {
      (*__iter).~value_type();
    }
    return true;
  }
  // double bitonic order merge.
  __forward_merge<__construct, _Comp_ref>(__first, __first + __detail::__bitonic_batch, __buff, _Comp_ref(__comp));
  __backward_merge<__construct, _Comp_ref>(__first + __detail::__bitonic_batch, __last,
                                           __buff + __detail::__bitonic_batch, _Comp_ref(__comp));
  __forward_merge<__move_assign, _Comp_ref>(__buff, __buff + __len, __first, _Comp_ref(__comp));
  for (auto __iter = __buff; __iter < __buff + __len; __iter++) {
    (*__iter).~value_type();
  }
  return true;
}
} // namespace __bitonic

namespace __bitsetsort {
struct __64bit_set {
  typedef uint64_t __storage_t;
  enum { __block_size = 64 };
  static __storage_t __blsr(__storage_t x) {
    // _blsr_u64 can be used here but it did not make any performance
    // difference in practice.
    return x ^ (x & -x);
  }
  static int __clz(__storage_t x) { return __builtin_clzll(x); }
  static int __ctz(__storage_t x) { return __builtin_ctzll(x); }
};

struct __32bit_set {
  typedef uint32_t __storage_t;
  enum { __block_size = 32 };
  static __storage_t __blsr(__storage_t x) {
    // _blsr_u32 can be used here but it did not make any performance
    // difference in practice.
    return x ^ (x & -x);
  }
  static int __clz(__storage_t x) { return __builtin_clzl(x); }
  static int __ctz(__storage_t x) { return __builtin_ctzl(x); }
};

template <int _Width>
struct __set_selector {
  typedef __64bit_set __set;
};

template <>
struct __set_selector<4> {
  typedef __32bit_set __set;
};

template <class _Bitset, class _RandomAccessIterator>
inline _LIBCPP_HIDE_FROM_ABI void __swap_bitmap_pos(_RandomAccessIterator __first, _RandomAccessIterator __last,
                                                    typename _Bitset::__storage_t& __left_bitset,
                                                    typename _Bitset::__storage_t& __right_bitset) {
  while (__left_bitset != 0 & __right_bitset != 0) {
    int tz_left = _Bitset::__ctz(__left_bitset);
    __left_bitset = _Bitset::__blsr(__left_bitset);
    int tz_right = _Bitset::__ctz(__right_bitset);
    __right_bitset = _Bitset::__blsr(__right_bitset);
    _VSTD::iter_swap(__first + tz_left, __last - tz_right);
  }
}

template <class _Bitset, class _RandomAccessIterator, class _Compare>
_LIBCPP_HIDE_FROM_ABI _VSTD::pair<_RandomAccessIterator, bool>
__bitset_partition(_RandomAccessIterator __first, _RandomAccessIterator __last, _Compare __comp) {
  typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type value_type;
  typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::difference_type difference_type;
  typedef typename _Bitset::__storage_t __storage_t;
  _RandomAccessIterator __begin = __first;
  value_type __pivot(_VSTD::move(*__first));

  // Check if pivot is less than the last element.  Checking this first avoids
  // comparing the first and the last iterators on each iteration as done in the
  // else part.
  if (__comp(__pivot, *(__last - 1))) {
    // Guarded.
    while (!__comp(__pivot, *++__first)) {
    }
  } else {
    while (++__first < __last && !__comp(__pivot, *__first)) {
    }
  }

  if (__first < __last) {
    // It will be always guarded because __bitset_sort will do the
    // median-of-three before calling this.
    while (__comp(__pivot, *--__last)) {
    }
  }
  bool __already_partitioned = __first >= __last;
  if (!__already_partitioned) {
    _VSTD::iter_swap(__first, __last);
    ++__first;
  }

  // In [__first, __last) __last is not inclusive. From now one, it uses last
  // minus one to be inclusive on both sides.
  _RandomAccessIterator __lm1 = __last - 1;
  __storage_t __left_bitset = 0;
  __storage_t __right_bitset = 0;

  // Reminder: length = __lm1 - __first + 1.
  while (__lm1 - __first >= 2 * _Bitset::__block_size - 1) {
    if (__left_bitset == 0) {
      // Possible vectorization. With a proper "-march" flag, the following loop
      // will be compiled into a set of SIMD instructions.
      _RandomAccessIterator __iter = __first;
      for (int __j = 0; __j < _Bitset::__block_size;) {
        bool __comp_result = __comp(__pivot, *__iter);
        __left_bitset |= (static_cast<__storage_t>(__comp_result) << __j);
        __j++;
        ++__iter;
      }
    }
    if (__right_bitset == 0) {
      // Possible vectorization. With a proper "-march" flag, the following loop
      // will be compiled into a set of SIMD instructions.
      _RandomAccessIterator __iter = __lm1;
      for (int __j = 0; __j < _Bitset::__block_size;) {
        bool __comp_result = __comp(*__iter, __pivot);
        __right_bitset |= (static_cast<__storage_t>(__comp_result) << __j);
        __j++;
        --__iter;
      }
    }
    __swap_bitmap_pos<_Bitset>(__first, __lm1, __left_bitset, __right_bitset);
    __first += (__left_bitset == 0) ? _Bitset::__block_size : 0;
    __lm1 -= (__right_bitset == 0) ? _Bitset::__block_size : 0;
  }
  // Now, we have a less-than a block on each side.
  difference_type __remaining_len = __lm1 - __first + 1;
  difference_type __l_size;
  difference_type __r_size;
  if (__left_bitset == 0 && __right_bitset == 0) {
    __l_size = __remaining_len / 2;
    __r_size = __remaining_len - __l_size;
  } else if (__left_bitset == 0) {
    // We know at least one side is a full block.
    __l_size = __remaining_len - _Bitset::__block_size;
    __r_size = _Bitset::__block_size;
  } else { // if (__right_bitset == 0)
    __l_size = _Bitset::__block_size;
    __r_size = __remaining_len - _Bitset::__block_size;
  }
  if (__left_bitset == 0) {
    _RandomAccessIterator __iter = __first;
    for (int j = 0; j < __l_size; j++) {
      bool __comp_result = __comp(__pivot, *__iter);
      __left_bitset |= (static_cast<__storage_t>(__comp_result) << j);
      ++__iter;
    }
  }
  if (__right_bitset == 0) {
    _RandomAccessIterator __iter = __lm1;
    for (int j = 0; j < __r_size; j++) {
      bool __comp_result = __comp(*__iter, __pivot);
      __right_bitset |= (static_cast<__storage_t>(__comp_result) << j);
      --__iter;
    }
  }
  __swap_bitmap_pos<_Bitset>(__first, __lm1, __left_bitset, __right_bitset);
  __first += (__left_bitset == 0) ? __l_size : 0;
  __lm1 -= (__right_bitset == 0) ? __r_size : 0;

  if (__left_bitset) {
    // Swap within the left side.
    // Need to find set positions in the reverse order.
    while (__left_bitset != 0) {
      int __tz_left = _Bitset::__block_size - 1 - _Bitset::__clz(__left_bitset);
      __left_bitset &= (static_cast<__storage_t>(1) << __tz_left) - 1;
      _RandomAccessIterator it = __first + __tz_left;
      if (it != __lm1) {
        _VSTD::iter_swap(it, __lm1);
      }
      --__lm1;
    }
    __first = __lm1 + 1;
  } else if (__right_bitset) {
    // Swap within the right side.
    // Need to find set positions in the reverse order.
    while (__right_bitset != 0) {
      int __tz_right = _Bitset::__block_size - 1 - _Bitset::__clz(__right_bitset);
      __right_bitset &= (static_cast<__storage_t>(1) << __tz_right) - 1;
      _RandomAccessIterator it = __lm1 - __tz_right;
      if (it != __first) {
        _VSTD::iter_swap(it, __first);
      }
      ++__first;
    }
  }

  _RandomAccessIterator __pivot_pos = __first - 1;
  if (__begin != __pivot_pos) {
    *__begin = _VSTD::move(*__pivot_pos);
  }
  *__pivot_pos = _VSTD::move(__pivot);
  return _VSTD::make_pair(__pivot_pos, __already_partitioned);
}

template <class _Compare, class _RandomAccessIterator>
inline _LIBCPP_HIDE_FROM_ABI bool __partial_insertion_sort(_RandomAccessIterator __first, _RandomAccessIterator __last,
                                                           _Compare __comp) {
  typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type value_type;
  if (__first == __last)
    return true;

  const unsigned __limit = 8;
  unsigned __count = 0;
  _RandomAccessIterator __j = __first;
  for (_RandomAccessIterator __i = __j + 1; __i != __last; ++__i) {
    if (__comp(*__i, *__j)) {
      value_type __t(_VSTD::move(*__i));
      _RandomAccessIterator __k = __j;
      __j = __i;
      do {
        *__j = _VSTD::move(*__k);
        __j = __k;
      } while (__j != __first && __comp(__t, *--__k));
      *__j = _VSTD::move(__t);
      if (++__count == __limit)
        return ++__i == __last;
    }
    __j = __i;
  }
  return true;
}

template <class _Compare, class _RandomAccessIterator>
void __bitsetsort_loop(_RandomAccessIterator __first, _RandomAccessIterator __last, _Compare __comp,
                       typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type* __buff,
                       typename _VSTD::iterator_traits<_RandomAccessIterator>::difference_type __limit) {
  _LIBCPP_CONSTEXPR_AFTER_CXX11 int __ninther_threshold = 128;
  typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::difference_type difference_type;
  typedef typename _VSTD::__comp_ref_type<_Compare>::type _Comp_ref;
  __sorting_network::__conditional_swap<_RandomAccessIterator, _Compare> __cond_swap(__comp);
  __sorting_network::__reverse_conditional_swap<_RandomAccessIterator, _Compare> __reverse_cond_swap(__comp);
  while (true) {
    if (__limit == 0) {
      // Fallback to heap sort as Introsort suggests.
      _VSTD::make_heap<_RandomAccessIterator, _Comp_ref>(__first, __last, _Comp_ref(__comp));
      _VSTD::sort_heap<_RandomAccessIterator, _Comp_ref>(__first, __last, _Comp_ref(__comp));
      return;
    }
    __limit--;
    difference_type __len = __last - __first;
    if (__len <= __bitonic::__detail::__batch) {
      __sorting_network::__sort1to8(__first, __len, __cond_swap);
      return;
    } else if (__len <= __bitonic::__detail::__small_sort_max) {
      __bitonic::__small_sort(__first, __len, __buff, __cond_swap, __reverse_cond_swap);
      return;
    }
    difference_type __half_len = __len / 2;
    if (__len > __ninther_threshold) {
      __sorting_network::__sort3(__first, __first + __half_len, __last - 1, __cond_swap);
      __sorting_network::__sort3(__first + 1, __first + (__half_len - 1), __last - 2, __cond_swap);
      __sorting_network::__sort3(__first + 2, __first + (__half_len + 1), __last - 3, __cond_swap);
      __sorting_network::__sort3(__first + (__half_len - 1), __first + __half_len, __first + (__half_len + 1),
                                 __cond_swap);
      _VSTD::iter_swap(__first, __first + __half_len);
    } else {
      __sorting_network::__sort3(__first + __half_len, __first, __last - 1, __cond_swap);
    }
    auto __ret = __bitset_partition<__64bit_set, _RandomAccessIterator, _Comp_ref>(__first, __last, _Comp_ref(__comp));
    if (__ret.second) {
      bool __left = __partial_insertion_sort<_Comp_ref>(__first, __ret.first, _Comp_ref(__comp));
      bool __right = __partial_insertion_sort<_Comp_ref>(__ret.first + 1, __last, _Comp_ref(__comp));
      if (__right) {
        if (__left)
          return;
        __last = __ret.first;
        continue;
      } else {
        if (__left) {
          __first = ++__ret.first;
          continue;
        }
      }
    }

    // Sort smaller range with recursive call and larger with tail recursion
    // elimination.
    if (__ret.first - __first < __last - __ret.first) {
      __bitsetsort_loop<_Compare>(__first, __ret.first, __comp, __buff, __limit);
      __first = ++__ret.first;
    } else {
      __bitsetsort_loop<_Compare>(__ret.first + 1, __last, __comp, __buff, __limit);
      __last = __ret.first;
    }
  }
}

template <typename _Number>
inline _LIBCPP_HIDE_FROM_ABI _Number __log2i(_Number __n) {
  _Number __log2 = 0;
  while (__n > 1) {
    __log2++;
    __n >>= 1;
  }
  return __log2;
}

template <class _Compare, class _RandomAccessIterator>
inline _LIBCPP_HIDE_FROM_ABI void __bitsetsort_internal(_RandomAccessIterator __first, _RandomAccessIterator __last,
                                                        _Compare __comp) {
  typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type value_type;
  typedef typename _VSTD::iterator_traits<_RandomAccessIterator>::difference_type difference_type;
  typename _VSTD::aligned_storage<sizeof(value_type)>::type __buff[__bitonic::__detail::__small_sort_max];
  typedef typename _VSTD::__comp_ref_type<_Compare>::type _Comp_ref;

  // 2*log2 comes from Introsort https://reviews.llvm.org/D36423.
  difference_type __depth_limit = 2 * __log2i(__last - __first);
  __bitsetsort_loop<_Comp_ref>(__first, __last, _Comp_ref(__comp), reinterpret_cast<value_type*>(&__buff[0]),
                               __depth_limit);
}
} // namespace __bitsetsort

template <class _RandomAccessIterator, class _Compare>
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_AFTER_CXX17 void bitsetsort(_RandomAccessIterator __first,
                                                                     _RandomAccessIterator __last, _Compare __comp) {
  typedef typename _VSTD::__comp_ref_type<_Compare>::type _Comp_ref;
  if (_VSTD::__libcpp_is_constant_evaluated()) {
    _VSTD::__partial_sort<_Comp_ref>(__first, __last, __last, _Comp_ref(__comp));
  } else {
    __bitsetsort::__bitsetsort_internal<_Comp_ref>(_VSTD::__unwrap_iter(__first), _VSTD::__unwrap_iter(__last),
                                                   _Comp_ref(__comp));
  }
}

template <class _RandomAccessIterator>
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_AFTER_CXX17 void bitsetsort(_RandomAccessIterator __first,
                                                                     _RandomAccessIterator __last) {
  bitsetsort(__first, __last, __less<typename _VSTD::iterator_traits<_RandomAccessIterator>::value_type>());
}

}  // namespace stdext

#endif  // _LIBCPP___BITSETSORT
