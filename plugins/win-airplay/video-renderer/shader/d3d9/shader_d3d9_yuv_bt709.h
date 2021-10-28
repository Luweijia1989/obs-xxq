#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
//
// Parameters:
//
//   sampler2D USampler;
//   sampler2D VSampler;
//   sampler2D YSampler;
//
//
// Registers:
//
//   Name         Reg   Size
//   ------------ ----- ----
//   YSampler     s0       1
//   USampler     s1       1
//   VSampler     s2       1
//

    ps_2_0
    def c0, -0.0627451017, -0.501960814, -0.501960814, 1
    def c1, 1.16439998, 0, 1.79270005, 0
    def c2, 1.16439998, -0.213200003, -0.532899976, 0
    def c3, 1.16439998, 2.11240005, 0, 0
    dcl t0.xy
    dcl_2d s0
    dcl_2d s1
    dcl_2d s2
    texld r0, t0, s0
    texld r1, t0, s1
    texld r2, t0, s2
    mov r0.y, r1.x
    mov r0.z, r2.x
    add r0.xyz, r0, c0
    dp3 r1.x, r0, c1
    dp3 r1.y, r0, c2
    dp2add r1.z, r0, c3, c3.z
    mov r1.w, c0.w
    mov oC0, r1

// approximately 12 instruction slots used (3 texture, 9 arithmetic)
#endif

const BYTE shader_d3d9_yuv_bt709[] =
{
      0,   2, 255, 255, 254, 255, 
     56,   0,  67,  84,  65,  66, 
     28,   0,   0,   0, 179,   0, 
      0,   0,   0,   2, 255, 255, 
      3,   0,   0,   0,  28,   0, 
      0,   0,   0,   1,   0,   0, 
    172,   0,   0,   0,  88,   0, 
      0,   0,   3,   0,   1,   0, 
      1,   0,   0,   0, 100,   0, 
      0,   0,   0,   0,   0,   0, 
    116,   0,   0,   0,   3,   0, 
      2,   0,   1,   0,   0,   0, 
    128,   0,   0,   0,   0,   0, 
      0,   0, 144,   0,   0,   0, 
      3,   0,   0,   0,   1,   0, 
      0,   0, 156,   0,   0,   0, 
      0,   0,   0,   0,  85,  83, 
     97, 109, 112, 108, 101, 114, 
      0, 171, 171, 171,   4,   0, 
     12,   0,   1,   0,   1,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,  86,  83,  97, 109, 
    112, 108, 101, 114,   0, 171, 
    171, 171,   4,   0,  12,   0, 
      1,   0,   1,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
     89,  83,  97, 109, 112, 108, 
    101, 114,   0, 171, 171, 171, 
      4,   0,  12,   0,   1,   0, 
      1,   0,   1,   0,   0,   0, 
      0,   0,   0,   0, 112, 115, 
     95,  50,  95,  48,   0,  77, 
    105,  99, 114, 111, 115, 111, 
    102, 116,  32,  40,  82,  41, 
     32,  72,  76,  83,  76,  32, 
     83, 104,  97, 100, 101, 114, 
     32,  67, 111, 109, 112, 105, 
    108, 101, 114,  32,  49,  48, 
     46,  49,   0, 171,  81,   0, 
      0,   5,   0,   0,  15, 160, 
    129, 128, 128, 189, 129, 128, 
      0, 191, 129, 128,   0, 191, 
      0,   0, 128,  63,  81,   0, 
      0,   5,   1,   0,  15, 160, 
     15,  11, 149,  63,   0,   0, 
      0,   0,  50, 119, 229,  63, 
      0,   0,   0,   0,  81,   0, 
      0,   5,   2,   0,  15, 160, 
     15,  11, 149,  63,  26,  81, 
     90, 190,  34, 108,   8, 191, 
      0,   0,   0,   0,  81,   0, 
      0,   5,   3,   0,  15, 160, 
     15,  11, 149,  63, 144,  49, 
      7,  64,   0,   0,   0,   0, 
      0,   0,   0,   0,  31,   0, 
      0,   2,   0,   0,   0, 128, 
      0,   0,   3, 176,  31,   0, 
      0,   2,   0,   0,   0, 144, 
      0,   8,  15, 160,  31,   0, 
      0,   2,   0,   0,   0, 144, 
      1,   8,  15, 160,  31,   0, 
      0,   2,   0,   0,   0, 144, 
      2,   8,  15, 160,  66,   0, 
      0,   3,   0,   0,  15, 128, 
      0,   0, 228, 176,   0,   8, 
    228, 160,  66,   0,   0,   3, 
      1,   0,  15, 128,   0,   0, 
    228, 176,   1,   8, 228, 160, 
     66,   0,   0,   3,   2,   0, 
     15, 128,   0,   0, 228, 176, 
      2,   8, 228, 160,   1,   0, 
      0,   2,   0,   0,   2, 128, 
      1,   0,   0, 128,   1,   0, 
      0,   2,   0,   0,   4, 128, 
      2,   0,   0, 128,   2,   0, 
      0,   3,   0,   0,   7, 128, 
      0,   0, 228, 128,   0,   0, 
    228, 160,   8,   0,   0,   3, 
      1,   0,   1, 128,   0,   0, 
    228, 128,   1,   0, 228, 160, 
      8,   0,   0,   3,   1,   0, 
      2, 128,   0,   0, 228, 128, 
      2,   0, 228, 160,  90,   0, 
      0,   4,   1,   0,   4, 128, 
      0,   0, 228, 128,   3,   0, 
    228, 160,   3,   0, 170, 160, 
      1,   0,   0,   2,   1,   0, 
      8, 128,   0,   0, 255, 160, 
      1,   0,   0,   2,   0,   8, 
     15, 128,   1,   0, 228, 128, 
    255, 255,   0,   0
};