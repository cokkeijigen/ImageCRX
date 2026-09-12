#include <iostream>
#include <format>
#include <string>
#include <vector>
#include <args.hpp>
#include <image_crx.hpp>
#include <image_png.hpp>
#include <windows.h>
#include <console.hpp>
#include <xmem.hpp>

console::helper_t console::helper{ L"" PROJECT_NAME " v" PROJECT_VERSION };

namespace image_crx
{

	static auto to_png(std::wstring_view input, std::wstring_view output = {}) noexcept
	{
		std::vector<uint8_t> buffer{};
		{
			xfsys::file file{ xfsys::open(input, xfsys::read, false) };
			if (!file.is_open())
			{
				xcout::helper.write(L"open failed: %ls\n", input.data());
				return;
			}

			const size_t size{ file.size() };
			if (size == 0)
			{
				return;
			}

			const size_t bytes{ file.read(buffer, size) };
			if (bytes != size)
			{
				return;
			}
		}

		const crx::image_crx image{ buffer };
		if (!image.is_valid())
		{
			xcout::helper.write(L"invalid crx: %ls\n", input.data());
			return;
		}

		xfsys::file out_file{};
		{
			if (output.empty())
			{
				out_file = xfsys::create(xfsys::extname_change(input, L".png"));
			}
			else if(xfsys::is_directory(output))
			{
				std::wstring name{ xfsys::extname_change(xfsys::path::name(input), L".png") };
				out_file = xfsys::create(output, name);
			}
			else 
			{
				std::wstring_view parent{ xfsys::path::parent(output) };
				if (!parent.empty() && !xfsys::create_directory(parent, true))
				{
					return;
				}
				out_file = xfsys::create(output);
			}

			if (!out_file.is_open())
			{
				return;
			}
		}

		std::vector<uint8_t> bytes{};
		if (!image.decode(bytes, crx::fmt::bgra))
		{
			xcout::helper.write(L"decode failed: %ls\n", input.data());
			return;
		}

		const png::pixels data
		{
			.data   = bytes,
			.width  = image.width(),
			.height = image.height(),
			.format = png::fmt::bgra
		};

		if (png::encode(data, bytes))
		{
			out_file.write(bytes, bytes.size());

			const size_t header_size{ image.view().header_size() };
			if (header_size > 0)
			{
				const std::wstring ctl_path
				{
					output.empty() ?
					xfsys::extname_change(input,  L".ctl") :
					xfsys::extname_change(output, L".ctl")
				};
				xfsys::file ctl_file{ xfsys::open(ctl_path, xfsys::write, true) };
				if (ctl_file.is_open())
				{
					ctl_file.write(image.view().raw().data(), header_size);
				}
			}
		}
		else
		{
			xcout::helper.write(L"png encode failed: %ls\n", input.data());
		}
	}

	static auto to_crx(std::wstring_view input, std::wstring_view output = {}) noexcept
	{
		xmem::buffer<uint8_t> buffer{};
		{
			xfsys::file file{ xfsys::open(input, xfsys::read, false) };
			if (!file.is_open())
			{
				xcout::helper.write(L"open failed: %ls\n", input.data());
				return;
			}

			const size_t size{ file.size() };
			if (size == 0)
			{
				return;
			}

			const size_t bytes{ file.read(buffer, size) };
			if (bytes != size)
			{
				return;
			}
		}

		const std::optional<png::psize> info{ png::size(buffer) };
		if (!info.has_value())
		{
			xcout::helper.write(L"invalid png: %ls\n", input.data());
			return;
		}

		std::vector<uint8_t> decoded{};
		if (!png::decode(buffer, decoded, png::fmt::bgra))
		{
			xcout::helper.write(L"png decode failed: %ls\n", input.data());
			return;
		}

		const crx::pixels data
		{
			.data   = decoded,
			.width  = info->width,
			.height = info->height,
			.format = crx::fmt::bgra
		};

		std::vector<uint8_t> ctl_data{};
		{
			const std::wstring ctl_path{ xfsys::extname_change(input, L".ctl") };
			xfsys::file        ctl_file{ xfsys::open(ctl_path, xfsys::read, false) };
			if (ctl_file.is_open())
			{
				const size_t ctl_size{ ctl_file.size() };
				if (ctl_size >= sizeof(crx::crxg_header))
				{
					ctl_file.read(ctl_data, ctl_size);
				}
			}
		}

		std::vector<uint8_t> crx_bytes{};
		if (!crx::image_crx::encode(data, crx_bytes, ctl_data))
		{
			xcout::helper.write(L"crx encode failed: %ls\n", input.data());
			return;
		}

		xfsys::file out_file{};
		{
			if (output.empty())
			{
				out_file = xfsys::create(xfsys::extname_change(input, L".crx"));
			}
			else if (xfsys::is_directory(output)) 
			{
				std::wstring name{ xfsys::extname_change(xfsys::path::name(input), L".crx") };
				out_file = xfsys::create(output, name);
			}
			else
			{
				std::wstring_view parent{ xfsys::path::parent(output) };
				if (!parent.empty() && !xfsys::create_directory(parent, true))
				{
					return;
				}
				out_file = xfsys::create(output);
			}

			if (!out_file.is_open())
			{
				return;
			}
		}
		out_file.write(crx_bytes, crx_bytes.size());
	}

	inline static auto main(const image_crx::args& args) noexcept -> int
	{
		xcout::helper.writeline(PROJECT_NAME " v" PROJECT_VERSION ". by iTsukezigen.");
		switch (args.type)
		{
		case image_crx::args::to_crx:
		{
			if (xfsys::is_directory(args.input)) 
			{
				for (const auto& entry : xfsys::dir::iter(args.input)) 
				{
					if (!entry.is_file()) 
					{
						continue;
					}
					image_crx::to_crx(entry.full_path(), args.output);
				}
				xcout::helper.write("done.");
			}
			else if(xfsys::is_file(args.input))
			{
				image_crx::to_crx(args.input, args.output);
				xcout::helper.write("done.");
			}
			break;
		}
		case image_crx::args::to_png:
		{
			if (xfsys::is_directory(args.input)) 
			{
				for (const auto& entry : xfsys::dir::iter(args.input))
				{
					if (!entry.is_file())
					{
						continue;
					}
					image_crx::to_png(entry.full_path(), args.output);
				}
				xcout::helper.write("done.");
			}
			else if (xfsys::is_file(args.input)) 
			{
				image_crx::to_png(args.input, args.output);
				xcout::helper.write("done.");
			}
			break;
		}
		default:
		{
			constexpr const char message[]
			{
				"Usage:\n"
				"  -png <file.crx or directory>  [-out <output>] ; Convert CRX to PNG (outputs .png and .ctl)\n"
				"  -crx <file.png or directory>  [-out <output>] ; Convert PNG to CRX (uses .ctl if exists)\n"
			};
			xcout::helper.write(message);
			break;
		}
		}

		xcout::helper.read_anykey();
		return {};
	}

	extern "C" auto main(void) noexcept -> int
	{
		int argc{};
		const LPWSTR  cmds{ ::GetCommandLineW() };
		const LPWSTR* argv{ ::CommandLineToArgvW(cmds, &argc) };
		return image_crx::main({ argc, argv });
	}
}