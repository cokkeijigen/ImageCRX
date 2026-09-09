#include <iostream>
#include <windows.h>
#include <console.hpp>

console::helper_t console::helper{ L"" PROJECT_NAME " v" PROJECT_VERSION };
namespace image_crx
{

	static inline auto main(const int argc, const wchar_t* const argv[]) noexcept -> int
	{

		return {};
	}

	extern "C" auto main(void) noexcept -> int
	{
		int argc{};
		const LPWSTR  cmds{::GetCommandLineW()};
		const LPWSTR* argv{ ::CommandLineToArgvW(cmds, &argc) };
		return image_crx::main(argc, argv);
	}
}