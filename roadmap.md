# RTGL1 Development Roadmap

## Priorities

1. **Denoiser fix: color clamping** (current branch)
   - Bug: a shotgun muzzle flash "burns in" on walls/floor/ceiling
     (temporal history is not validated against color changes)
   - A-SVGF-style clamping of the accumulated history in
     CmSVGFTemporalAccumulation.comp against the current frame's
     neighborhood min/max (depth/normal-weighted taps)
   - Lower SPEC_MAX_ACCUM_FRAMES (256 -> ~96)

2. **AMD optimizations** (from the previous roadmap)
   - Any-hit shader cost: skip texture sampling for materials
     without an alpha layer; use the opaque hit group for such geometry
   - Volumetric checkerboard / resolution scaling
   - Wave64 verification (SVGF compute passes, divergence in the
     reflection loop)
   - FP16 packing of G-buffer data (optional)

3. **Path Tracing upgrade: deeper paths** (Priority #1, separate branch)
   - Replace the fixed 1 specular + 1 diffuse bounce path in
     RtRaygenIndirect.inl with a loop over N bounces:
     NEE via the light-grid reservoir at every vertex +
     BSDF-sampled next direction + Russian roulette
   - Accumulate the full path pdf into the ReSTIR reservoir
   - Radiance cache / ReSTIR PT - later; radiance cache officially
     requires FidelityFX SDK 2+, we are on 1.1.4

4. **FSR 3.1 Frame Generation** (swapchain proxy + UI resource
   registration in the game)

5. **DirectX 12 renderer** - postponed

6. **AMD FSR Ray Regeneration** - waiting for official Vulkan support
   from AMD

## Done

- [x] FSR 3.1 + explicit FSR 2 / FSR 3.1 version selection (v2.1.0, 2026-08)
- [x] RDNA 4 (RX 9070 XT) startup crash fix
