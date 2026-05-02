#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace MikuMikuWorld
{
	class Utilities
	{
	  public:
		static std::string getCurrentDateTime();
	};

	enum class ResultStatus
	{
		Success,
		Warning,
		Error
	};

	class Result
	{
	  private:
		ResultStatus status;
		std::string message;

	  public:
		Result(ResultStatus _status, std::string _msg = "") : status{ _status }, message{ _msg } {}

		ResultStatus getStatus() const { return status; }
		std::string getMessage() const { return message; }
		bool isOk() const { return status == ResultStatus::Success; }

		static Result Ok() { return Result(ResultStatus::Success); }
	};

	constexpr static const char* boolToString(bool value) { return value ? "true" : "false"; }

	template <typename, typename = void> struct size_type_trait
	{
		using type = std::size_t;
	};

	template <typename T>
	struct size_type_trait<T, std::void_t<typename std::remove_reference_t<T>::size_type>>
	{
		using type = typename std::remove_reference_t<T>::size_type;
	};

	template <typename ArrayType, typename IndexType,
	          typename SizeType = typename size_type_trait<ArrayType>::type>
	static auto arrayGetItemSafe(ArrayType& arr, IndexType idx) -> decltype(*std::begin(arr))
	{
		SizeType index = static_cast<SizeType>(idx);
		if (index < 0 || index >= std::size(arr))
			throw std::range_error("Index out of range!");
		return *std::next(std::begin(arr), index);
	}

	template <typename Cont>
	static inline bool isArrayIndexInBounds(size_t index, const Cont& arr)
	{
		return index >= 0 && index < std::size(arr);
	}

	template <typename ArrayType, typename ValueType, typename RetType>
	static RetType arrayFindOrDefault(ArrayType& arr, ValueType&& val, RetType def)
	{
		typename size_type_trait<ArrayType>::type i = 0;
		for (auto&& item : arr)
			if (item == val)
				return static_cast<RetType>(i);
			else
				++i;
		return def;
	}
}
