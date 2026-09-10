#pragma once
#include <ranges>
#include <xfsys.hpp>
#include <xstr.hpp>

namespace image_crx
{
	struct args 
	{
		enum type
		{
			unused = 0,
			to_png = 1,
			to_crx = 2
		};

		args::type type{ args::unused };
		std::wstring  input{};
		std::wstring output{};

		args(const int argc, const wchar_t* const argv[]) noexcept 
		{
			if (argc < 2)
			{
				return;
			}

			int args_next{ 3 };
			std::wstring_view arg1{ argv[1] };

			if (argc >= 3) 
			{
				if (arg1 == L"-png")
				{
					this->type = args::to_png;
				}
				else if (arg1 == L"-crx")
				{
					this->type = args::to_crx;
				}
			}

			if (this->type != args::unused)
			{
				this->input.assign(argv[2]);
			}
			else
			{
				if (xfsys::extname_check(arg1, L".png"))
				{
					this->type = args::to_crx;
				}
				else if (xfsys::extname_check(arg1, L".crx"))
				{
					this->type = args::to_png;
				}
				if (this->type != args::unused)
				{
					this->input.assign(arg1);
					args_next = 2;
				}
			}

			if (this->type == args::unused) 
			{
				return;
			}

			for (int i{ args_next }; i < argc; ++i)
			{
				std::wstring_view arg{ argv[i] };
				if (arg == L"-out")
				{
					if (i + 1 < argc)
					{
						this->output.assign(argv[i + 1]);
					}
					break;
				}
			}
		}
	};

}