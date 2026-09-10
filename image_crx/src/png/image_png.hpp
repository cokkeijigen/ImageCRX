#pragma once
#include <algorithm>
#include <span>
#include <vector>
#include <optional>

namespace png
{
	struct pixels
	{
		enum type : int8_t
		{
			rgba = 1,
			bgra = 2,
			argb = 3,
			abgr = 4,

			RGBA = 1,
			BGRA = 2,
			ARGB = 3,
			ABGR = 4,

			rgb = 5,
			bgr = 6,
			RGB = 5,
			BGR = 6,
		};

		std::span<uint8_t> data{};
		int     width{}, height{};
		pixels::type     format{};
	};
	using fmt = pixels::type;

	struct psize
	{
		int width{}, height{};
	};

	extern auto   size(std::span<uint8_t> data) noexcept -> std::optional<png::psize>;
	extern auto decode(std::span<uint8_t> data, std::vector<uint8_t>& output, pixels::type format) noexcept -> bool;
	extern auto encode(const pixels& data, std::vector<uint8_t>& output) noexcept -> bool;

	inline auto encode(const pixels& data) noexcept -> std::optional<std::vector<uint8_t>> 
	{
		std::vector<uint8_t> result{};
		if (png::encode(data, result)) 
		{
			return result;
		}
		return std::nullopt;
	}

	inline auto decode(std::span<uint8_t> data, pixels::type format) noexcept -> std::optional<std::vector<uint8_t>> 
	{
		std::vector<uint8_t> result{};
		if (png::decode(data, result, format)) 
		{
			return result;
		}
	}

	inline auto decode(std::vector<uint8_t>& data, pixels::type format) noexcept -> bool 
	{
		return png::decode(data, data, format);
	}
}