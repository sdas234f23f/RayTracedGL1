# RTGL1 AMD Optimization Roadmap

Target hardware: AMD RDNA 2/3/4 (RX 6000/7000/9070). Benchmark GPU: RX 9070 XT.

## Current pipeline (context)

Path tracing: 1 SPP, blue noise, checkerboard 2x1, jitter; BVH via
VK_KHR_acceleration_structure; ReSTIR DI (light grid 16^3) + ReSTIR GI
(world-space reservoirs); volumetric fog 160x88x64 with per-voxel ReSTIR;
denoiser: SVGF + ASVGF gradient estimation (4 a-trous), separate
direct-diffuse / indirect-SH / specular channels.

## Already AMD-friendly (no action needed)

- ReSTIR DI + GI — low ray counts (critical for RDNA)
- Checkerboard 2x1 (half-resolution tracing)
- ASVGF gradient estimation — AMD research (Schied et al.), already in denoiser
- PREFER_FAST_TRACE_BIT for static BLAS
- Shadow rays use SkipClosestHitShader
- Compute groups 16x16 = 256 threads = 4 wave64
- FSR 3.1 with explicit FSR 2 / FSR 3.1 selection + Native AA mode
- CAS sharpening

## Roadmap

### 1. Any-hit shader cost reduction
Priority: high | Impact: medium-high | Effort: medium

Any-hit (`RtAlphaTest.rahit`) runs for all alpha-tested geometry and is
expensive on RDNA (software traversal path), including a texture fetch.

- Add a "has alpha layer" flag to materials/geometry
- In any-hit: skip texture sampling when the flag is absent; decide from
  material color only, and use the opaque hit group for such geometry
- App side: geometry without alpha textures uploaded as
  RG_GEOMETRY_PASS_THROUGH_TYPE_OPAQUE (no any-hit at all)

Files: Source/Shaders/RtAlphaTest.rahit, RaygenCommon.h,
ShaderCommonGLSLFunc.h, material upload path (Scene/RasterizedDataCollector).

### 2. Volumetric resolution scaling
Priority: medium | Impact: medium | Effort: low

Fixed 160x88x64 (900K rays/frame) is a heavy fixed cost on RDNA.

- Add a resolution scale uniform; trace at reduced resolution and
  upsample in CmVolumetricProcess.comp
- Or apply the existing 2x1 checkerboard pattern over voxels with
  2-frame temporal accumulation

Files: Source/Shaders/RtVolumetric.rgen, CmVolumetricProcess.comp,
Source/Volumetric.cpp, Source/Generated/GenerateShaderCommon.py.

### 3. Wave64 verification (compute + divergence)
Priority: low | Impact: low-medium | Effort: low

- Profile SVGF passes on RDNA; check shared-memory usage in
  CmSVGFAtrous_Iter0.comp is wave64-friendly
- Reduce divergence in the reflection/refraction loop
  (RaygenPrimary.inl split) if profiling shows it

### 4. FP16 packing (optional, long-term)
Priority: low | Impact: low | Effort: medium

- Pack normals/position/radiance in G-buffer and denoiser intermediates
  to float16 formats to cut bandwidth

## Explicitly not planned

- FidelityFX Denoiser (FfxDenoiser): only shadow + reflection denoising;
  cannot denoise diffuse GI. RTGL1's SVGF is strictly more capable and
  already includes AMD's ASVGF gradient estimation.

## Done

- [x] FSR 3.1 integration + explicit FSR2/FSR3.1 selection (2026-08)
- [x] RDNA 4 (RX 9070 XT) startup crash fix — VMA pools on AMD
