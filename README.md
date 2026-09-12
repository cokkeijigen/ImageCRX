# ImageCRX - *.CRX (Circus Engine) Format Image Tool.
```
Usage:
  -png <file.crx or directory>  [-out <output>] ; Convert CRX to PNG (outputs .png and .ctl)
  -crx <file.png or directory>  [-out <output>] ; Convert PNG to CRX (uses .ctl if exists)
```
## How to build
 **Requirements:** Visual Studio 2022 or newer, CMake, VCPKG(optional)
> pwsh
>```pwsh
>$env:VCPKG_ROOT = "C:\path\to\vcpkg"
>```
>cmd
> ```cmd
> set VCPKG_ROOT=C:\path\to\vcpkg
> ```
> cmd or pwsh
> ```
> git clone https://github.com/cokkeijigen/ImageCRX.git
> cd .\ImageCRX\image_crx
> .\build.bat
> ```
