#include <image_png.hpp>
#include <png.h>
#include <cstring>
#include <csetjmp>

namespace png
{
	auto size(std::span<uint8_t> data) noexcept -> std::optional<png::psize>
	{
		if (data.empty())
		{
			return std::nullopt;
		}

		png_image image{};
		image.version = PNG_IMAGE_VERSION;

		if (::png_image_begin_read_from_memory(&image, data.data(), data.size()) == 0)
		{
			::png_image_free(&image);
			return std::nullopt;
		}

		const png::psize result
		{
			.width  = static_cast<int>(image.width),
			.height = static_cast<int>(image.height)
		};

		::png_image_free(&image);
		return result;
	}

	auto decode(std::span<uint8_t> data, std::vector<uint8_t>& output, pixels::type format) noexcept -> bool
	{
		if (data.empty())
		{
			return false;
		}

		uint32_t out_format{};
		switch (format)
		{
		case pixels::type::rgba: out_format = PNG_FORMAT_RGBA; break;
		case pixels::type::bgra: out_format = PNG_FORMAT_BGRA; break;
		case pixels::type::argb: out_format = PNG_FORMAT_ARGB; break;
		case pixels::type::abgr: out_format = PNG_FORMAT_ABGR; break;
		case pixels::type::rgb:  out_format = PNG_FORMAT_RGB;  break;
		case pixels::type::bgr:  out_format = PNG_FORMAT_BGR;  break;
		default:                 return false;
		}

		png_image image{};
		image.version = PNG_IMAGE_VERSION;

		if (::png_image_begin_read_from_memory(&image, data.data(), data.size()) == 0)
		{
			return false;
		}

		image.format = out_format;

		std::vector<uint8_t> temp{};
		temp.resize(PNG_IMAGE_SIZE(image));

		if (::png_image_finish_read(&image, nullptr, temp.data(), 0, nullptr) == 0)
		{
			::png_image_free(&image);
			return false;
		}

		::png_image_free(&image);
		output = std::move(temp);

		return true;
	}

	auto encode(const pixels& data, std::vector<uint8_t>& output) noexcept -> bool
	{
		if (data.data.empty() || data.width <= 0 || data.height <= 0)
		{
			return false;
		}

		int color_type{};
		bool  swap_bgr{}, swap_alpha{};
		switch (data.format)
		{
		case pixels::type::rgba: color_type = PNG_COLOR_TYPE_RGBA; break;
		case pixels::type::bgra: color_type = PNG_COLOR_TYPE_RGBA; swap_bgr   = true; break;
		case pixels::type::argb: color_type = PNG_COLOR_TYPE_RGBA; swap_alpha = true; break;
		case pixels::type::abgr: color_type = PNG_COLOR_TYPE_RGBA; swap_bgr   = swap_alpha = true; break;
		case pixels::type::rgb:  color_type = PNG_COLOR_TYPE_RGB ; break;
		case pixels::type::bgr:  color_type = PNG_COLOR_TYPE_RGB ; swap_bgr   = true; break;
		default:  return false;
		}

		const int channels{ (data.format == pixels::type::rgb || data.format == pixels::type::bgr) ? 3 : 4 };
		const size_t expected{ static_cast<size_t>(data.width) * data.height * channels };
		if (data.data.size() < expected)
		{
			return false;
		}

		png_structp png{ ::png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr) };
		if (png == nullptr)
		{
			return false;
		}

		png_infop info_ptr{ ::png_create_info_struct(png) };
		if (info_ptr == nullptr)
		{
			::png_destroy_write_struct(&png, nullptr);
			return false;
		}

		if (setjmp(png_jmpbuf(png)) != 0)
		{
			::png_destroy_write_struct(&png, &info_ptr);
			return false;
		}

		struct write_buffer
		{
			std::vector<uint8_t> buffer{};
			size_t               pos{};

			static auto on_write(png_structp png, png_bytep data, png_size_t length) noexcept -> void
			{
				const auto self{ static_cast<write_buffer*>(::png_get_io_ptr(png)) };
				if (self == nullptr || length == 0)
				{
					return;
				}
				const size_t need{ self->pos + length };
				if (self->buffer.size() < need)
				{
					self->buffer.resize(need);
				}
				std::memcpy(self->buffer.data() + self->pos, data, length);
				self->pos += length;
			}

			static auto on_flush(png_structp) noexcept -> void
			{
			}
		} target{};

		::png_set_write_fn(png, &target, &write_buffer::on_write, &write_buffer::on_flush);
		if (swap_bgr)
		{
			::png_set_bgr(png);
		}
		if (swap_alpha)
		{
			::png_set_swap_alpha(png);
		}

		::png_set_IHDR
		(
			png, info_ptr,
			static_cast<png_uint_32>(data.width ),
			static_cast<png_uint_32>(data.height),
			8, color_type,
			PNG_INTERLACE_NONE, 
			PNG_COMPRESSION_TYPE_DEFAULT, 
			PNG_FILTER_TYPE_DEFAULT
		);

		std::vector<png_bytep> rows{};
		rows.resize(static_cast<size_t>(data.height));
		
		const size_t stride_bytes{ static_cast<size_t>(data.width) * channels };
		for (int y{}; y < data.height; ++y)
		{
			rows[static_cast<size_t>(y)] = data.data.data() + static_cast<size_t>(y) * stride_bytes;
		}

		::png_write_info (png, info_ptr);
		::png_write_image(png, rows.data());
		::png_write_end  (png, nullptr);
		::png_destroy_write_struct(&png, &info_ptr);

		output = std::move(target.buffer);
		return true;
	}
}
