# WinPixEventRuntime

GPU/CPU event markers for [PIX](https://devblogs.microsoft.com/pix/). MIT licensed; sourced from the [WinPixEventRuntime](https://www.nuget.org/packages/WinPixEventRuntime) NuGet package.

## Enable in AetherEngine

Configure with PIX support (Debug recommended):

```bat
cmake -B build/vs2022 -DAETHER_ENABLE_PIX=ON
cmake --build build/vs2022 --config Debug
```

On first configure, CMake downloads the NuGet package into `ThirdParty/WinPixEventRuntime/CMakeFiles/` (build tree). To vendor offline, extract the `.nupkg` to `ThirdParty/WinPixEventRuntime/package/` and pass:

```bat
cmake -B build -DWINPIX_EVENT_RUNTIME_ROOT=ThirdParty/WinPixEventRuntime/package
```

## Runtime

- `dx12_debug.json`: `usePixMarkers` (default `true` when built with PIX), `loadPixGpuCapturer` (optional programmatic GPU capture).
- `WinPixEventRuntime.dll` is copied next to `Runtime.exe` at build time.
- Install [PIX](https://devblogs.microsoft.com/pix/download/) for GPU capture; markers work in PIX and RenderDoc without it.
