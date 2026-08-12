# Changelog (quake-fsr31-support)

## v2.1.0

### Added
- **AMD FSR 3.1 upscaler** via FidelityFX SDK 1.1.4 (`ffxCreateContext` / `ffxDispatch` / `ffxQuery` API)
- `RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR3` — new public enum value for FSR 3.1
- `RG_RENDER_RESOLUTION_MODE_NATIVE_AA` — Native AA mode (render at 1.0x, FSR 3.1 anti-aliasing only)
- AMD-signed prebuilt `amd_fidelityfx_vk.dll` required at runtime (driver overlay detection depends on Authenticode signature)

### Changed
- FSR 3.1 is always enabled (no compile-time flag; `RG_USE_FSR3` is unconditional)
- `RG_RENDER_UPSCALE_TECHNIQUE_AMD_FSR2` is kept as a legacy alias — all FSR paths now use FSR 3.1 internally

### Removed
- Old FSR2 code (`Source/FSR2.cpp`, `Source/FSR2.h`)
- `RG_WITH_FSR3` CMake option — FSR 3.1 is always built-in

### Fixed
- **AMD RDNA 4 (RX 9070 XT) crash at startup with `VK_ERROR_OUT_OF_DEVICE_MEMORY`** — VMA custom pools (`texturesStagingPool` / `texturesFinalPool`) used a fixed `memoryTypeIndex` that pointed to the wrong memory heap on RDNA 4. Fix: on AMD GPUs, pools are no longer created; `VMA_MEMORY_USAGE_CPU_ONLY` and `VMA_MEMORY_USAGE_GPU_ONLY` are used directly, letting VMA pick the correct heap. Non-AMD GPUs are unaffected. Fixes [sultim-t/vkquake-rt#45](https://github.com/sultim-t/vkquake-rt/issues/45)
- Missing `VK_KHR_get_memory_requirements2` device extension (caused crash in `ffxCreateContext`)
- DLL digital signature required by AMD driver overlay for FSR detection
