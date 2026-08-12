# Changelog

## v2.1.0

### Added
- **AMD FSR 3.1 upscaler** via FidelityFX SDK 1.1.4 (`ffxCreateContext` / `ffxDispatch` / `ffxQuery` API)
- `RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR3` — new public enum value for FSR 3.1
- `RG_RENDER_RESOLUTION_MODE_NATIVE_AA` — Native AA mode (render at 1.0x, FSR 3.1 anti-aliasing only)
- AMD-signed prebuilt `amd_fidelityfx_vk.dll` required at runtime (driver overlay detection depends on Authenticode signature)
- **Explicit FSR version selection** — RTGL1 tells the FidelityFX framework which FSR algorithm to use via `ffxOverrideVersion` instead of letting the framework pick the "best" provider by itself:
  - `RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2` → FSR 2.x
  - `RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR3` → FSR 3.1
- Switching between FSR 2 and FSR 3.1 works at runtime (the FidelityFX context is recreated on version change)
- If the requested FSR version is not present in `amd_fidelityfx_vk.dll`, RTGL1 falls back to the other version and prints a message to the game console (`pfnPrint`)
- `rgIsRenderUpscaleTechniqueAvailable` now actually checks whether the requested FSR version exists in the DLL

### Changed
- FSR 3.1 is always enabled (no compile-time flag; `RG_USE_FSR3` is unconditional)
- `Source/FSR3.{h,cpp}` renamed to `Source/FSR.{h,cpp}`, class `FSR3` renamed to `FSR` (it now handles both FSR 2 and FSR 3.1)

### Removed
- Old FSR2 code (`Source/FSR2.cpp`, `Source/FSR2.h`)
- `RG_WITH_FSR3` CMake option — FSR 3.1 is always built-in

### Fixed
- **AMD RDNA 4 (RX 9070 XT) crash at startup with `VK_ERROR_OUT_OF_DEVICE_MEMORY`** — VMA custom pools (`texturesStagingPool` / `texturesFinalPool`) used a fixed `memoryTypeIndex` that pointed to the wrong memory heap on RDNA 4. Fix: on AMD GPUs, pools are no longer created; `VMA_MEMORY_USAGE_CPU_ONLY` and `VMA_MEMORY_USAGE_GPU_ONLY` are used directly, letting VMA pick the correct heap. Non-AMD GPUs are unaffected. Fixes [sultim-t/vkquake-rt#45](https://github.com/sultim-t/vkquake-rt/issues/45)
- Missing `VK_KHR_get_memory_requirements2` device extension (caused crash in `ffxCreateContext`)
- DLL digital signature required by AMD driver overlay for FSR detection
