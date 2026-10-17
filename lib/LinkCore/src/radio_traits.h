// radio_traits.h - compile-time contract between the bridge and a radio driver.
// If RadioLink (or a test double) drifts from the expected signatures, the build fails with a readable message
// instead of a wall of template errors.
#pragma once
#include <stdint.h>
#include <type_traits>
#include <utility>

namespace linkcore {
namespace traits {

template <class R>
using ExchangeT = decltype(std::declval<R&>().exchange(std::declval<const uint8_t*>(), std::declval<uint8_t>(),
                                                       std::declval<uint8_t*>(), std::declval<uint8_t&>(),
                                                       std::declval<uint8_t&>()));
template <class R>
using ReceiveT = decltype(std::declval<R&>().receive(std::declval<uint8_t*>(), std::declval<uint8_t&>()));
template <class R>
using LoadAckT = decltype(std::declval<R&>().loadAck(std::declval<const uint8_t*>(), std::declval<uint8_t>()));
template <class R>
using FlushTxT = decltype(std::declval<R&>().flushTx());

template <class R, class = void>
struct IsPtxRadio : std::false_type {};
template <class R>
struct IsPtxRadio<R, std::void_t<ExchangeT<R>>> : std::is_same<ExchangeT<R>, bool> {};

template <class R, class = void>
struct IsPrxRadio : std::false_type {};
template <class R>
struct IsPrxRadio<R, std::void_t<ReceiveT<R>, LoadAckT<R>, FlushTxT<R>>>
    : std::integral_constant<bool, std::is_same<ReceiveT<R>, bool>::value &&
                                       std::is_same<LoadAckT<R>, bool>::value &&
                                       std::is_void<FlushTxT<R>>::value> {};

}  // namespace traits
}  // namespace linkcore
