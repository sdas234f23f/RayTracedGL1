# Changelog

## v3.0.0

### Added
- **Q2RTX-style core rendering path** — a full new pipeline alongside the legacy one, switched at runtime by the host flag `RG_DEBUG_DRAW_Q2RTX_CORE_BIT` (`rt_core_q2rtx` cvar in vkquake-rt):
  - **ASVGF denoiser** ported from Q2RTX (`asvgf_*.comp`): temporal accumulation, low-frequency (YCoCg luma-SH) and high-frequency/specular atrous filtering, checkerboard interleave
  - **Checkerboard interleave + TAAU** (Q2RTX `taa`) replaces FSR/DLSS upscaling on the new path; FSR 2/3 and DLSS remain available on the legacy path
  - ReSTIR (direct/indirect) remains the lighting solution — `CmQ2Adapter` converts its output into the ASVGF color format (LF_SH / LF_COCG / HF / SPEC)
  - 29 new Q2-format framebuffers (ASVGF colors, history/moments/RNG, TAA history)
- **Fog volumes** — port of Q2RTX `fog.c` + `find_fog_volumes` / `evaluate_fog`:
  - New public API: `rgSetFogVolumes`, `RgFogVolume` (AABB via two diagonal points, color, half-extinction distance, optional soft face), `RG_MAX_FOG_VOLUMES` = 8
  - `CmQ2Fog.comp` blends up to two closest fog volumes along the camera ray in HDR, before tonemapping; works on both the legacy and the new Q2RTX path
- **Q2RTX noise-aware tone mapping** (histogram + curve + apply, Eilertsen et al. + NVIDIA mods)
- **Sun shadow map + god rays** — depth-only world render from the sun's view, ray-marched volumetric light at half-res with a bilateral filter
- **Procedural physical sky** — `RG_SKY_TYPE_PROCEDURAL`: analytic single-scattering atmosphere (Rayleigh + Mie), sun disc, procedural fBm clouds (no textures), HDR cubemap 1024², cached when clouds are off

### Fixed
- **std140 layout of scalar arrays in generated C headers** — GLSL std140 aligns every array element to 16 bytes, but the header generator emitted scalar arrays with a 4-byte stride (e.g. `uint[8]` became `uint32_t[8]` = 32 bytes instead of 128), shifting every field after the first scalar array by 96 bytes on the CPU side. The GPU then read garbage (fog volumes silently never rendered while host-side diagnostics looked correct). The generator now emits `count*4` scalars per std140 array element

## v2.2.0

### Fixed
- **Denoised ghosting** — `CmSVGFTemporalAccumulation.comp` reworked to remove ghosting artifacts in the SVGF/ASVGF temporal accumulation

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
