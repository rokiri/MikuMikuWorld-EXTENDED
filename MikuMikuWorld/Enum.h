#pragma once
#include <type_traits>
#include <iterator>

// Macro to allow usage of flags operators with types enums
#define DECLARE_ENUM_FLAG_OPERATORS(EnumType)                                                      \
	inline constexpr EnumType operator|(EnumType lhs, EnumType rhs)                                \
	{                                                                                              \
		return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) |          \
		                             static_cast<std::underlying_type_t<EnumType>>(rhs));          \
	}                                                                                              \
	inline constexpr EnumType operator&(EnumType lhs, EnumType rhs)                                \
	{                                                                                              \
		return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) &          \
		                             static_cast<std::underlying_type_t<EnumType>>(rhs));          \
	}                                                                                              \
	inline constexpr EnumType operator^(EnumType lhs, EnumType rhs)                                \
	{                                                                                              \
		return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) ^          \
		                             static_cast<std::underlying_type_t<EnumType>>(rhs));          \
	}                                                                                              \
	inline constexpr EnumType operator~(EnumType value)                                            \
	{                                                                                              \
		return static_cast<EnumType>(~static_cast<std::underlying_type_t<EnumType>>(value));       \
	}                                                                                              \
	inline constexpr EnumType& operator|=(EnumType& lhs, EnumType rhs)                             \
	{                                                                                              \
		lhs = lhs | rhs;                                                                           \
		return lhs;                                                                                \
	}                                                                                              \
	inline constexpr EnumType& operator&=(EnumType& lhs, EnumType rhs)                             \
	{                                                                                              \
		lhs = lhs & rhs;                                                                           \
		return lhs;                                                                                \
	}                                                                                              \
	inline constexpr EnumType& operator^=(EnumType& lhs, EnumType rhs)                             \
	{                                                                                              \
		lhs = lhs ^ rhs;                                                                           \
		return lhs;                                                                                \
	}

template <typename TEnum, typename TUnder = std::underlying_type_t<TEnum>>
inline constexpr auto hasFlag(TEnum value, TEnum flag) noexcept
    -> std::enable_if_t<std::is_enum_v<TEnum> && std::is_integral_v<TUnder>, bool>
{
	TUnder v = static_cast<TUnder>(value);
	TUnder f = static_cast<TUnder>(flag);
	return (v & f) == f;
}

template <typename TEnum, typename TUnder = std::underlying_type_t<TEnum>>
inline constexpr auto setFlag(TEnum value, TEnum flag, bool enable = true) noexcept
    -> std::enable_if_t<std::is_enum_v<TEnum> && std::is_integral_v<TUnder>, TEnum>
{
	TUnder v = static_cast<TUnder>(value);
	TUnder f = static_cast<TUnder>(flag);
	return static_cast<TEnum>(enable ? (v | f) : (v & ~f));
}

template <typename TEnum, typename TUnder = std::underlying_type_t<TEnum>>
inline constexpr auto setFlag(TEnum value, TEnum flag, TEnum control) noexcept
    -> std::enable_if_t<std::is_enum_v<TEnum> && std::is_integral_v<TUnder>, TEnum>
{
	TUnder v = static_cast<TUnder>(value);
	TUnder f = static_cast<TUnder>(flag);
	TUnder c = static_cast<TUnder>(control);

	// set or unset base on control having the flag or not
	return static_cast<TEnum>((c & f) == f ? (v | f) : (v & ~f));
}

template <typename TEnum, typename TUnder = std::underlying_type_t<TEnum>>
inline constexpr auto cycleMode(TEnum value, TEnum max) noexcept
    -> std::enable_if_t<std::is_enum_v<TEnum> && std::is_integral_v<TUnder>, TEnum>
{
	TUnder v = static_cast<TUnder>(value);
	TUnder m = static_cast<TUnder>(max);
	TEnum r = static_cast<TEnum>((v + 1) % m);
	return r;
}

template <typename E> class EnumRangeIterator
{
	static_assert(std::is_enum_v<E>, "EnumIterator requires an enum type");
	using U = std::underlying_type_t<E>;

  public:
	using iterator_category = std::forward_iterator_tag;
	using value_type = E;
	using difference_type = std::ptrdiff_t;

	constexpr explicit EnumRangeIterator(E value) noexcept : value_(static_cast<U>(value)) {}

	constexpr E operator*() const noexcept { return static_cast<E>(value_); }

	constexpr EnumRangeIterator& operator++() noexcept
	{
		++value_;
		return *this;
	}

	constexpr bool operator!=(const EnumRangeIterator& other) const noexcept
	{
		return value_ != other.value_;
	}

  private:
	U value_;
};
template <typename E> class EnumRange
{
	static_assert(std::is_enum_v<E>, "EnumRange requires an enum type");

  public:
	constexpr explicit EnumRange(E max) noexcept : begin_(static_cast<E>(0)), end_(max) {}
	constexpr EnumRange(E start, E max) noexcept : begin_(start), end_(max) {}
	constexpr EnumRangeIterator<E> begin() const noexcept { return EnumRangeIterator<E>(begin_); }
	constexpr EnumRangeIterator<E> end() const noexcept { return EnumRangeIterator<E>(end_); }
	constexpr size_t size() const noexcept { return size_t(end_) - size_t(begin_); }

  private:
	E begin_;
	E end_;
};