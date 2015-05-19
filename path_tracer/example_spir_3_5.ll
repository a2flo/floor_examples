; ModuleID = 'spir_3_5.bc'
target datalayout = "e-i64:64-v16:16-v24:32-v32:32-v48:64-v96:128-v192:256-v256:256-v512:512-v1024:1024"
target triple = "spir64-unknown-unknown"

%"struct.std::__1::array" = type { [64 x %class.vector3] }
%class.vector3 = type { %union.anon }
%union.anon = type { %struct.anon }
%struct.anon = type { float, float, float }
%"struct.std::__1::array.39" = type { [32 x %class.vector3.38] }
%class.vector3.38 = type { %union.anon.18 }
%union.anon.18 = type { %struct.anon.17 }
%struct.anon.17 = type { i32, i32, i32 }
%"struct.std::__1::array.40" = type { [8 x %struct.material] }
%struct.material = type { %class.vector3, %class.vector3, float, %class.vector3, %class.vector3, %class.vector3 }
%"struct.std::__1::array.63" = type { [32 x i32] }
%class.vector2.23 = type { %union.anon.24 }
%union.anon.24 = type { %struct.anon.25 }
%struct.anon.25 = type { i32, i32 }
%struct.ray = type { %class.vector3, %class.vector3 }
%"struct.simple_intersector::intersection" = type { %class.vector3, %class.vector3, float, i32 }
%"struct.std::__1::pair" = type { %class.vector3, %class.vector3 }

@_ZL16cornell_vertices = internal addrspace(2) constant %"struct.std::__1::array" { [64 x %class.vector3] [%class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0.000000e+00 } } }, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4041266660000000, float 0x404B666660000000, float 0x4036B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4041266660000000, float 0x404B666660000000, float 0x40409999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40354CCCC0000000, float 0x404B666660000000, float 0x40409999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40354CCCC0000000, float 0x404B666660000000, float 0x4036B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 1.650000e+01, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 1.650000e+01, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 1.650000e+01, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 1.650000e+01, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 0.000000e+00, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 1.650000e+01, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 1.650000e+01, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 0.000000e+00, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 0.000000e+00, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 1.650000e+01, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 1.650000e+01, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 0.000000e+00, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 0.000000e+00, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 1.650000e+01, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 1.650000e+01, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 0.000000e+00, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 0.000000e+00, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 1.650000e+01, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 1.650000e+01, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 0.000000e+00, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 3.300000e+01, float 0x4038B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 3.300000e+01, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 3.300000e+01, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 3.300000e+01, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 0.000000e+00, float 0x4038B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 3.300000e+01, float 0x4038B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 3.300000e+01, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 0.000000e+00, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 0.000000e+00, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 3.300000e+01, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 3.300000e+01, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 0.000000e+00, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 0.000000e+00, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 3.300000e+01, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 3.300000e+01, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 0.000000e+00, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 0.000000e+00, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 3.300000e+01, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 3.300000e+01, float 0x4038B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 0.000000e+00, float 0x4038B33340000000 } } }] }, align 4
@_ZL15cornell_indices = internal addrspace(2) constant %"struct.std::__1::array.39" { [32 x %class.vector3.38] [%class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 0, i32 1, i32 2 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 0, i32 2, i32 3 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 4, i32 5, i32 6 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 4, i32 6, i32 7 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 8, i32 9, i32 10 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 8, i32 10, i32 11 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 12, i32 13, i32 14 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 12, i32 14, i32 15 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 16, i32 17, i32 18 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 16, i32 18, i32 19 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 20, i32 21, i32 22 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 20, i32 22, i32 23 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 24, i32 25, i32 26 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 24, i32 26, i32 27 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 28, i32 29, i32 30 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 28, i32 30, i32 31 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 32, i32 33, i32 34 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 32, i32 34, i32 35 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 36, i32 37, i32 38 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 36, i32 38, i32 39 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 40, i32 41, i32 42 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 40, i32 42, i32 43 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 44, i32 45, i32 46 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 44, i32 46, i32 47 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 48, i32 49, i32 50 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 48, i32 50, i32 51 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 52, i32 53, i32 54 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 52, i32 54, i32 55 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 56, i32 57, i32 58 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 56, i32 58, i32 59 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 60, i32 61, i32 62 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 60, i32 62, i32 63 } } }] }, align 4
@_ZL17cornell_materials = internal addrspace(2) constant %"struct.std::__1::array.40" { [8 x %struct.material] [%struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0x3FDA1A1980000000, float 0x3FDA1A1980000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FC99999A0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 { %union.anon { %struct.anon { float 1.500000e+01, float 1.500000e+01, float 1.500000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x3FB04C26C0000000, float 0x3FB04C26C0000000, float 0x3FB04C26C0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0x3FDA1A1980000000, float 0x3FDA1A1980000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0x3FDA1A1980000000, float 0x3FDA1A1980000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x3FDA1A1980000000, float 0.000000e+00 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x3FC09DF4E0000000, float 0.000000e+00 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0.000000e+00, float 0.000000e+00 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0.000000e+00, float 0.000000e+00 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0x3FDA1A1980000000, float 0x3FDA1A1980000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FB99999A0000000, float 0x3FB99999A0000000, float 0x3FB99999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x3FECCCCCC0000000, float 0x3FECCCCCC0000000, float 0x3FECCCCCC0000000 } } }, float 1.000000e+02, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FA04C26C0000000, float 0x3FA04C26C0000000, float 0x3FA04C26C0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x402D388940000000, float 0x402D388940000000, float 0x402D388940000000 } } } }] }, align 4
@_ZL18cornell_object_map = internal addrspace(2) constant %"struct.std::__1::array.63" { [32 x i32] [i32 0, i32 0, i32 1, i32 1, i32 2, i32 2, i32 3, i32 3, i32 4, i32 4, i32 5, i32 5, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7] }, align 4

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_fmodf"(float %y, float %x) #0 {
  %1 = tail call float @_Z4fmodff(float %y, float %x) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z4fmodff(float, float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_sqrtf"(float %val) #0 {
  %1 = tail call float @_Z4sqrtf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z4sqrtf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_inv_sqrtf"(float %val) #0 {
  %1 = tail call float @_Z5rsqrtf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z5rsqrtf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_absf"(float %val) #0 {
  %1 = tail call float @_Z4fabsf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z4fabsf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_floorf"(float %val) #0 {
  %1 = tail call float @_Z5floorf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z5floorf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_ceilf"(float %val) #0 {
  %1 = tail call float @_Z4ceilf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z4ceilf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_roundf"(float %val) #0 {
  %1 = tail call float @_Z5roundf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z5roundf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_truncf"(float %val) #0 {
  %1 = tail call float @_Z5truncf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z5truncf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_rintf"(float %val) #0 {
  %1 = tail call float @_Z4rintf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z4rintf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_sinf"(float %val) #0 {
  %1 = tail call float @_Z3sinf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z3sinf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_cosf"(float %val) #0 {
  %1 = tail call float @_Z3cosf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z3cosf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_tanf"(float %val) #0 {
  %1 = tail call float @_Z3tanf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z3tanf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_asinf"(float %val) #0 {
  %1 = tail call float @_Z4asinf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z4asinf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_acosf"(float %val) #0 {
  %1 = tail call float @_Z4acosf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z4acosf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_atanf"(float %val) #0 {
  %1 = tail call float @_Z4atanf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z4atanf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_atan2f"(float %y, float %x) #0 {
  %1 = tail call float @_Z5atan2ff(float %y, float %x) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z5atan2ff(float, float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_fmaf"(float %a, float %b, float %c) #0 {
  %1 = tail call float @_Z3fmafff(float %a, float %b, float %c) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z3fmafff(float, float, float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_expf"(float %val) #0 {
  %1 = tail call float @_Z3expf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z3expf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_logf"(float %val) #0 {
  %1 = tail call float @_Z3logf(float %val) #4
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @_Z3logf(float) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_fmodd"(double %y, double %x) #0 {
  %1 = tail call double @_Z4fmoddd(double %y, double %x) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z4fmoddd(double, double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_sqrtd"(double %val) #0 {
  %1 = tail call double @_Z4sqrtd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z4sqrtd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_inv_sqrtd"(double %val) #0 {
  %1 = tail call double @_Z5rsqrtd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z5rsqrtd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_absd"(double %val) #0 {
  %1 = tail call double @_Z4fabsd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z4fabsd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_floord"(double %val) #0 {
  %1 = tail call double @_Z5floord(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z5floord(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_ceild"(double %val) #0 {
  %1 = tail call double @_Z4ceild(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z4ceild(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_roundd"(double %val) #0 {
  %1 = tail call double @_Z5roundd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z5roundd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_truncd"(double %val) #0 {
  %1 = tail call double @_Z5truncd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z5truncd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_rintd"(double %val) #0 {
  %1 = tail call double @_Z4rintd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z4rintd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_sind"(double %val) #0 {
  %1 = tail call double @_Z3sind(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z3sind(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_cosd"(double %val) #0 {
  %1 = tail call double @_Z3cosd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z3cosd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_tand"(double %val) #0 {
  %1 = tail call double @_Z3tand(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z3tand(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_asind"(double %val) #0 {
  %1 = tail call double @_Z4asind(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z4asind(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_acosd"(double %val) #0 {
  %1 = tail call double @_Z4acosd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z4acosd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_atand"(double %val) #0 {
  %1 = tail call double @_Z4atand(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z4atand(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_atan2d"(double %y, double %x) #0 {
  %1 = tail call double @_Z5atan2dd(double %y, double %x) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z5atan2dd(double, double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_fmad"(double %a, double %b, double %c) #0 {
  %1 = tail call double @_Z3fmaddd(double %a, double %b, double %c) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z3fmaddd(double, double, double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_expd"(double %val) #0 {
  %1 = tail call double @_Z3expd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z3expd(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_logd"(double %val) #0 {
  %1 = tail call double @_Z3logd(double %val) #4
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @_Z3logd(double) #1

; Function Attrs: nounwind
define spir_kernel void @_Z10path_tracePU3AS17vector3IfEjj7vector2IjE(%class.vector3 addrspace(1)* nocapture %img, i32 %iteration, i32 %seed, %class.vector2.23* byval nocapture  %img_size) #2 {
  %random_seed = alloca i32, align 4
  %r = alloca %struct.ray, align 4
  %color = alloca %class.vector3, align 4
  %1 = tail call i64 @_Z13get_global_idj(i32 0) #4
  %2 = trunc i64 %1 to i32
  %3 = add i32 %2, %iteration
  %4 = and i32 %seed, 31
  %5 = and i32 %4, %3
  %6 = shl i32 %2, %5
  %7 = xor i32 %6, %seed
  %8 = add i32 %2, %seed
  %9 = getelementptr inbounds %class.vector2.23* %img_size, i64 0, i32 0, i32 0, i32 0
  %10 = load i32* %9, align 4, !tbaa !7
  %11 = mul i32 %8, %10
  %12 = getelementptr inbounds %class.vector2.23* %img_size, i64 0, i32 0, i32 0, i32 1
  %13 = load i32* %12, align 4, !tbaa !12
  %14 = mul i32 %11, %13
  %15 = add i32 %14, %seed
  %16 = add i32 %15, %7
  %17 = xor i32 %16, 1392237036
  %18 = urem i32 %2, %10
  %19 = udiv i32 %2, %13
  %20 = uitofp i32 %18 to float
  %21 = uitofp i32 %19 to float
  %22 = mul i32 %17, 16807
  %23 = lshr i32 %22, 9
  %24 = or i32 %23, 1065353216
  %25 = bitcast i32 %24 to float
  %26 = fadd fast float %25, -1.000000e+00
  %27 = mul i32 %17, 282475249
  store i32 %27, i32* %random_seed, align 4, !tbaa !13
  %28 = lshr i32 %27, 9
  %29 = or i32 %28, 1065353216
  %30 = bitcast i32 %29 to float
  %31 = fadd fast float %30, -1.000000e+00
  %32 = fadd fast float %20, %26
  %33 = fadd fast float %21, %31
  %34 = uitofp i32 %10 to float
  %35 = uitofp i32 %13 to float
  %36 = fdiv fast float %34, %35, !fpmath !14
  %37 = tail call float @_Z3tanf(float 0x3FD38C3540000000) #4
  %38 = fmul fast float %37, -2.000000e+00
  %39 = fmul fast float %36, %38
  %40 = fdiv fast float %39, %34, !fpmath !14
  %41 = fdiv fast float 0.000000e+00, %34, !fpmath !14
  %42 = fdiv fast float -0.000000e+00, %35, !fpmath !14
  %43 = fdiv fast float %38, %35, !fpmath !14
  %44 = fdiv fast float 0.000000e+00, %35, !fpmath !14
  %45 = fmul fast float %39, 5.000000e-01
  %46 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 0
  store float 0x403BCCCCC0000000, float* %46, align 4, !tbaa !15
  %47 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 1
  store float 0x403B4CCCC0000000, float* %47, align 4, !tbaa !18
  %48 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 2
  store float -8.000000e+01, float* %48, align 4, !tbaa !19
  %49 = fmul fast float %40, %32
  %50 = fmul fast float %41, %32
  %51 = fsub fast float %49, %45
  %52 = fadd fast float %37, %50
  %53 = fadd fast float %50, 1.000000e+00
  %54 = fmul fast float %42, %33
  %55 = fmul fast float %33, %43
  %56 = fmul fast float %33, %44
  %57 = fadd fast float %54, %51
  %58 = fadd fast float %52, %55
  %59 = fadd fast float %53, %56
  %60 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %57, float* %60, align 4, !tbaa !15
  %61 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %58, float* %61, align 4, !tbaa !18
  %62 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %59, float* %62, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer16compute_radianceILj0ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %color, %struct.ray* dereferenceable(24) %r, i32* dereferenceable(4) %random_seed, i1 zeroext true)
  %63 = icmp eq i32 %iteration, 0
  %64 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 0
  %65 = load float* %64, align 4
  br i1 %63, label %66, label %75

; <label>:66                                      ; preds = %0
  %67 = and i64 %1, 4294967295
  %68 = getelementptr inbounds %class.vector3 addrspace(1)* %img, i64 %67, i32 0, i32 0, i32 0
  store float %65, float addrspace(1)* %68, align 4, !tbaa !15
  %69 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 1
  %70 = load float* %69, align 4, !tbaa !18
  %71 = getelementptr inbounds %class.vector3 addrspace(1)* %img, i64 %67, i32 0, i32 0, i32 1
  store float %70, float addrspace(1)* %71, align 4, !tbaa !18
  %72 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 2
  %73 = load float* %72, align 4, !tbaa !19
  %74 = getelementptr inbounds %class.vector3 addrspace(1)* %img, i64 %67, i32 0, i32 0, i32 2
  store float %73, float addrspace(1)* %74, align 4, !tbaa !19
  br label %104

; <label>:75                                      ; preds = %0
  %76 = add i32 %iteration, 1
  %77 = uitofp i32 %76 to float
  %78 = fdiv fast float 1.000000e+00, %77, !fpmath !14
  %79 = fsub fast float 1.000000e+00, %78
  %80 = and i64 %1, 4294967295
  %81 = getelementptr inbounds %class.vector3 addrspace(1)* %img, i64 %80
  %82 = getelementptr inbounds %class.vector3 addrspace(1)* %81, i64 0, i32 0, i32 0, i32 0
  %83 = load float addrspace(1)* %82, align 4, !tbaa !15
  %84 = getelementptr inbounds %class.vector3 addrspace(1)* %img, i64 %80, i32 0, i32 0, i32 1
  %85 = load float addrspace(1)* %84, align 4, !tbaa !18
  %86 = getelementptr inbounds %class.vector3 addrspace(1)* %img, i64 %80, i32 0, i32 0, i32 2
  %87 = load float addrspace(1)* %86, align 4, !tbaa !19
  %88 = fmul fast float %83, %79
  %89 = fmul fast float %79, %85
  %90 = fmul fast float %79, %87
  %91 = fmul fast float %78, %65
  %92 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 1
  %93 = load float* %92, align 4, !tbaa !18
  %94 = fmul fast float %78, %93
  %95 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 2
  %96 = load float* %95, align 4, !tbaa !19
  %97 = fmul fast float %78, %96
  %98 = fadd fast float %88, %91
  %99 = fadd fast float %89, %94
  %100 = fadd fast float %90, %97
  %101 = getelementptr inbounds %class.vector3 addrspace(1)* %81, i64 0, i32 0, i32 0, i32 0
  store float %98, float addrspace(1)* %101, align 4, !tbaa !15
  %102 = getelementptr inbounds %class.vector3 addrspace(1)* %img, i64 %80, i32 0, i32 0, i32 1
  store float %99, float addrspace(1)* %102, align 4, !tbaa !18
  %103 = getelementptr inbounds %class.vector3 addrspace(1)* %img, i64 %80, i32 0, i32 0, i32 2
  store float %100, float addrspace(1)* %103, align 4, !tbaa !19
  br label %104

; <label>:104                                     ; preds = %75, %66
  ret void
}

; Function Attrs: nounwind readnone
declare i64 @_Z13get_global_idj(i32) #1

; Function Attrs: nounwind
declare void @llvm.lifetime.start(i64, i8* nocapture) #3

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer16compute_radianceILj0ELi0EEE7vector3IfERK3rayRjb(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r, i32* nocapture dereferenceable(4) %random_seed, i1 zeroext %sample_emission) #2 align 2 {
  %p = alloca %"struct.simple_intersector::intersection", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %class.vector3, align 4
  %3 = alloca %class.vector3, align 4
  call void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* sret %p, %struct.ray* dereferenceable(24) %r)
  %4 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 3
  %5 = load i32* %4, align 4, !tbaa !20
  %6 = icmp ugt i32 %5, 7
  br i1 %6, label %7, label %11

; <label>:7                                       ; preds = %0
  %8 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %8, align 4, !tbaa !15
  %9 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %9, align 4, !tbaa !18
  %10 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %10, align 4, !tbaa !19
  br label %58

; <label>:11                                      ; preds = %0
  %12 = zext i32 %5 to i64
  %13 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12
  br i1 %sample_emission, label %14, label %21

; <label>:14                                      ; preds = %11
  %15 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 0
  %16 = load float* %15, align 4, !tbaa !15
  %17 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 1
  %18 = load float* %17, align 4, !tbaa !18
  %19 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 2
  %20 = load float* %19, align 4, !tbaa !19
  br label %21

; <label>:21                                      ; preds = %14, %11
  %22 = phi float [ %20, %14 ], [ 0.000000e+00, %11 ]
  %23 = phi float [ %18, %14 ], [ 0.000000e+00, %11 ]
  %24 = phi float [ %16, %14 ], [ 0.000000e+00, %11 ]
  %25 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %26 = load float* %25, align 4, !tbaa !15
  %27 = fsub fast float -0.000000e+00, %26
  %28 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %29 = load float* %28, align 4, !tbaa !18
  %30 = fsub fast float -0.000000e+00, %29
  %31 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %32 = load float* %31, align 4, !tbaa !19
  %33 = fsub fast float -0.000000e+00, %32
  %34 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 0
  store float %27, float* %34, align 4, !tbaa !15
  %35 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 1
  store float %30, float* %35, align 4, !tbaa !18
  %36 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 2
  store float %33, float* %36, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer27compute_direct_illuminationERK7vector3IfERKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %1, %class.vector3* dereferenceable(12) %2, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %37 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !15
  %39 = fadd fast float %24, %38
  %40 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !18
  %42 = fadd fast float %23, %41
  %43 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %44 = load float* %43, align 4, !tbaa !19
  %45 = fadd fast float %22, %44
  call void @_ZN18simple_path_tracer29compute_indirect_illuminationILj0EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %r, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %46 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %47 = load float* %46, align 4, !tbaa !15
  %48 = fadd fast float %39, %47
  %49 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %50 = load float* %49, align 4, !tbaa !18
  %51 = fadd fast float %42, %50
  %52 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %53 = load float* %52, align 4, !tbaa !19
  %54 = fadd fast float %45, %53
  %55 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %48, float* %55, align 4, !tbaa !15
  %56 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %51, float* %56, align 4, !tbaa !18
  %57 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %54, float* %57, align 4, !tbaa !19
  br label %58

; <label>:58                                      ; preds = %21, %7
  ret void
}

; Function Attrs: nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) #3

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r) #2 align 2 {
  %1 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %2 = load float* %1, align 4, !tbaa !18
  %3 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %4 = load float* %3, align 4, !tbaa !19
  %5 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %6 = load float* %5, align 4, !tbaa !15
  %7 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 0
  %8 = load float* %7, align 4, !tbaa !15
  %9 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 1
  %10 = load float* %9, align 4, !tbaa !18
  %11 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 2
  %12 = load float* %11, align 4, !tbaa !19
  br label %13

; <label>:13                                      ; preds = %_ZNK7vector3IbE3anyIbvEEbv.exit.thread, %0
  %object.019 = phi i32 [ 8, %0 ], [ %object.1, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %storemerge17 = phi i64 [ 0, %0 ], [ %141, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %14 = phi float [ 0x7FF0000000000000, %0 ], [ %140, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %15 = phi float [ 0.000000e+00, %0 ], [ %139, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %16 = phi float [ 0.000000e+00, %0 ], [ %138, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %17 = phi float [ 0.000000e+00, %0 ], [ %137, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %18 = getelementptr inbounds %"struct.std::__1::array.39"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.39" addrspace(2)* @_ZL15cornell_indices to i64) to %"struct.std::__1::array.39"*), i64 0, i32 0, i64 %storemerge17, i32 0, i32 0, i32 0
  %19 = load i32* %18, align 4, !tbaa !24
  %20 = zext i32 %19 to i64
  %21 = getelementptr inbounds %"struct.std::__1::array.39"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.39" addrspace(2)* @_ZL15cornell_indices to i64) to %"struct.std::__1::array.39"*), i64 0, i32 0, i64 %storemerge17, i32 0, i32 0, i32 1
  %22 = load i32* %21, align 4, !tbaa !26
  %23 = zext i32 %22 to i64
  %24 = getelementptr inbounds %"struct.std::__1::array.39"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.39" addrspace(2)* @_ZL15cornell_indices to i64) to %"struct.std::__1::array.39"*), i64 0, i32 0, i64 %storemerge17, i32 0, i32 0, i32 2
  %25 = load i32* %24, align 4, !tbaa !27
  %26 = zext i32 %25 to i64
  %27 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %20, i32 0, i32 0, i32 0
  %28 = load float* %27, align 4, !tbaa !15
  %29 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %26, i32 0, i32 0, i32 0
  %30 = load float* %29, align 4, !tbaa !15
  %31 = fsub fast float %28, %30
  %32 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %20, i32 0, i32 0, i32 1
  %33 = load float* %32, align 4, !tbaa !18
  %34 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %26, i32 0, i32 0, i32 1
  %35 = load float* %34, align 4, !tbaa !18
  %36 = fsub fast float %33, %35
  %37 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %20, i32 0, i32 0, i32 2
  %38 = load float* %37, align 4, !tbaa !19
  %39 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %26, i32 0, i32 0, i32 2
  %40 = load float* %39, align 4, !tbaa !19
  %41 = fsub fast float %38, %40
  %42 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %23, i32 0, i32 0, i32 0
  %43 = load float* %42, align 4, !tbaa !15
  %44 = fsub fast float %43, %30
  %45 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %23, i32 0, i32 0, i32 1
  %46 = load float* %45, align 4, !tbaa !18
  %47 = fsub fast float %46, %35
  %48 = getelementptr inbounds %"struct.std::__1::array"* inttoptr (i64 ptrtoint (%"struct.std::__1::array" addrspace(2)* @_ZL16cornell_vertices to i64) to %"struct.std::__1::array"*), i64 0, i32 0, i64 %23, i32 0, i32 0, i32 2
  %49 = load float* %48, align 4, !tbaa !19
  %50 = fsub fast float %49, %40
  %51 = fmul fast float %2, %50
  %52 = fmul fast float %47, %4
  %53 = fsub fast float %51, %52
  %54 = fmul fast float %44, %4
  %55 = fmul fast float %50, %6
  %56 = fsub fast float %54, %55
  %57 = fmul fast float %47, %6
  %58 = fmul fast float %44, %2
  %59 = fsub fast float %57, %58
  %60 = fmul fast float %31, %53
  %61 = fmul fast float %36, %56
  %62 = fadd fast float %60, %61
  %63 = fmul fast float %41, %59
  %64 = fadd fast float %63, %62
  %65 = fcmp olt float %64, 0.000000e+00
  br i1 %65, label %66, label %_ZN10const_math3absIfvEET_S1_.exit

; <label>:66                                      ; preds = %13
  %67 = fsub fast float -0.000000e+00, %64
  br label %_ZN10const_math3absIfvEET_S1_.exit

_ZN10const_math3absIfvEET_S1_.exit:               ; preds = %66, %13
  %68 = phi float [ %67, %66 ], [ %64, %13 ]
  %69 = fcmp ugt float %68, 0x3EE4F8B580000000
  br i1 %69, label %70, label %_ZNK7vector3IbE3anyIbvEEbv.exit.thread

; <label>:70                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit
  %71 = fdiv fast float 1.000000e+00, %64, !fpmath !14
  %72 = fsub fast float %8, %30
  %73 = fsub fast float %10, %35
  %74 = fsub fast float %12, %40
  %75 = fmul fast float %41, %73
  %76 = fmul fast float %36, %74
  %77 = fsub fast float %75, %76
  %78 = fmul fast float %31, %74
  %79 = fmul fast float %41, %72
  %80 = fsub fast float %78, %79
  %81 = fmul fast float %36, %72
  %82 = fmul fast float %31, %73
  %83 = fsub fast float %81, %82
  %84 = fmul fast float %53, %72
  %85 = fmul fast float %56, %73
  %86 = fadd fast float %84, %85
  %87 = fmul fast float %59, %74
  %88 = fadd fast float %86, %87
  %89 = fmul fast float %71, %88
  %90 = fmul fast float %6, %77
  %91 = fmul fast float %2, %80
  %92 = fadd fast float %90, %91
  %93 = fmul fast float %83, %4
  %94 = fadd fast float %93, %92
  %95 = fmul fast float %71, %94
  %96 = fcmp uge float %89, 0.000000e+00
  %97 = fcmp uge float %95, 0.000000e+00
  %or.cond12 = and i1 %96, %97
  br i1 %or.cond12, label %_ZNK7vector3IbE3anyIbvEEbv.exit, label %_ZNK7vector3IbE3anyIbvEEbv.exit.thread

_ZNK7vector3IbE3anyIbvEEbv.exit:                  ; preds = %70
  %98 = fsub fast float 1.000000e+00, %89
  %99 = fsub fast float %98, %95
  %100 = fcmp olt float %99, 0.000000e+00
  br i1 %100, label %_ZNK7vector3IbE3anyIbvEEbv.exit.thread, label %101

; <label>:101                                     ; preds = %_ZNK7vector3IbE3anyIbvEEbv.exit
  %102 = fmul fast float %44, %77
  %103 = fmul fast float %47, %80
  %104 = fadd fast float %102, %103
  %105 = fmul fast float %50, %83
  %106 = fadd fast float %105, %104
  %107 = fmul fast float %71, %106
  %108 = fcmp oge float %107, 0x3F1A36E2E0000000
  %109 = fcmp olt float %107, %14
  %or.cond = and i1 %108, %109
  br i1 %or.cond, label %110, label %_ZNK7vector3IbE3anyIbvEEbv.exit.thread

; <label>:110                                     ; preds = %101
  %111 = fmul fast float %36, %50
  %112 = fmul fast float %41, %47
  %113 = fsub fast float %111, %112
  %114 = fmul fast float %41, %44
  %115 = fmul fast float %31, %50
  %116 = fsub fast float %114, %115
  %117 = fmul fast float %31, %47
  %118 = fmul fast float %36, %44
  %119 = fsub fast float %117, %118
  %120 = fcmp oeq float %113, 0.000000e+00
  %121 = fcmp oeq float %116, 0.000000e+00
  %or.cond.i = and i1 %120, %121
  %122 = fcmp oeq float %119, 0.000000e+00
  %or.cond13 = and i1 %or.cond.i, %122
  br i1 %or.cond13, label %_ZNK7vector3IfE10normalizedEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %110
  %123 = fmul fast float %113, %113
  %124 = fmul fast float %116, %116
  %125 = fadd fast float %123, %124
  %126 = fmul fast float %119, %119
  %127 = fadd fast float %126, %125
  %128 = tail call float @_Z5rsqrtf(float %127) #4
  %129 = fmul fast float %113, %128
  %130 = fmul fast float %116, %128
  %131 = fmul fast float %119, %128
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i, %110
  %132 = phi float [ %131, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %119, %110 ]
  %133 = phi float [ %130, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %116, %110 ]
  %134 = phi float [ %129, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %113, %110 ]
  %135 = getelementptr inbounds %"struct.std::__1::array.63"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.63" addrspace(2)* @_ZL18cornell_object_map to i64) to %"struct.std::__1::array.63"*), i64 0, i32 0, i64 %storemerge17
  %136 = load i32* %135, align 4, !tbaa !28
  br label %_ZNK7vector3IbE3anyIbvEEbv.exit.thread

_ZNK7vector3IbE3anyIbvEEbv.exit.thread:           ; preds = %_ZNK7vector3IfE10normalizedEv.exit, %101, %_ZNK7vector3IbE3anyIbvEEbv.exit, %70, %_ZN10const_math3absIfvEET_S1_.exit
  %137 = phi float [ %17, %_ZN10const_math3absIfvEET_S1_.exit ], [ %17, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %132, %_ZNK7vector3IfE10normalizedEv.exit ], [ %17, %101 ], [ %17, %70 ]
  %138 = phi float [ %16, %_ZN10const_math3absIfvEET_S1_.exit ], [ %16, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %133, %_ZNK7vector3IfE10normalizedEv.exit ], [ %16, %101 ], [ %16, %70 ]
  %139 = phi float [ %15, %_ZN10const_math3absIfvEET_S1_.exit ], [ %15, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %134, %_ZNK7vector3IfE10normalizedEv.exit ], [ %15, %101 ], [ %15, %70 ]
  %140 = phi float [ %14, %_ZN10const_math3absIfvEET_S1_.exit ], [ %14, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %107, %_ZNK7vector3IfE10normalizedEv.exit ], [ %14, %101 ], [ %14, %70 ]
  %object.1 = phi i32 [ %object.019, %_ZN10const_math3absIfvEET_S1_.exit ], [ %object.019, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %136, %_ZNK7vector3IfE10normalizedEv.exit ], [ %object.019, %101 ], [ %object.019, %70 ]
  %141 = add i64 %storemerge17, 1
  %142 = icmp ult i64 %141, 32
  br i1 %142, label %13, label %143

; <label>:143                                     ; preds = %_ZNK7vector3IbE3anyIbvEEbv.exit.thread
  %object.1.lcssa = phi i32 [ %object.1, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %.lcssa28 = phi float [ %140, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %.lcssa27 = phi float [ %139, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %.lcssa26 = phi float [ %138, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %.lcssa = phi float [ %137, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %144 = fmul fast float %.lcssa28, %6
  %145 = fmul fast float %.lcssa28, %2
  %146 = fmul fast float %.lcssa28, %4
  %147 = fadd fast float %144, %8
  %148 = fadd fast float %145, %10
  %149 = fadd fast float %146, %12
  %150 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 0
  store float %147, float* %150, align 4, !tbaa !15
  %151 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 1
  store float %148, float* %151, align 4, !tbaa !18
  %152 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 2
  store float %149, float* %152, align 4, !tbaa !19
  %153 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %.lcssa27, float* %153, align 4, !tbaa !15
  %154 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %.lcssa26, float* %154, align 4, !tbaa !18
  %155 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %.lcssa, float* %155, align 4, !tbaa !19
  %156 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 2
  store float %.lcssa28, float* %156, align 4, !tbaa !29
  %157 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 3
  store i32 %object.1.lcssa, i32* %157, align 4, !tbaa !20
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer27compute_direct_illuminationERK7vector3IfERKN18simple_intersector12intersectionERK8materialRj(%class.vector3* noalias nocapture sret %agg.result, %class.vector3* nocapture  dereferenceable(12) %eye_direction, %"struct.simple_intersector::intersection"* nocapture  dereferenceable(32) %p, %struct.material* nocapture  dereferenceable(64) %mat, i32* nocapture dereferenceable(4) %random_seed) #2 align 2 {
  %r = alloca %struct.ray, align 4
  %ret = alloca %"struct.simple_intersector::intersection", align 4
  %1 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %1, align 4, !tbaa !15
  %2 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %2, align 4, !tbaa !18
  %3 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %3, align 4, !tbaa !19
  %4 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 0
  %5 = load float* %4, align 4, !tbaa !15
  %6 = fcmp oeq float %5, 0.000000e+00
  %7 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 1
  %8 = load float* %7, align 4, !tbaa !18
  %9 = fcmp oeq float %8, 0.000000e+00
  %or.cond.i = and i1 %6, %9
  %10 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 2
  %11 = load float* %10, align 4, !tbaa !19
  %12 = fcmp oeq float %11, 0.000000e+00
  %or.cond33 = and i1 %or.cond.i, %12
  br i1 %or.cond33, label %_ZNK7vector3IfE10normalizedEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %0
  %13 = fmul fast float %5, %5
  %14 = fmul fast float %8, %8
  %15 = fadd fast float %13, %14
  %16 = fmul fast float %11, %11
  %17 = fadd fast float %15, %16
  %18 = tail call float @_Z5rsqrtf(float %17) #4
  %19 = fmul fast float %5, %18
  %20 = fmul fast float %8, %18
  %21 = fmul fast float %11, %18
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i, %0
  %22 = phi float [ %21, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %11, %0 ]
  %23 = phi float [ %20, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %8, %0 ]
  %24 = phi float [ %19, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %5, %0 ]
  %25 = getelementptr inbounds %class.vector3* %eye_direction, i64 0, i32 0, i32 0, i32 0
  %26 = load float* %25, align 4, !tbaa !15
  %27 = fcmp oeq float %26, 0.000000e+00
  %28 = getelementptr inbounds %class.vector3* %eye_direction, i64 0, i32 0, i32 0, i32 1
  %29 = load float* %28, align 4, !tbaa !18
  %30 = fcmp oeq float %29, 0.000000e+00
  %or.cond.i1 = and i1 %27, %30
  %31 = getelementptr inbounds %class.vector3* %eye_direction, i64 0, i32 0, i32 0, i32 2
  %32 = load float* %31, align 4, !tbaa !19
  %33 = fcmp oeq float %32, 0.000000e+00
  %or.cond34 = and i1 %or.cond.i1, %33
  br i1 %or.cond34, label %_ZNK7vector3IfE10normalizedEv.exit4, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i3

_ZNK7vector3IfE7is_nullEv.exit.thread.i3:         ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %34 = fmul fast float %26, %26
  %35 = fmul fast float %29, %29
  %36 = fadd fast float %34, %35
  %37 = fmul fast float %32, %32
  %38 = fadd fast float %36, %37
  %39 = tail call float @_Z5rsqrtf(float %38) #4
  %40 = fmul fast float %26, %39
  %41 = fmul fast float %29, %39
  %42 = fmul fast float %32, %39
  br label %_ZNK7vector3IfE10normalizedEv.exit4

_ZNK7vector3IfE10normalizedEv.exit4:              ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i3, %_ZNK7vector3IfE10normalizedEv.exit
  %43 = phi float [ %42, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3 ], [ %32, %_ZNK7vector3IfE10normalizedEv.exit ]
  %44 = phi float [ %41, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3 ], [ %29, %_ZNK7vector3IfE10normalizedEv.exit ]
  %45 = phi float [ %40, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3 ], [ %26, %_ZNK7vector3IfE10normalizedEv.exit ]
  %46 = load i32* %random_seed, align 4, !tbaa !13
  %47 = mul i32 %46, 16807
  %48 = lshr i32 %47, 9
  %49 = or i32 %48, 1065353216
  %50 = bitcast i32 %49 to float
  %51 = fadd fast float %50, -1.000000e+00
  %52 = mul i32 %46, 282475249
  store i32 %52, i32* %random_seed, align 4, !tbaa !13
  %53 = lshr i32 %52, 9
  %54 = or i32 %53, 1065353216
  %55 = bitcast i32 %54 to float
  %56 = fadd fast float %55, -1.000000e+00
  %57 = fmul fast float %51, 1.050000e+01
  %58 = fadd fast float %57, 0x4036B33340000000
  %59 = fmul fast float %56, -1.300000e+01
  %60 = fadd fast float %59, 0x4041266660000000
  %61 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %62 = load float* %61, align 4, !tbaa !15
  %63 = fsub fast float %60, %62
  %64 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %65 = load float* %64, align 4, !tbaa !18
  %66 = fsub fast float 0x404B666660000000, %65
  %67 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %68 = load float* %67, align 4, !tbaa !19
  %69 = fsub fast float %58, %68
  %70 = fmul fast float %63, %63
  %71 = fmul fast float %66, %66
  %72 = fadd fast float %71, %70
  %73 = fmul fast float %69, %69
  %74 = fadd fast float %73, %72
  %75 = fcmp ogt float %74, 0x3F1A36E2E0000000
  %..i15 = select i1 %75, float %74, float 0x3F1A36E2E0000000
  %76 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 0
  store float %62, float* %76, align 4, !tbaa !15
  %77 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 1
  store float %65, float* %77, align 4, !tbaa !18
  %78 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 2
  store float %68, float* %78, align 4, !tbaa !19
  %79 = fcmp oeq float %63, 0.000000e+00
  %80 = fcmp oeq float %66, 0.000000e+00
  %or.cond.i11 = and i1 %79, %80
  %81 = fcmp oeq float %69, 0.000000e+00
  %or.cond27 = and i1 %or.cond.i11, %81
  br i1 %or.cond27, label %_ZNK7vector3IfE10normalizedEv.exit14, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i13

_ZNK7vector3IfE7is_nullEv.exit.thread.i13:        ; preds = %_ZNK7vector3IfE10normalizedEv.exit4
  %82 = tail call float @_Z5rsqrtf(float %74) #4
  %83 = fmul fast float %63, %82
  %84 = fmul fast float %66, %82
  %85 = fmul fast float %69, %82
  br label %_ZNK7vector3IfE10normalizedEv.exit14

_ZNK7vector3IfE10normalizedEv.exit14:             ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i13, %_ZNK7vector3IfE10normalizedEv.exit4
  %86 = phi float [ %83, %_ZNK7vector3IfE7is_nullEv.exit.thread.i13 ], [ %63, %_ZNK7vector3IfE10normalizedEv.exit4 ]
  %87 = phi float [ %84, %_ZNK7vector3IfE7is_nullEv.exit.thread.i13 ], [ %66, %_ZNK7vector3IfE10normalizedEv.exit4 ]
  %88 = phi float [ %85, %_ZNK7vector3IfE7is_nullEv.exit.thread.i13 ], [ %69, %_ZNK7vector3IfE10normalizedEv.exit4 ]
  %89 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %86, float* %89, align 4
  %90 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %87, float* %90, align 4
  %91 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %88, float* %91, align 4
  call void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* sret %ret, %struct.ray* dereferenceable(24) %r)
  %92 = getelementptr inbounds %"struct.simple_intersector::intersection"* %ret, i64 0, i32 3
  %93 = load i32* %92, align 4, !tbaa !20
  %94 = icmp ugt i32 %93, 7
  br i1 %94, label %102, label %95

; <label>:95                                      ; preds = %_ZNK7vector3IfE10normalizedEv.exit14
  %96 = getelementptr inbounds %"struct.simple_intersector::intersection"* %ret, i64 0, i32 2
  %97 = load float* %96, align 4, !tbaa !29
  %98 = tail call float @_Z4sqrtf(float %..i15) #4
  %99 = fadd fast float %98, 0xBF1A36E2E0000000
  %100 = fcmp oge float %97, %99
  %101 = icmp eq i32 %93, 1
  %or.cond = or i1 %100, %101
  br i1 %or.cond, label %102, label %190

; <label>:102                                     ; preds = %95, %_ZNK7vector3IfE10normalizedEv.exit14
  %103 = fdiv fast float 1.000000e+00, %..i15, !fpmath !14
  %104 = fcmp ogt float %87, 0.000000e+00
  %..i10 = select i1 %104, float %87, float 0.000000e+00
  %105 = fmul fast float %24, %86
  %106 = fmul fast float %23, %87
  %107 = fadd fast float %106, %105
  %108 = fmul fast float %22, %88
  %109 = fadd fast float %107, %108
  %110 = fcmp ogt float %109, 0.000000e+00
  %..i9 = select i1 %110, float %109, float 0.000000e+00
  %111 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 0
  %112 = load float* %111, align 4, !tbaa !15
  %113 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 1
  %114 = load float* %113, align 4, !tbaa !18
  %115 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 2
  %116 = load float* %115, align 4, !tbaa !19
  %117 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 0
  %118 = load float* %117, align 4, !tbaa !15
  %119 = fcmp oeq float %118, 0.000000e+00
  br i1 %119, label %120, label %_ZNK7vector3IfE7is_nullEv.exit.thread

; <label>:120                                     ; preds = %102
  %121 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 1
  %122 = load float* %121, align 4, !tbaa !18
  %123 = fcmp oeq float %122, 0.000000e+00
  br i1 %123, label %_ZNK7vector3IfE7is_nullEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread

_ZNK7vector3IfE7is_nullEv.exit:                   ; preds = %120
  %124 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 2
  %125 = load float* %124, align 4, !tbaa !19
  %126 = fcmp oeq float %125, 0.000000e+00
  br i1 %126, label %178, label %_ZNK7vector3IfE7is_nullEv.exit.thread

_ZNK7vector3IfE7is_nullEv.exit.thread:            ; preds = %_ZNK7vector3IfE7is_nullEv.exit, %120, %102
  %127 = fsub fast float -0.000000e+00, %86
  %128 = fsub fast float -0.000000e+00, %87
  %129 = fsub fast float -0.000000e+00, %88
  %130 = fmul fast float %24, %127
  %131 = fmul fast float %23, %128
  %132 = fadd fast float %130, %131
  %133 = fmul fast float %22, %129
  %134 = fadd fast float %132, %133
  %135 = fmul fast float %134, 2.000000e+00
  %136 = fmul fast float %24, %135
  %137 = fmul fast float %23, %135
  %138 = fmul fast float %22, %135
  %139 = fsub fast float %127, %136
  %140 = fsub fast float %128, %137
  %141 = fsub fast float %129, %138
  %142 = fcmp oeq float %139, 0.000000e+00
  %143 = fcmp oeq float %140, 0.000000e+00
  %or.cond.i5 = and i1 %142, %143
  %144 = fcmp oeq float %141, 0.000000e+00
  %or.cond28 = and i1 %or.cond.i5, %144
  br i1 %or.cond28, label %_ZNK7vector3IfE10normalizedEv.exit8, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i7

_ZNK7vector3IfE7is_nullEv.exit.thread.i7:         ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread
  %145 = fmul fast float %139, %139
  %146 = fmul fast float %140, %140
  %147 = fadd fast float %145, %146
  %148 = fmul fast float %141, %141
  %149 = fadd fast float %148, %147
  %150 = tail call float @_Z5rsqrtf(float %149) #4
  %151 = fmul fast float %139, %150
  %152 = fmul fast float %140, %150
  %153 = fmul fast float %141, %150
  br label %_ZNK7vector3IfE10normalizedEv.exit8

_ZNK7vector3IfE10normalizedEv.exit8:              ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i7, %_ZNK7vector3IfE7is_nullEv.exit.thread
  %154 = phi float [ %153, %_ZNK7vector3IfE7is_nullEv.exit.thread.i7 ], [ %141, %_ZNK7vector3IfE7is_nullEv.exit.thread ]
  %155 = phi float [ %152, %_ZNK7vector3IfE7is_nullEv.exit.thread.i7 ], [ %140, %_ZNK7vector3IfE7is_nullEv.exit.thread ]
  %156 = phi float [ %151, %_ZNK7vector3IfE7is_nullEv.exit.thread.i7 ], [ %139, %_ZNK7vector3IfE7is_nullEv.exit.thread ]
  %157 = fmul fast float %45, %156
  %158 = fmul fast float %44, %155
  %159 = fadd fast float %158, %157
  %160 = fmul fast float %43, %154
  %161 = fadd fast float %160, %159
  %162 = fcmp ogt float %161, 0.000000e+00
  %..i = select i1 %162, float %161, float 0.000000e+00
  %163 = getelementptr inbounds %struct.material* %mat, i64 0, i32 2
  %164 = load float* %163, align 4, !tbaa !30
  %165 = tail call float @_Z3powff(float %..i, float %164) #4
  %166 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 0
  %167 = load float* %166, align 4, !tbaa !15
  %168 = fmul fast float %165, %167
  %169 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 1
  %170 = load float* %169, align 4, !tbaa !18
  %171 = fmul fast float %165, %170
  %172 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 2
  %173 = load float* %172, align 4, !tbaa !19
  %174 = fmul fast float %165, %173
  %175 = fadd fast float %112, %168
  %176 = fadd fast float %114, %171
  %177 = fadd fast float %116, %174
  br label %178

; <label>:178                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit8, %_ZNK7vector3IfE7is_nullEv.exit
  %179 = phi float [ %177, %_ZNK7vector3IfE10normalizedEv.exit8 ], [ %116, %_ZNK7vector3IfE7is_nullEv.exit ]
  %180 = phi float [ %176, %_ZNK7vector3IfE10normalizedEv.exit8 ], [ %114, %_ZNK7vector3IfE7is_nullEv.exit ]
  %181 = phi float [ %175, %_ZNK7vector3IfE10normalizedEv.exit8 ], [ %112, %_ZNK7vector3IfE7is_nullEv.exit ]
  %182 = fmul fast float %..i10, %..i9
  %183 = tail call float @_Z4sqrtf(float 1.863225e+04) #4
  %184 = fmul fast float %182, %183
  %185 = fmul fast float %103, %184
  %186 = fmul fast float %185, 1.500000e+01
  %187 = fmul fast float %181, %186
  %188 = fmul fast float %180, %186
  %189 = fmul fast float %179, %186
  store float %187, float* %1, align 4, !tbaa !15
  store float %188, float* %2, align 4, !tbaa !18
  store float %189, float* %3, align 4, !tbaa !19
  br label %190

; <label>:190                                     ; preds = %178, %95
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer29compute_indirect_illuminationILj0EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r, %"struct.simple_intersector::intersection"* nocapture  dereferenceable(32) %p, %struct.material* nocapture  dereferenceable(64) %mat, i32* nocapture dereferenceable(4) %random_seed) #2 align 2 {
  %dir.i5 = alloca %class.vector3, align 4
  %dir.i = alloca %class.vector3, align 4
  %normal = alloca %class.vector3, align 4
  %tb = alloca %"struct.std::__1::pair", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %struct.ray, align 4
  %rnormal = alloca %class.vector3, align 4
  %tb1 = alloca %"struct.std::__1::pair", align 4
  %3 = alloca %class.vector3, align 4
  %4 = alloca %struct.ray, align 4
  %5 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 0
  %6 = load float* %5, align 4, !tbaa !15
  %7 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %8 = load float* %7, align 4, !tbaa !15
  %9 = fmul fast float %6, %8
  %10 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 1
  %11 = load float* %10, align 4, !tbaa !18
  %12 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %13 = load float* %12, align 4, !tbaa !18
  %14 = fmul fast float %11, %13
  %15 = fadd fast float %9, %14
  %16 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 2
  %17 = load float* %16, align 4, !tbaa !19
  %18 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %19 = load float* %18, align 4, !tbaa !19
  %20 = fmul fast float %17, %19
  %21 = fadd fast float %15, %20
  %22 = fcmp ogt float %21, 0.000000e+00
  br i1 %22, label %23, label %29

; <label>:23                                      ; preds = %0
  %24 = fsub fast float -0.000000e+00, %6
  %25 = fsub fast float -0.000000e+00, %11
  %26 = fsub fast float -0.000000e+00, %17
  %27 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %24, float* %27, align 4, !tbaa !15
  %28 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %25, float* %28, align 4, !tbaa !18
  br label %32

; <label>:29                                      ; preds = %0
  %30 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %6, float* %30, align 4, !tbaa !15
  %31 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %11, float* %31, align 4, !tbaa !18
  br label %32

; <label>:32                                      ; preds = %29, %23
  %33 = phi float [ %11, %29 ], [ %25, %23 ]
  %34 = phi float [ %6, %29 ], [ %24, %23 ]
  %35 = phi float [ %17, %29 ], [ %26, %23 ]
  %36 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 2
  store float %35, float* %36, align 4
  %37 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !15
  %39 = fmul fast float %38, 0x3FCC6A7F00000000
  %40 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !18
  %42 = fmul fast float %41, 0x3FE69D4960000000
  %43 = fadd fast float %39, %42
  %44 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 2
  %45 = load float* %44, align 4, !tbaa !19
  %46 = fmul fast float %45, 0x3FB240B780000000
  %47 = fadd fast float %43, %46
  %48 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 0
  %49 = load float* %48, align 4, !tbaa !15
  %50 = fmul fast float %49, 0x3FCC6A7F00000000
  %51 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 1
  %52 = load float* %51, align 4, !tbaa !18
  %53 = fmul fast float %52, 0x3FE69D4960000000
  %54 = fadd fast float %50, %53
  %55 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 2
  %56 = load float* %55, align 4, !tbaa !19
  %57 = fmul fast float %56, 0x3FB240B780000000
  %58 = fadd fast float %54, %57
  %59 = load i32* %random_seed, align 4, !tbaa !13
  %60 = mul i32 %59, 16807
  store i32 %60, i32* %random_seed, align 4, !tbaa !13
  %61 = lshr i32 %60, 9
  %62 = or i32 %61, 1065353216
  %63 = bitcast i32 %62 to float
  %64 = fadd fast float %63, -1.000000e+00
  %65 = fcmp olt float %64, %47
  br i1 %65, label %66, label %140

; <label>:66                                      ; preds = %32
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb, %class.vector3* dereferenceable(12) %normal)
  %67 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 0
  %68 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 1
  %69 = load i32* %random_seed, align 4, !tbaa !13
  %70 = mul i32 %69, 16807
  %71 = lshr i32 %70, 9
  %72 = or i32 %71, 1065353216
  %73 = bitcast i32 %72 to float
  %74 = fadd fast float %73, -1.000000e+00
  %75 = mul i32 %69, 282475249
  store i32 %75, i32* %random_seed, align 4, !tbaa !13
  %76 = lshr i32 %75, 9
  %77 = or i32 %76, 1065353216
  %78 = bitcast i32 %77 to float
  %79 = fadd fast float %78, -1.000000e+00
  %80 = bitcast %class.vector3* %dir.i5 to i8*
  call void @llvm.lifetime.start(i64 12, i8* %80)
  %81 = fmul fast float %74, 0x401921FB60000000
  %82 = fsub fast float 2.000000e+00, %78
  %83 = tail call float @_Z4sqrtf(float %82) #4
  %84 = tail call float @_Z4sqrtf(float %79) #4
  %85 = tail call float @_Z3cosf(float %81) #4
  %86 = fmul fast float %83, %85
  %87 = tail call float @_Z3sinf(float %81) #4
  %88 = fmul fast float %83, %87
  %89 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 0
  store float %86, float* %89, align 4, !tbaa !15
  %90 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 1
  store float %84, float* %90, align 4, !tbaa !18
  %91 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 2
  store float %88, float* %91, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir.i5, %class.vector3* dereferenceable(12) %normal, %class.vector3* dereferenceable(12) %67, %class.vector3* dereferenceable(12) %68) #3
  %92 = load float* %89, align 4, !tbaa !15
  %93 = load float* %90, align 4, !tbaa !18
  %94 = load float* %91, align 4, !tbaa !19
  call void @llvm.lifetime.end(i64 12, i8* %80)
  %95 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 0
  %96 = load float* %95, align 4, !tbaa !15
  %97 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 1
  %98 = load float* %97, align 4, !tbaa !18
  %99 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 2
  %100 = load float* %99, align 4, !tbaa !19
  %101 = fmul fast float %92, %34
  %102 = fmul fast float %93, %33
  %103 = fadd fast float %101, %102
  %104 = fmul fast float %94, %35
  %105 = fadd fast float %103, %104
  %106 = fcmp ogt float %105, 0.000000e+00
  %..i4 = select i1 %106, float %105, float 0.000000e+00
  %107 = fmul fast float %96, %..i4
  %108 = fmul fast float %98, %..i4
  %109 = fmul fast float %100, %..i4
  %110 = fcmp ogt float %107, 0.000000e+00
  %111 = fcmp ogt float %108, 0.000000e+00
  %or.cond = or i1 %110, %111
  %112 = fcmp ogt float %109, 0.000000e+00
  %or.cond16 = or i1 %or.cond, %112
  br i1 %or.cond16, label %113, label %271

; <label>:113                                     ; preds = %66
  %114 = fmul fast float %84, 0x3FD45F3060000000
  %115 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 0
  %116 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %117 = load float* %116, align 4, !tbaa !15
  store float %117, float* %115, align 4, !tbaa !15
  %118 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 1
  %119 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %120 = load float* %119, align 4, !tbaa !18
  store float %120, float* %118, align 4, !tbaa !18
  %121 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 2
  %122 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %123 = load float* %122, align 4, !tbaa !19
  store float %123, float* %121, align 4, !tbaa !19
  %124 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %92, float* %124, align 4, !tbaa !15
  %125 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %93, float* %125, align 4, !tbaa !18
  %126 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %94, float* %126, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer16compute_radianceILj1ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %1, %struct.ray* dereferenceable(24) %2, i32* dereferenceable(4) %random_seed, i1 zeroext false)
  %127 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %128 = load float* %127, align 4, !tbaa !15
  %129 = fmul fast float %107, %128
  %130 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %131 = load float* %130, align 4, !tbaa !18
  %132 = fmul fast float %108, %131
  %133 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %134 = load float* %133, align 4, !tbaa !19
  %135 = fmul fast float %109, %134
  %136 = fmul fast float %47, %114
  %137 = fdiv fast float %129, %136, !fpmath !14
  %138 = fdiv fast float %132, %136, !fpmath !14
  %139 = fdiv fast float %135, %136, !fpmath !14
  br label %271

; <label>:140                                     ; preds = %32
  %141 = fadd fast float %47, %58
  %142 = fcmp olt float %64, %141
  br i1 %142, label %143, label %271

; <label>:143                                     ; preds = %140
  %144 = fmul fast float %34, %8
  %145 = fmul fast float %33, %13
  %146 = fadd fast float %144, %145
  %147 = fmul fast float %35, %19
  %148 = fadd fast float %146, %147
  %149 = fmul fast float %148, 2.000000e+00
  %150 = fmul fast float %34, %149
  %151 = fmul fast float %33, %149
  %152 = fmul fast float %35, %149
  %153 = fsub fast float %8, %150
  %154 = fsub fast float %13, %151
  %155 = fsub fast float %19, %152
  %156 = fcmp oeq float %153, 0.000000e+00
  %157 = fcmp oeq float %154, 0.000000e+00
  %or.cond.i = and i1 %156, %157
  %158 = fcmp oeq float %155, 0.000000e+00
  %or.cond17 = and i1 %or.cond.i, %158
  br i1 %or.cond17, label %_ZNK7vector3IfE10normalizedEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %143
  %159 = fmul fast float %153, %153
  %160 = fmul fast float %154, %154
  %161 = fadd fast float %159, %160
  %162 = fmul fast float %155, %155
  %163 = fadd fast float %162, %161
  %164 = tail call float @_Z5rsqrtf(float %163) #4
  %165 = fmul fast float %153, %164
  %166 = fmul fast float %154, %164
  %167 = fmul fast float %155, %164
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i, %143
  %168 = phi float [ %165, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %153, %143 ]
  %169 = phi float [ %166, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %154, %143 ]
  %170 = phi float [ %167, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %155, %143 ]
  %171 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 0
  store float %168, float* %171, align 4
  %172 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 1
  store float %169, float* %172, align 4
  %173 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 2
  store float %170, float* %173, align 4
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb1, %class.vector3* dereferenceable(12) %rnormal)
  %174 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 0
  %175 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 1
  %176 = load i32* %random_seed, align 4, !tbaa !13
  %177 = mul i32 %176, 16807
  %178 = lshr i32 %177, 9
  %179 = or i32 %178, 1065353216
  %180 = bitcast i32 %179 to float
  %181 = fadd fast float %180, -1.000000e+00
  %182 = mul i32 %176, 282475249
  store i32 %182, i32* %random_seed, align 4, !tbaa !13
  %183 = lshr i32 %182, 9
  %184 = or i32 %183, 1065353216
  %185 = bitcast i32 %184 to float
  %186 = fadd fast float %185, -1.000000e+00
  %187 = getelementptr inbounds %struct.material* %mat, i64 0, i32 2
  %188 = load float* %187, align 4, !tbaa !30
  %189 = bitcast %class.vector3* %dir.i to i8*
  call void @llvm.lifetime.start(i64 12, i8* %189)
  %190 = fmul fast float %181, 0x401921FB60000000
  %191 = fadd fast float %188, 1.000000e+00
  %192 = fdiv fast float 1.000000e+00, %191, !fpmath !14
  %193 = tail call float @_Z3powff(float %186, float %192) #4
  %194 = fmul fast float %193, %193
  %195 = fsub fast float 1.000000e+00, %194
  %196 = tail call float @_Z4sqrtf(float %195) #4
  %197 = tail call float @_Z3cosf(float %190) #4
  %198 = fmul fast float %196, %197
  %199 = tail call float @_Z3sinf(float %190) #4
  %200 = fmul fast float %196, %199
  %201 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 0
  store float %198, float* %201, align 4, !tbaa !15
  %202 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 1
  store float %193, float* %202, align 4, !tbaa !18
  %203 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 2
  store float %200, float* %203, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir.i, %class.vector3* dereferenceable(12) %rnormal, %class.vector3* dereferenceable(12) %174, %class.vector3* dereferenceable(12) %175) #3
  %204 = load float* %201, align 4, !tbaa !15
  %205 = load float* %202, align 4, !tbaa !18
  %206 = load float* %203, align 4, !tbaa !19
  %207 = call float @_Z3powff(float %193, float %188) #4
  %208 = fmul fast float %191, %207
  %209 = fmul fast float %208, 0x3FC45F3060000000
  call void @llvm.lifetime.end(i64 12, i8* %189)
  %210 = fmul fast float %204, %34
  %211 = fmul fast float %205, %33
  %212 = fadd fast float %210, %211
  %213 = fmul fast float %206, %35
  %214 = fadd fast float %212, %213
  %215 = fcmp ugt float %214, 0.000000e+00
  br i1 %215, label %220, label %216

; <label>:216                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %217 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %217, align 4, !tbaa !15
  %218 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %218, align 4, !tbaa !18
  %219 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %219, align 4, !tbaa !19
  br label %278

; <label>:220                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %221 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 0
  %222 = load float* %221, align 4, !tbaa !15
  %223 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 1
  %224 = load float* %223, align 4, !tbaa !18
  %225 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 2
  %226 = load float* %225, align 4, !tbaa !19
  %227 = fmul fast float %204, %168
  %228 = fmul fast float %205, %169
  %229 = fadd fast float %227, %228
  %230 = fmul fast float %206, %170
  %231 = fadd fast float %229, %230
  %232 = fcmp ogt float %231, 0.000000e+00
  %..i3 = select i1 %232, float %231, float 0.000000e+00
  %233 = load float* %187, align 4, !tbaa !30
  %234 = call float @_Z3powff(float %..i3, float %233) #4
  %235 = fmul fast float %222, %234
  %236 = fmul fast float %224, %234
  %237 = fmul fast float %226, %234
  %238 = fcmp ogt float %214, 0.000000e+00
  %..i = select i1 %238, float %214, float 0.000000e+00
  %239 = fmul fast float %..i, %235
  %240 = fmul fast float %..i, %236
  %241 = fmul fast float %..i, %237
  %242 = fcmp ogt float %239, 0.000000e+00
  %243 = fcmp ogt float %240, 0.000000e+00
  %or.cond18 = or i1 %242, %243
  %244 = fcmp ogt float %241, 0.000000e+00
  %or.cond19 = or i1 %or.cond18, %244
  br i1 %or.cond19, label %245, label %271

; <label>:245                                     ; preds = %220
  %246 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 0
  %247 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %248 = load float* %247, align 4, !tbaa !15
  store float %248, float* %246, align 4, !tbaa !15
  %249 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 1
  %250 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %251 = load float* %250, align 4, !tbaa !18
  store float %251, float* %249, align 4, !tbaa !18
  %252 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 2
  %253 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %254 = load float* %253, align 4, !tbaa !19
  store float %254, float* %252, align 4, !tbaa !19
  %255 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %204, float* %255, align 4, !tbaa !15
  %256 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %205, float* %256, align 4, !tbaa !18
  %257 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %206, float* %257, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer16compute_radianceILj1ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %4, i32* dereferenceable(4) %random_seed, i1 zeroext false)
  %258 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %259 = load float* %258, align 4, !tbaa !15
  %260 = fmul fast float %239, %259
  %261 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %262 = load float* %261, align 4, !tbaa !18
  %263 = fmul fast float %240, %262
  %264 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %265 = load float* %264, align 4, !tbaa !19
  %266 = fmul fast float %241, %265
  %267 = fmul fast float %58, %209
  %268 = fdiv fast float %260, %267, !fpmath !14
  %269 = fdiv fast float %263, %267, !fpmath !14
  %270 = fdiv fast float %266, %267, !fpmath !14
  br label %271

; <label>:271                                     ; preds = %245, %220, %140, %113, %66
  %272 = phi float [ 0.000000e+00, %140 ], [ %270, %245 ], [ %139, %113 ], [ %109, %66 ], [ %241, %220 ]
  %273 = phi float [ 0.000000e+00, %140 ], [ %269, %245 ], [ %138, %113 ], [ %108, %66 ], [ %240, %220 ]
  %274 = phi float [ 0.000000e+00, %140 ], [ %268, %245 ], [ %137, %113 ], [ %107, %66 ], [ %239, %220 ]
  %275 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %274, float* %275, align 4, !tbaa !15
  %276 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %273, float* %276, align 4, !tbaa !18
  %277 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %272, float* %277, align 4, !tbaa !19
  br label %278

; <label>:278                                     ; preds = %271, %216
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* noalias nocapture sret %agg.result, %class.vector3* nocapture  dereferenceable(12) %normal) #2 align 2 {
  %tangent = alloca %class.vector3, align 4
  %1 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  %2 = load float* %1, align 4, !tbaa !15
  %3 = fcmp olt float %2, 0.000000e+00
  br i1 %3, label %4, label %_ZN10const_math3absIfvEET_S1_.exit

; <label>:4                                       ; preds = %0
  %5 = fsub fast float -0.000000e+00, %2
  br label %_ZN10const_math3absIfvEET_S1_.exit

_ZN10const_math3absIfvEET_S1_.exit:               ; preds = %4, %0
  %6 = phi float [ %5, %4 ], [ %2, %0 ]
  %7 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  %8 = load float* %7, align 4, !tbaa !18
  %9 = fcmp olt float %8, 0.000000e+00
  br i1 %9, label %10, label %_ZN10const_math3absIfvEET_S1_.exit1

; <label>:10                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit
  %11 = fsub fast float -0.000000e+00, %8
  br label %_ZN10const_math3absIfvEET_S1_.exit1

_ZN10const_math3absIfvEET_S1_.exit1:              ; preds = %10, %_ZN10const_math3absIfvEET_S1_.exit
  %12 = phi float [ %11, %10 ], [ %8, %_ZN10const_math3absIfvEET_S1_.exit ]
  %not. = fcmp ule float %6, %12
  %13 = zext i1 %not. to i64
  br i1 %3, label %14, label %_ZN10const_math3absIfvEET_S1_.exit6

; <label>:14                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit1
  %15 = fsub fast float -0.000000e+00, %2
  br label %_ZN10const_math3absIfvEET_S1_.exit6

_ZN10const_math3absIfvEET_S1_.exit6:              ; preds = %14, %_ZN10const_math3absIfvEET_S1_.exit1
  %16 = phi float [ %15, %14 ], [ %2, %_ZN10const_math3absIfvEET_S1_.exit1 ]
  br i1 %9, label %17, label %_ZN10const_math3absIfvEET_S1_.exit9

; <label>:17                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit6
  %18 = fsub fast float -0.000000e+00, %8
  br label %_ZN10const_math3absIfvEET_S1_.exit9

_ZN10const_math3absIfvEET_S1_.exit9:              ; preds = %17, %_ZN10const_math3absIfvEET_S1_.exit6
  %19 = phi float [ %18, %17 ], [ %8, %_ZN10const_math3absIfvEET_S1_.exit6 ]
  %20 = fcmp ogt float %16, %19
  %21 = zext i1 %20 to i64
  br i1 %3, label %22, label %_ZN10const_math3absIfvEET_S1_.exit8

; <label>:22                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit9
  %23 = fsub fast float -0.000000e+00, %2
  br label %_ZN10const_math3absIfvEET_S1_.exit8

_ZN10const_math3absIfvEET_S1_.exit8:              ; preds = %22, %_ZN10const_math3absIfvEET_S1_.exit9
  %24 = phi float [ %23, %22 ], [ %2, %_ZN10const_math3absIfvEET_S1_.exit9 ]
  br i1 %9, label %25, label %_ZN10const_math3absIfvEET_S1_.exit7

; <label>:25                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit8
  %26 = fsub fast float -0.000000e+00, %8
  br label %_ZN10const_math3absIfvEET_S1_.exit7

_ZN10const_math3absIfvEET_S1_.exit7:              ; preds = %25, %_ZN10const_math3absIfvEET_S1_.exit8
  %27 = phi float [ %26, %25 ], [ %8, %_ZN10const_math3absIfvEET_S1_.exit8 ]
  %28 = fcmp ogt float %24, %27
  %29 = select i1 %28, float -1.000000e+00, float 1.000000e+00
  %30 = getelementptr inbounds float* %1, i64 %13
  %31 = load float* %30, align 4, !tbaa !32
  %32 = fmul fast float %31, %31
  %33 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 2
  %34 = load float* %33, align 4, !tbaa !19
  %35 = fmul fast float %34, %34
  %36 = fadd fast float %32, %35
  %37 = fdiv fast float 1.000000e+00, %36, !fpmath !14
  %38 = getelementptr inbounds %class.vector3* %tangent, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %38, align 4, !tbaa !15
  %39 = getelementptr inbounds %class.vector3* %tangent, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %39, align 4, !tbaa !18
  %40 = getelementptr inbounds %class.vector3* %tangent, i64 0, i32 0, i32 0, i32 2
  %41 = fmul fast float %29, %34
  %42 = fmul fast float %37, %41
  %43 = getelementptr inbounds float* %38, i64 %13
  store float %42, float* %43, align 4, !tbaa !32
  %44 = getelementptr inbounds float* %38, i64 %21
  store float 0.000000e+00, float* %44, align 4, !tbaa !32
  %45 = fmul fast float %29, %31
  %46 = fmul fast float %37, %45
  %47 = fsub fast float -0.000000e+00, %46
  store float %47, float* %40, align 4, !tbaa !19
  %48 = load float* %38, align 4, !tbaa !15
  %49 = fcmp oeq float %48, 0.000000e+00
  %50 = load float* %39, align 4, !tbaa !18
  %51 = fcmp oeq float %50, 0.000000e+00
  %or.cond.i2 = and i1 %49, %51
  %52 = fcmp oeq float %46, -0.000000e+00
  %or.cond = and i1 %or.cond.i2, %52
  br i1 %or.cond, label %_ZN7vector3IfE9normalizeEv.exit5, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i4

_ZNK7vector3IfE7is_nullEv.exit.thread.i4:         ; preds = %_ZN10const_math3absIfvEET_S1_.exit7
  %53 = fmul fast float %48, %48
  %54 = fmul fast float %50, %50
  %55 = fadd fast float %53, %54
  %56 = fmul fast float %46, %46
  %57 = fadd fast float %55, %56
  %58 = tail call float @_Z5rsqrtf(float %57) #4
  %59 = fmul fast float %48, %58
  store float %59, float* %38, align 4, !tbaa !15
  %60 = fmul fast float %50, %58
  store float %60, float* %39, align 4, !tbaa !18
  %61 = fmul fast float %58, %47
  store float %61, float* %40, align 4, !tbaa !19
  br label %_ZN7vector3IfE9normalizeEv.exit5

_ZN7vector3IfE9normalizeEv.exit5:                 ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i4, %_ZN10const_math3absIfvEET_S1_.exit7
  %62 = phi float [ %48, %_ZN10const_math3absIfvEET_S1_.exit7 ], [ %59, %_ZNK7vector3IfE7is_nullEv.exit.thread.i4 ]
  %63 = phi float [ %50, %_ZN10const_math3absIfvEET_S1_.exit7 ], [ %60, %_ZNK7vector3IfE7is_nullEv.exit.thread.i4 ]
  %64 = phi float [ %47, %_ZN10const_math3absIfvEET_S1_.exit7 ], [ %61, %_ZNK7vector3IfE7is_nullEv.exit.thread.i4 ]
  %65 = fmul fast float %8, %64
  %66 = fmul fast float %34, %63
  %67 = fsub fast float %65, %66
  %68 = fmul fast float %34, %62
  %69 = fmul fast float %64, %2
  %70 = fsub fast float %68, %69
  %71 = fmul fast float %63, %2
  %72 = fmul fast float %8, %62
  %73 = fsub fast float %71, %72
  %74 = fcmp oeq float %67, 0.000000e+00
  %75 = fcmp oeq float %70, 0.000000e+00
  %or.cond.i = and i1 %74, %75
  %76 = fcmp oeq float %73, 0.000000e+00
  %or.cond10 = and i1 %or.cond.i, %76
  br i1 %or.cond10, label %_ZN7vector3IfE9normalizeEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %_ZN7vector3IfE9normalizeEv.exit5
  %77 = fmul fast float %67, %67
  %78 = fmul fast float %70, %70
  %79 = fadd fast float %77, %78
  %80 = fmul fast float %73, %73
  %81 = fadd fast float %80, %79
  %82 = tail call float @_Z5rsqrtf(float %81) #4
  %83 = fmul fast float %67, %82
  %84 = fmul fast float %70, %82
  %85 = fmul fast float %73, %82
  br label %_ZN7vector3IfE9normalizeEv.exit

_ZN7vector3IfE9normalizeEv.exit:                  ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i, %_ZN7vector3IfE9normalizeEv.exit5
  %86 = phi float [ %85, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %73, %_ZN7vector3IfE9normalizeEv.exit5 ]
  %87 = phi float [ %84, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %70, %_ZN7vector3IfE9normalizeEv.exit5 ]
  %88 = phi float [ %83, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %67, %_ZN7vector3IfE9normalizeEv.exit5 ]
  %89 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 0
  store float %62, float* %89, align 4, !tbaa !15
  %90 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 1
  store float %63, float* %90, align 4, !tbaa !18
  %91 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 2
  store float %64, float* %91, align 4, !tbaa !19
  %92 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %88, float* %92, align 4, !tbaa !15
  %93 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %87, float* %93, align 4, !tbaa !18
  %94 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %86, float* %94, align 4, !tbaa !19
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer16compute_radianceILj1ELi0EEE7vector3IfERK3rayRjb(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r, i32* nocapture dereferenceable(4) %random_seed, i1 zeroext %sample_emission) #2 align 2 {
  %p = alloca %"struct.simple_intersector::intersection", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %class.vector3, align 4
  %3 = alloca %class.vector3, align 4
  call void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* sret %p, %struct.ray* dereferenceable(24) %r)
  %4 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 3
  %5 = load i32* %4, align 4, !tbaa !20
  %6 = icmp ugt i32 %5, 7
  br i1 %6, label %7, label %11

; <label>:7                                       ; preds = %0
  %8 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %8, align 4, !tbaa !15
  %9 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %9, align 4, !tbaa !18
  %10 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %10, align 4, !tbaa !19
  br label %58

; <label>:11                                      ; preds = %0
  %12 = zext i32 %5 to i64
  %13 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12
  br i1 %sample_emission, label %14, label %21

; <label>:14                                      ; preds = %11
  %15 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 0
  %16 = load float* %15, align 4, !tbaa !15
  %17 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 1
  %18 = load float* %17, align 4, !tbaa !18
  %19 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 2
  %20 = load float* %19, align 4, !tbaa !19
  br label %21

; <label>:21                                      ; preds = %14, %11
  %22 = phi float [ %20, %14 ], [ 0.000000e+00, %11 ]
  %23 = phi float [ %18, %14 ], [ 0.000000e+00, %11 ]
  %24 = phi float [ %16, %14 ], [ 0.000000e+00, %11 ]
  %25 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %26 = load float* %25, align 4, !tbaa !15
  %27 = fsub fast float -0.000000e+00, %26
  %28 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %29 = load float* %28, align 4, !tbaa !18
  %30 = fsub fast float -0.000000e+00, %29
  %31 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %32 = load float* %31, align 4, !tbaa !19
  %33 = fsub fast float -0.000000e+00, %32
  %34 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 0
  store float %27, float* %34, align 4, !tbaa !15
  %35 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 1
  store float %30, float* %35, align 4, !tbaa !18
  %36 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 2
  store float %33, float* %36, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer27compute_direct_illuminationERK7vector3IfERKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %1, %class.vector3* dereferenceable(12) %2, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %37 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !15
  %39 = fadd fast float %24, %38
  %40 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !18
  %42 = fadd fast float %23, %41
  %43 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %44 = load float* %43, align 4, !tbaa !19
  %45 = fadd fast float %22, %44
  call void @_ZN18simple_path_tracer29compute_indirect_illuminationILj1EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %r, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %46 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %47 = load float* %46, align 4, !tbaa !15
  %48 = fadd fast float %39, %47
  %49 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %50 = load float* %49, align 4, !tbaa !18
  %51 = fadd fast float %42, %50
  %52 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %53 = load float* %52, align 4, !tbaa !19
  %54 = fadd fast float %45, %53
  %55 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %48, float* %55, align 4, !tbaa !15
  %56 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %51, float* %56, align 4, !tbaa !18
  %57 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %54, float* %57, align 4, !tbaa !19
  br label %58

; <label>:58                                      ; preds = %21, %7
  ret void
}

; Function Attrs: nounwind readnone
declare float @_Z3powff(float, float) #1

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir, %class.vector3* nocapture  dereferenceable(12) %up, %class.vector3* nocapture  dereferenceable(12) %left, %class.vector3* nocapture  dereferenceable(12) %forward) #2 align 2 {
  %1 = getelementptr inbounds %class.vector3* %dir, i64 0, i32 0, i32 0, i32 0
  %2 = load float* %1, align 4, !tbaa !15
  %3 = fcmp oeq float %2, 0.000000e+00
  %4 = getelementptr inbounds %class.vector3* %dir, i64 0, i32 0, i32 0, i32 1
  %5 = load float* %4, align 4, !tbaa !18
  %6 = fcmp oeq float %5, 0.000000e+00
  %or.cond.i = and i1 %3, %6
  %7 = getelementptr inbounds %class.vector3* %dir, i64 0, i32 0, i32 0, i32 2
  %8 = load float* %7, align 4, !tbaa !19
  %9 = fcmp oeq float %8, 0.000000e+00
  %or.cond5 = and i1 %or.cond.i, %9
  br i1 %or.cond5, label %_ZN7vector3IfE9normalizeEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %0
  %10 = fmul fast float %2, %2
  %11 = fmul fast float %5, %5
  %12 = fadd fast float %10, %11
  %13 = getelementptr inbounds %class.vector3* %dir, i64 0, i32 0, i32 0, i32 2
  %14 = fmul fast float %8, %8
  %15 = fadd fast float %12, %14
  %16 = tail call float @_Z5rsqrtf(float %15) #4
  %17 = fmul fast float %2, %16
  store float %17, float* %1, align 4, !tbaa !15
  %18 = fmul fast float %5, %16
  store float %18, float* %4, align 4, !tbaa !18
  %19 = fmul fast float %8, %16
  store float %19, float* %13, align 4, !tbaa !19
  br label %_ZN7vector3IfE9normalizeEv.exit

_ZN7vector3IfE9normalizeEv.exit:                  ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i, %0
  %20 = phi float [ %19, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %8, %0 ]
  %21 = phi float [ %18, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %5, %0 ]
  %22 = phi float [ %17, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %2, %0 ]
  %23 = getelementptr inbounds %class.vector3* %left, i64 0, i32 0, i32 0, i32 0
  %24 = load float* %23, align 4, !tbaa !32
  %25 = fmul fast float %22, %24
  %26 = getelementptr inbounds %class.vector3* %up, i64 0, i32 0, i32 0, i32 0
  %27 = load float* %26, align 4, !tbaa !32
  %28 = fmul fast float %21, %27
  %29 = fadd fast float %25, %28
  %30 = getelementptr inbounds %class.vector3* %dir, i64 0, i32 0, i32 0, i32 2
  %31 = getelementptr inbounds %class.vector3* %forward, i64 0, i32 0, i32 0, i32 0
  %32 = load float* %31, align 4, !tbaa !32
  %33 = fmul fast float %20, %32
  %34 = fadd fast float %29, %33
  %35 = getelementptr inbounds %class.vector3* %left, i64 0, i32 0, i32 0, i32 1
  %36 = load float* %35, align 4, !tbaa !32
  %37 = fmul fast float %22, %36
  %38 = getelementptr inbounds %class.vector3* %up, i64 0, i32 0, i32 0, i32 1
  %39 = load float* %38, align 4, !tbaa !32
  %40 = fmul fast float %21, %39
  %41 = fadd fast float %37, %40
  %42 = getelementptr inbounds %class.vector3* %forward, i64 0, i32 0, i32 0, i32 1
  %43 = load float* %42, align 4, !tbaa !32
  %44 = fmul fast float %20, %43
  %45 = fadd fast float %41, %44
  %46 = getelementptr inbounds %class.vector3* %left, i64 0, i32 0, i32 0, i32 2
  %47 = load float* %46, align 4, !tbaa !32
  %48 = fmul fast float %22, %47
  %49 = getelementptr inbounds %class.vector3* %up, i64 0, i32 0, i32 0, i32 2
  %50 = load float* %49, align 4, !tbaa !32
  %51 = fmul fast float %21, %50
  %52 = fadd fast float %48, %51
  %53 = getelementptr inbounds %class.vector3* %forward, i64 0, i32 0, i32 0, i32 2
  %54 = load float* %53, align 4, !tbaa !32
  %55 = fmul fast float %20, %54
  %56 = fadd fast float %52, %55
  store float %34, float* %1, align 4, !tbaa !15
  store float %45, float* %4, align 4, !tbaa !18
  store float %56, float* %30, align 4, !tbaa !19
  %57 = fcmp oeq float %34, 0.000000e+00
  %58 = fcmp oeq float %45, 0.000000e+00
  %or.cond.i1 = and i1 %57, %58
  %59 = fcmp oeq float %56, 0.000000e+00
  %or.cond = and i1 %or.cond.i1, %59
  br i1 %or.cond, label %_ZN7vector3IfE9normalizeEv.exit4, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i3

_ZNK7vector3IfE7is_nullEv.exit.thread.i3:         ; preds = %_ZN7vector3IfE9normalizeEv.exit
  %60 = fmul fast float %34, %34
  %61 = fmul fast float %45, %45
  %62 = fadd fast float %60, %61
  %63 = fmul fast float %56, %56
  %64 = fadd fast float %62, %63
  %65 = tail call float @_Z5rsqrtf(float %64) #4
  %66 = fmul fast float %34, %65
  store float %66, float* %1, align 4, !tbaa !15
  %67 = fmul fast float %45, %65
  store float %67, float* %4, align 4, !tbaa !18
  %68 = fmul fast float %56, %65
  store float %68, float* %30, align 4, !tbaa !19
  br label %_ZN7vector3IfE9normalizeEv.exit4

_ZN7vector3IfE9normalizeEv.exit4:                 ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i3, %_ZN7vector3IfE9normalizeEv.exit
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer29compute_indirect_illuminationILj1EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r, %"struct.simple_intersector::intersection"* nocapture  dereferenceable(32) %p, %struct.material* nocapture  dereferenceable(64) %mat, i32* nocapture dereferenceable(4) %random_seed) #2 align 2 {
  %dir.i5 = alloca %class.vector3, align 4
  %dir.i = alloca %class.vector3, align 4
  %normal = alloca %class.vector3, align 4
  %tb = alloca %"struct.std::__1::pair", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %struct.ray, align 4
  %rnormal = alloca %class.vector3, align 4
  %tb1 = alloca %"struct.std::__1::pair", align 4
  %3 = alloca %class.vector3, align 4
  %4 = alloca %struct.ray, align 4
  %5 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 0
  %6 = load float* %5, align 4, !tbaa !15
  %7 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %8 = load float* %7, align 4, !tbaa !15
  %9 = fmul fast float %6, %8
  %10 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 1
  %11 = load float* %10, align 4, !tbaa !18
  %12 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %13 = load float* %12, align 4, !tbaa !18
  %14 = fmul fast float %11, %13
  %15 = fadd fast float %9, %14
  %16 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 2
  %17 = load float* %16, align 4, !tbaa !19
  %18 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %19 = load float* %18, align 4, !tbaa !19
  %20 = fmul fast float %17, %19
  %21 = fadd fast float %15, %20
  %22 = fcmp ogt float %21, 0.000000e+00
  br i1 %22, label %23, label %29

; <label>:23                                      ; preds = %0
  %24 = fsub fast float -0.000000e+00, %6
  %25 = fsub fast float -0.000000e+00, %11
  %26 = fsub fast float -0.000000e+00, %17
  %27 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %24, float* %27, align 4, !tbaa !15
  %28 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %25, float* %28, align 4, !tbaa !18
  br label %32

; <label>:29                                      ; preds = %0
  %30 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %6, float* %30, align 4, !tbaa !15
  %31 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %11, float* %31, align 4, !tbaa !18
  br label %32

; <label>:32                                      ; preds = %29, %23
  %33 = phi float [ %11, %29 ], [ %25, %23 ]
  %34 = phi float [ %6, %29 ], [ %24, %23 ]
  %35 = phi float [ %17, %29 ], [ %26, %23 ]
  %36 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 2
  store float %35, float* %36, align 4
  %37 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !15
  %39 = fmul fast float %38, 0x3FCC6A7F00000000
  %40 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !18
  %42 = fmul fast float %41, 0x3FE69D4960000000
  %43 = fadd fast float %39, %42
  %44 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 2
  %45 = load float* %44, align 4, !tbaa !19
  %46 = fmul fast float %45, 0x3FB240B780000000
  %47 = fadd fast float %43, %46
  %48 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 0
  %49 = load float* %48, align 4, !tbaa !15
  %50 = fmul fast float %49, 0x3FCC6A7F00000000
  %51 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 1
  %52 = load float* %51, align 4, !tbaa !18
  %53 = fmul fast float %52, 0x3FE69D4960000000
  %54 = fadd fast float %50, %53
  %55 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 2
  %56 = load float* %55, align 4, !tbaa !19
  %57 = fmul fast float %56, 0x3FB240B780000000
  %58 = fadd fast float %54, %57
  %59 = load i32* %random_seed, align 4, !tbaa !13
  %60 = mul i32 %59, 16807
  store i32 %60, i32* %random_seed, align 4, !tbaa !13
  %61 = lshr i32 %60, 9
  %62 = or i32 %61, 1065353216
  %63 = bitcast i32 %62 to float
  %64 = fadd fast float %63, -1.000000e+00
  %65 = fcmp olt float %64, %47
  br i1 %65, label %66, label %140

; <label>:66                                      ; preds = %32
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb, %class.vector3* dereferenceable(12) %normal)
  %67 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 0
  %68 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 1
  %69 = load i32* %random_seed, align 4, !tbaa !13
  %70 = mul i32 %69, 16807
  %71 = lshr i32 %70, 9
  %72 = or i32 %71, 1065353216
  %73 = bitcast i32 %72 to float
  %74 = fadd fast float %73, -1.000000e+00
  %75 = mul i32 %69, 282475249
  store i32 %75, i32* %random_seed, align 4, !tbaa !13
  %76 = lshr i32 %75, 9
  %77 = or i32 %76, 1065353216
  %78 = bitcast i32 %77 to float
  %79 = fadd fast float %78, -1.000000e+00
  %80 = bitcast %class.vector3* %dir.i5 to i8*
  call void @llvm.lifetime.start(i64 12, i8* %80)
  %81 = fmul fast float %74, 0x401921FB60000000
  %82 = fsub fast float 2.000000e+00, %78
  %83 = tail call float @_Z4sqrtf(float %82) #4
  %84 = tail call float @_Z4sqrtf(float %79) #4
  %85 = tail call float @_Z3cosf(float %81) #4
  %86 = fmul fast float %83, %85
  %87 = tail call float @_Z3sinf(float %81) #4
  %88 = fmul fast float %83, %87
  %89 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 0
  store float %86, float* %89, align 4, !tbaa !15
  %90 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 1
  store float %84, float* %90, align 4, !tbaa !18
  %91 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 2
  store float %88, float* %91, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir.i5, %class.vector3* dereferenceable(12) %normal, %class.vector3* dereferenceable(12) %67, %class.vector3* dereferenceable(12) %68) #3
  %92 = load float* %89, align 4, !tbaa !15
  %93 = load float* %90, align 4, !tbaa !18
  %94 = load float* %91, align 4, !tbaa !19
  call void @llvm.lifetime.end(i64 12, i8* %80)
  %95 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 0
  %96 = load float* %95, align 4, !tbaa !15
  %97 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 1
  %98 = load float* %97, align 4, !tbaa !18
  %99 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 2
  %100 = load float* %99, align 4, !tbaa !19
  %101 = fmul fast float %92, %34
  %102 = fmul fast float %93, %33
  %103 = fadd fast float %101, %102
  %104 = fmul fast float %94, %35
  %105 = fadd fast float %103, %104
  %106 = fcmp ogt float %105, 0.000000e+00
  %..i4 = select i1 %106, float %105, float 0.000000e+00
  %107 = fmul fast float %96, %..i4
  %108 = fmul fast float %98, %..i4
  %109 = fmul fast float %100, %..i4
  %110 = fcmp ogt float %107, 0.000000e+00
  %111 = fcmp ogt float %108, 0.000000e+00
  %or.cond = or i1 %110, %111
  %112 = fcmp ogt float %109, 0.000000e+00
  %or.cond16 = or i1 %or.cond, %112
  br i1 %or.cond16, label %113, label %271

; <label>:113                                     ; preds = %66
  %114 = fmul fast float %84, 0x3FD45F3060000000
  %115 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 0
  %116 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %117 = load float* %116, align 4, !tbaa !15
  store float %117, float* %115, align 4, !tbaa !15
  %118 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 1
  %119 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %120 = load float* %119, align 4, !tbaa !18
  store float %120, float* %118, align 4, !tbaa !18
  %121 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 2
  %122 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %123 = load float* %122, align 4, !tbaa !19
  store float %123, float* %121, align 4, !tbaa !19
  %124 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %92, float* %124, align 4, !tbaa !15
  %125 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %93, float* %125, align 4, !tbaa !18
  %126 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %94, float* %126, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer16compute_radianceILj2ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %1, %struct.ray* dereferenceable(24) %2, i32* dereferenceable(4) %random_seed, i1 zeroext false)
  %127 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %128 = load float* %127, align 4, !tbaa !15
  %129 = fmul fast float %107, %128
  %130 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %131 = load float* %130, align 4, !tbaa !18
  %132 = fmul fast float %108, %131
  %133 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %134 = load float* %133, align 4, !tbaa !19
  %135 = fmul fast float %109, %134
  %136 = fmul fast float %47, %114
  %137 = fdiv fast float %129, %136, !fpmath !14
  %138 = fdiv fast float %132, %136, !fpmath !14
  %139 = fdiv fast float %135, %136, !fpmath !14
  br label %271

; <label>:140                                     ; preds = %32
  %141 = fadd fast float %47, %58
  %142 = fcmp olt float %64, %141
  br i1 %142, label %143, label %271

; <label>:143                                     ; preds = %140
  %144 = fmul fast float %34, %8
  %145 = fmul fast float %33, %13
  %146 = fadd fast float %144, %145
  %147 = fmul fast float %35, %19
  %148 = fadd fast float %146, %147
  %149 = fmul fast float %148, 2.000000e+00
  %150 = fmul fast float %34, %149
  %151 = fmul fast float %33, %149
  %152 = fmul fast float %35, %149
  %153 = fsub fast float %8, %150
  %154 = fsub fast float %13, %151
  %155 = fsub fast float %19, %152
  %156 = fcmp oeq float %153, 0.000000e+00
  %157 = fcmp oeq float %154, 0.000000e+00
  %or.cond.i = and i1 %156, %157
  %158 = fcmp oeq float %155, 0.000000e+00
  %or.cond17 = and i1 %or.cond.i, %158
  br i1 %or.cond17, label %_ZNK7vector3IfE10normalizedEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %143
  %159 = fmul fast float %153, %153
  %160 = fmul fast float %154, %154
  %161 = fadd fast float %159, %160
  %162 = fmul fast float %155, %155
  %163 = fadd fast float %162, %161
  %164 = tail call float @_Z5rsqrtf(float %163) #4
  %165 = fmul fast float %153, %164
  %166 = fmul fast float %154, %164
  %167 = fmul fast float %155, %164
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i, %143
  %168 = phi float [ %165, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %153, %143 ]
  %169 = phi float [ %166, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %154, %143 ]
  %170 = phi float [ %167, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %155, %143 ]
  %171 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 0
  store float %168, float* %171, align 4
  %172 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 1
  store float %169, float* %172, align 4
  %173 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 2
  store float %170, float* %173, align 4
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb1, %class.vector3* dereferenceable(12) %rnormal)
  %174 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 0
  %175 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 1
  %176 = load i32* %random_seed, align 4, !tbaa !13
  %177 = mul i32 %176, 16807
  %178 = lshr i32 %177, 9
  %179 = or i32 %178, 1065353216
  %180 = bitcast i32 %179 to float
  %181 = fadd fast float %180, -1.000000e+00
  %182 = mul i32 %176, 282475249
  store i32 %182, i32* %random_seed, align 4, !tbaa !13
  %183 = lshr i32 %182, 9
  %184 = or i32 %183, 1065353216
  %185 = bitcast i32 %184 to float
  %186 = fadd fast float %185, -1.000000e+00
  %187 = getelementptr inbounds %struct.material* %mat, i64 0, i32 2
  %188 = load float* %187, align 4, !tbaa !30
  %189 = bitcast %class.vector3* %dir.i to i8*
  call void @llvm.lifetime.start(i64 12, i8* %189)
  %190 = fmul fast float %181, 0x401921FB60000000
  %191 = fadd fast float %188, 1.000000e+00
  %192 = fdiv fast float 1.000000e+00, %191, !fpmath !14
  %193 = tail call float @_Z3powff(float %186, float %192) #4
  %194 = fmul fast float %193, %193
  %195 = fsub fast float 1.000000e+00, %194
  %196 = tail call float @_Z4sqrtf(float %195) #4
  %197 = tail call float @_Z3cosf(float %190) #4
  %198 = fmul fast float %196, %197
  %199 = tail call float @_Z3sinf(float %190) #4
  %200 = fmul fast float %196, %199
  %201 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 0
  store float %198, float* %201, align 4, !tbaa !15
  %202 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 1
  store float %193, float* %202, align 4, !tbaa !18
  %203 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 2
  store float %200, float* %203, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir.i, %class.vector3* dereferenceable(12) %rnormal, %class.vector3* dereferenceable(12) %174, %class.vector3* dereferenceable(12) %175) #3
  %204 = load float* %201, align 4, !tbaa !15
  %205 = load float* %202, align 4, !tbaa !18
  %206 = load float* %203, align 4, !tbaa !19
  %207 = call float @_Z3powff(float %193, float %188) #4
  %208 = fmul fast float %191, %207
  %209 = fmul fast float %208, 0x3FC45F3060000000
  call void @llvm.lifetime.end(i64 12, i8* %189)
  %210 = fmul fast float %204, %34
  %211 = fmul fast float %205, %33
  %212 = fadd fast float %210, %211
  %213 = fmul fast float %206, %35
  %214 = fadd fast float %212, %213
  %215 = fcmp ugt float %214, 0.000000e+00
  br i1 %215, label %220, label %216

; <label>:216                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %217 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %217, align 4, !tbaa !15
  %218 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %218, align 4, !tbaa !18
  %219 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %219, align 4, !tbaa !19
  br label %278

; <label>:220                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %221 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 0
  %222 = load float* %221, align 4, !tbaa !15
  %223 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 1
  %224 = load float* %223, align 4, !tbaa !18
  %225 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 2
  %226 = load float* %225, align 4, !tbaa !19
  %227 = fmul fast float %204, %168
  %228 = fmul fast float %205, %169
  %229 = fadd fast float %227, %228
  %230 = fmul fast float %206, %170
  %231 = fadd fast float %229, %230
  %232 = fcmp ogt float %231, 0.000000e+00
  %..i3 = select i1 %232, float %231, float 0.000000e+00
  %233 = load float* %187, align 4, !tbaa !30
  %234 = call float @_Z3powff(float %..i3, float %233) #4
  %235 = fmul fast float %222, %234
  %236 = fmul fast float %224, %234
  %237 = fmul fast float %226, %234
  %238 = fcmp ogt float %214, 0.000000e+00
  %..i = select i1 %238, float %214, float 0.000000e+00
  %239 = fmul fast float %..i, %235
  %240 = fmul fast float %..i, %236
  %241 = fmul fast float %..i, %237
  %242 = fcmp ogt float %239, 0.000000e+00
  %243 = fcmp ogt float %240, 0.000000e+00
  %or.cond18 = or i1 %242, %243
  %244 = fcmp ogt float %241, 0.000000e+00
  %or.cond19 = or i1 %or.cond18, %244
  br i1 %or.cond19, label %245, label %271

; <label>:245                                     ; preds = %220
  %246 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 0
  %247 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %248 = load float* %247, align 4, !tbaa !15
  store float %248, float* %246, align 4, !tbaa !15
  %249 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 1
  %250 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %251 = load float* %250, align 4, !tbaa !18
  store float %251, float* %249, align 4, !tbaa !18
  %252 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 2
  %253 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %254 = load float* %253, align 4, !tbaa !19
  store float %254, float* %252, align 4, !tbaa !19
  %255 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %204, float* %255, align 4, !tbaa !15
  %256 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %205, float* %256, align 4, !tbaa !18
  %257 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %206, float* %257, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer16compute_radianceILj2ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %4, i32* dereferenceable(4) %random_seed, i1 zeroext false)
  %258 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %259 = load float* %258, align 4, !tbaa !15
  %260 = fmul fast float %239, %259
  %261 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %262 = load float* %261, align 4, !tbaa !18
  %263 = fmul fast float %240, %262
  %264 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %265 = load float* %264, align 4, !tbaa !19
  %266 = fmul fast float %241, %265
  %267 = fmul fast float %58, %209
  %268 = fdiv fast float %260, %267, !fpmath !14
  %269 = fdiv fast float %263, %267, !fpmath !14
  %270 = fdiv fast float %266, %267, !fpmath !14
  br label %271

; <label>:271                                     ; preds = %245, %220, %140, %113, %66
  %272 = phi float [ 0.000000e+00, %140 ], [ %270, %245 ], [ %139, %113 ], [ %109, %66 ], [ %241, %220 ]
  %273 = phi float [ 0.000000e+00, %140 ], [ %269, %245 ], [ %138, %113 ], [ %108, %66 ], [ %240, %220 ]
  %274 = phi float [ 0.000000e+00, %140 ], [ %268, %245 ], [ %137, %113 ], [ %107, %66 ], [ %239, %220 ]
  %275 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %274, float* %275, align 4, !tbaa !15
  %276 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %273, float* %276, align 4, !tbaa !18
  %277 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %272, float* %277, align 4, !tbaa !19
  br label %278

; <label>:278                                     ; preds = %271, %216
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer16compute_radianceILj2ELi0EEE7vector3IfERK3rayRjb(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r, i32* nocapture dereferenceable(4) %random_seed, i1 zeroext %sample_emission) #2 align 2 {
  %p = alloca %"struct.simple_intersector::intersection", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %class.vector3, align 4
  %3 = alloca %class.vector3, align 4
  call void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* sret %p, %struct.ray* dereferenceable(24) %r)
  %4 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 3
  %5 = load i32* %4, align 4, !tbaa !20
  %6 = icmp ugt i32 %5, 7
  br i1 %6, label %7, label %11

; <label>:7                                       ; preds = %0
  %8 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %8, align 4, !tbaa !15
  %9 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %9, align 4, !tbaa !18
  %10 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %10, align 4, !tbaa !19
  br label %58

; <label>:11                                      ; preds = %0
  %12 = zext i32 %5 to i64
  %13 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12
  br i1 %sample_emission, label %14, label %21

; <label>:14                                      ; preds = %11
  %15 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 0
  %16 = load float* %15, align 4, !tbaa !15
  %17 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 1
  %18 = load float* %17, align 4, !tbaa !18
  %19 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 2
  %20 = load float* %19, align 4, !tbaa !19
  br label %21

; <label>:21                                      ; preds = %14, %11
  %22 = phi float [ %20, %14 ], [ 0.000000e+00, %11 ]
  %23 = phi float [ %18, %14 ], [ 0.000000e+00, %11 ]
  %24 = phi float [ %16, %14 ], [ 0.000000e+00, %11 ]
  %25 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %26 = load float* %25, align 4, !tbaa !15
  %27 = fsub fast float -0.000000e+00, %26
  %28 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %29 = load float* %28, align 4, !tbaa !18
  %30 = fsub fast float -0.000000e+00, %29
  %31 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %32 = load float* %31, align 4, !tbaa !19
  %33 = fsub fast float -0.000000e+00, %32
  %34 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 0
  store float %27, float* %34, align 4, !tbaa !15
  %35 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 1
  store float %30, float* %35, align 4, !tbaa !18
  %36 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 2
  store float %33, float* %36, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer27compute_direct_illuminationERK7vector3IfERKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %1, %class.vector3* dereferenceable(12) %2, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %37 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !15
  %39 = fadd fast float %24, %38
  %40 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !18
  %42 = fadd fast float %23, %41
  %43 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %44 = load float* %43, align 4, !tbaa !19
  %45 = fadd fast float %22, %44
  call void @_ZN18simple_path_tracer29compute_indirect_illuminationILj2EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %r, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %46 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %47 = load float* %46, align 4, !tbaa !15
  %48 = fadd fast float %39, %47
  %49 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %50 = load float* %49, align 4, !tbaa !18
  %51 = fadd fast float %42, %50
  %52 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %53 = load float* %52, align 4, !tbaa !19
  %54 = fadd fast float %45, %53
  %55 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %48, float* %55, align 4, !tbaa !15
  %56 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %51, float* %56, align 4, !tbaa !18
  %57 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %54, float* %57, align 4, !tbaa !19
  br label %58

; <label>:58                                      ; preds = %21, %7
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer29compute_indirect_illuminationILj2EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r, %"struct.simple_intersector::intersection"* nocapture  dereferenceable(32) %p, %struct.material* nocapture  dereferenceable(64) %mat, i32* nocapture dereferenceable(4) %random_seed) #2 align 2 {
  %dir.i5 = alloca %class.vector3, align 4
  %dir.i = alloca %class.vector3, align 4
  %normal = alloca %class.vector3, align 4
  %tb = alloca %"struct.std::__1::pair", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %struct.ray, align 4
  %rnormal = alloca %class.vector3, align 4
  %tb1 = alloca %"struct.std::__1::pair", align 4
  %3 = alloca %class.vector3, align 4
  %4 = alloca %struct.ray, align 4
  %5 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 0
  %6 = load float* %5, align 4, !tbaa !15
  %7 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %8 = load float* %7, align 4, !tbaa !15
  %9 = fmul fast float %6, %8
  %10 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 1
  %11 = load float* %10, align 4, !tbaa !18
  %12 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %13 = load float* %12, align 4, !tbaa !18
  %14 = fmul fast float %11, %13
  %15 = fadd fast float %9, %14
  %16 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 2
  %17 = load float* %16, align 4, !tbaa !19
  %18 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %19 = load float* %18, align 4, !tbaa !19
  %20 = fmul fast float %17, %19
  %21 = fadd fast float %15, %20
  %22 = fcmp ogt float %21, 0.000000e+00
  br i1 %22, label %23, label %29

; <label>:23                                      ; preds = %0
  %24 = fsub fast float -0.000000e+00, %6
  %25 = fsub fast float -0.000000e+00, %11
  %26 = fsub fast float -0.000000e+00, %17
  %27 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %24, float* %27, align 4, !tbaa !15
  %28 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %25, float* %28, align 4, !tbaa !18
  br label %32

; <label>:29                                      ; preds = %0
  %30 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %6, float* %30, align 4, !tbaa !15
  %31 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %11, float* %31, align 4, !tbaa !18
  br label %32

; <label>:32                                      ; preds = %29, %23
  %33 = phi float [ %11, %29 ], [ %25, %23 ]
  %34 = phi float [ %6, %29 ], [ %24, %23 ]
  %35 = phi float [ %17, %29 ], [ %26, %23 ]
  %36 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 2
  store float %35, float* %36, align 4
  %37 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !15
  %39 = fmul fast float %38, 0x3FCC6A7F00000000
  %40 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !18
  %42 = fmul fast float %41, 0x3FE69D4960000000
  %43 = fadd fast float %39, %42
  %44 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 2
  %45 = load float* %44, align 4, !tbaa !19
  %46 = fmul fast float %45, 0x3FB240B780000000
  %47 = fadd fast float %43, %46
  %48 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 0
  %49 = load float* %48, align 4, !tbaa !15
  %50 = fmul fast float %49, 0x3FCC6A7F00000000
  %51 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 1
  %52 = load float* %51, align 4, !tbaa !18
  %53 = fmul fast float %52, 0x3FE69D4960000000
  %54 = fadd fast float %50, %53
  %55 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 2
  %56 = load float* %55, align 4, !tbaa !19
  %57 = fmul fast float %56, 0x3FB240B780000000
  %58 = fadd fast float %54, %57
  %59 = load i32* %random_seed, align 4, !tbaa !13
  %60 = mul i32 %59, 16807
  store i32 %60, i32* %random_seed, align 4, !tbaa !13
  %61 = lshr i32 %60, 9
  %62 = or i32 %61, 1065353216
  %63 = bitcast i32 %62 to float
  %64 = fadd fast float %63, -1.000000e+00
  %65 = fcmp olt float %64, %47
  br i1 %65, label %66, label %140

; <label>:66                                      ; preds = %32
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb, %class.vector3* dereferenceable(12) %normal)
  %67 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 0
  %68 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 1
  %69 = load i32* %random_seed, align 4, !tbaa !13
  %70 = mul i32 %69, 16807
  %71 = lshr i32 %70, 9
  %72 = or i32 %71, 1065353216
  %73 = bitcast i32 %72 to float
  %74 = fadd fast float %73, -1.000000e+00
  %75 = mul i32 %69, 282475249
  store i32 %75, i32* %random_seed, align 4, !tbaa !13
  %76 = lshr i32 %75, 9
  %77 = or i32 %76, 1065353216
  %78 = bitcast i32 %77 to float
  %79 = fadd fast float %78, -1.000000e+00
  %80 = bitcast %class.vector3* %dir.i5 to i8*
  call void @llvm.lifetime.start(i64 12, i8* %80)
  %81 = fmul fast float %74, 0x401921FB60000000
  %82 = fsub fast float 2.000000e+00, %78
  %83 = tail call float @_Z4sqrtf(float %82) #4
  %84 = tail call float @_Z4sqrtf(float %79) #4
  %85 = tail call float @_Z3cosf(float %81) #4
  %86 = fmul fast float %83, %85
  %87 = tail call float @_Z3sinf(float %81) #4
  %88 = fmul fast float %83, %87
  %89 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 0
  store float %86, float* %89, align 4, !tbaa !15
  %90 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 1
  store float %84, float* %90, align 4, !tbaa !18
  %91 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 2
  store float %88, float* %91, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir.i5, %class.vector3* dereferenceable(12) %normal, %class.vector3* dereferenceable(12) %67, %class.vector3* dereferenceable(12) %68) #3
  %92 = load float* %89, align 4, !tbaa !15
  %93 = load float* %90, align 4, !tbaa !18
  %94 = load float* %91, align 4, !tbaa !19
  call void @llvm.lifetime.end(i64 12, i8* %80)
  %95 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 0
  %96 = load float* %95, align 4, !tbaa !15
  %97 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 1
  %98 = load float* %97, align 4, !tbaa !18
  %99 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 2
  %100 = load float* %99, align 4, !tbaa !19
  %101 = fmul fast float %92, %34
  %102 = fmul fast float %93, %33
  %103 = fadd fast float %101, %102
  %104 = fmul fast float %94, %35
  %105 = fadd fast float %103, %104
  %106 = fcmp ogt float %105, 0.000000e+00
  %..i4 = select i1 %106, float %105, float 0.000000e+00
  %107 = fmul fast float %96, %..i4
  %108 = fmul fast float %98, %..i4
  %109 = fmul fast float %100, %..i4
  %110 = fcmp ogt float %107, 0.000000e+00
  %111 = fcmp ogt float %108, 0.000000e+00
  %or.cond = or i1 %110, %111
  %112 = fcmp ogt float %109, 0.000000e+00
  %or.cond16 = or i1 %or.cond, %112
  br i1 %or.cond16, label %113, label %271

; <label>:113                                     ; preds = %66
  %114 = fmul fast float %84, 0x3FD45F3060000000
  %115 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 0
  %116 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %117 = load float* %116, align 4, !tbaa !15
  store float %117, float* %115, align 4, !tbaa !15
  %118 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 1
  %119 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %120 = load float* %119, align 4, !tbaa !18
  store float %120, float* %118, align 4, !tbaa !18
  %121 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 2
  %122 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %123 = load float* %122, align 4, !tbaa !19
  store float %123, float* %121, align 4, !tbaa !19
  %124 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %92, float* %124, align 4, !tbaa !15
  %125 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %93, float* %125, align 4, !tbaa !18
  %126 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %94, float* %126, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer16compute_radianceILj3ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %1, %struct.ray* dereferenceable(24) %2, i32* dereferenceable(4) %random_seed, i1 zeroext false)
  %127 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %128 = load float* %127, align 4, !tbaa !15
  %129 = fmul fast float %107, %128
  %130 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %131 = load float* %130, align 4, !tbaa !18
  %132 = fmul fast float %108, %131
  %133 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %134 = load float* %133, align 4, !tbaa !19
  %135 = fmul fast float %109, %134
  %136 = fmul fast float %47, %114
  %137 = fdiv fast float %129, %136, !fpmath !14
  %138 = fdiv fast float %132, %136, !fpmath !14
  %139 = fdiv fast float %135, %136, !fpmath !14
  br label %271

; <label>:140                                     ; preds = %32
  %141 = fadd fast float %47, %58
  %142 = fcmp olt float %64, %141
  br i1 %142, label %143, label %271

; <label>:143                                     ; preds = %140
  %144 = fmul fast float %34, %8
  %145 = fmul fast float %33, %13
  %146 = fadd fast float %144, %145
  %147 = fmul fast float %35, %19
  %148 = fadd fast float %146, %147
  %149 = fmul fast float %148, 2.000000e+00
  %150 = fmul fast float %34, %149
  %151 = fmul fast float %33, %149
  %152 = fmul fast float %35, %149
  %153 = fsub fast float %8, %150
  %154 = fsub fast float %13, %151
  %155 = fsub fast float %19, %152
  %156 = fcmp oeq float %153, 0.000000e+00
  %157 = fcmp oeq float %154, 0.000000e+00
  %or.cond.i = and i1 %156, %157
  %158 = fcmp oeq float %155, 0.000000e+00
  %or.cond17 = and i1 %or.cond.i, %158
  br i1 %or.cond17, label %_ZNK7vector3IfE10normalizedEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %143
  %159 = fmul fast float %153, %153
  %160 = fmul fast float %154, %154
  %161 = fadd fast float %159, %160
  %162 = fmul fast float %155, %155
  %163 = fadd fast float %162, %161
  %164 = tail call float @_Z5rsqrtf(float %163) #4
  %165 = fmul fast float %153, %164
  %166 = fmul fast float %154, %164
  %167 = fmul fast float %155, %164
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i, %143
  %168 = phi float [ %165, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %153, %143 ]
  %169 = phi float [ %166, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %154, %143 ]
  %170 = phi float [ %167, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %155, %143 ]
  %171 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 0
  store float %168, float* %171, align 4
  %172 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 1
  store float %169, float* %172, align 4
  %173 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 2
  store float %170, float* %173, align 4
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb1, %class.vector3* dereferenceable(12) %rnormal)
  %174 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 0
  %175 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 1
  %176 = load i32* %random_seed, align 4, !tbaa !13
  %177 = mul i32 %176, 16807
  %178 = lshr i32 %177, 9
  %179 = or i32 %178, 1065353216
  %180 = bitcast i32 %179 to float
  %181 = fadd fast float %180, -1.000000e+00
  %182 = mul i32 %176, 282475249
  store i32 %182, i32* %random_seed, align 4, !tbaa !13
  %183 = lshr i32 %182, 9
  %184 = or i32 %183, 1065353216
  %185 = bitcast i32 %184 to float
  %186 = fadd fast float %185, -1.000000e+00
  %187 = getelementptr inbounds %struct.material* %mat, i64 0, i32 2
  %188 = load float* %187, align 4, !tbaa !30
  %189 = bitcast %class.vector3* %dir.i to i8*
  call void @llvm.lifetime.start(i64 12, i8* %189)
  %190 = fmul fast float %181, 0x401921FB60000000
  %191 = fadd fast float %188, 1.000000e+00
  %192 = fdiv fast float 1.000000e+00, %191, !fpmath !14
  %193 = tail call float @_Z3powff(float %186, float %192) #4
  %194 = fmul fast float %193, %193
  %195 = fsub fast float 1.000000e+00, %194
  %196 = tail call float @_Z4sqrtf(float %195) #4
  %197 = tail call float @_Z3cosf(float %190) #4
  %198 = fmul fast float %196, %197
  %199 = tail call float @_Z3sinf(float %190) #4
  %200 = fmul fast float %196, %199
  %201 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 0
  store float %198, float* %201, align 4, !tbaa !15
  %202 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 1
  store float %193, float* %202, align 4, !tbaa !18
  %203 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 2
  store float %200, float* %203, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir.i, %class.vector3* dereferenceable(12) %rnormal, %class.vector3* dereferenceable(12) %174, %class.vector3* dereferenceable(12) %175) #3
  %204 = load float* %201, align 4, !tbaa !15
  %205 = load float* %202, align 4, !tbaa !18
  %206 = load float* %203, align 4, !tbaa !19
  %207 = call float @_Z3powff(float %193, float %188) #4
  %208 = fmul fast float %191, %207
  %209 = fmul fast float %208, 0x3FC45F3060000000
  call void @llvm.lifetime.end(i64 12, i8* %189)
  %210 = fmul fast float %204, %34
  %211 = fmul fast float %205, %33
  %212 = fadd fast float %210, %211
  %213 = fmul fast float %206, %35
  %214 = fadd fast float %212, %213
  %215 = fcmp ugt float %214, 0.000000e+00
  br i1 %215, label %220, label %216

; <label>:216                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %217 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %217, align 4, !tbaa !15
  %218 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %218, align 4, !tbaa !18
  %219 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %219, align 4, !tbaa !19
  br label %278

; <label>:220                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %221 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 0
  %222 = load float* %221, align 4, !tbaa !15
  %223 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 1
  %224 = load float* %223, align 4, !tbaa !18
  %225 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 2
  %226 = load float* %225, align 4, !tbaa !19
  %227 = fmul fast float %204, %168
  %228 = fmul fast float %205, %169
  %229 = fadd fast float %227, %228
  %230 = fmul fast float %206, %170
  %231 = fadd fast float %229, %230
  %232 = fcmp ogt float %231, 0.000000e+00
  %..i3 = select i1 %232, float %231, float 0.000000e+00
  %233 = load float* %187, align 4, !tbaa !30
  %234 = call float @_Z3powff(float %..i3, float %233) #4
  %235 = fmul fast float %222, %234
  %236 = fmul fast float %224, %234
  %237 = fmul fast float %226, %234
  %238 = fcmp ogt float %214, 0.000000e+00
  %..i = select i1 %238, float %214, float 0.000000e+00
  %239 = fmul fast float %..i, %235
  %240 = fmul fast float %..i, %236
  %241 = fmul fast float %..i, %237
  %242 = fcmp ogt float %239, 0.000000e+00
  %243 = fcmp ogt float %240, 0.000000e+00
  %or.cond18 = or i1 %242, %243
  %244 = fcmp ogt float %241, 0.000000e+00
  %or.cond19 = or i1 %or.cond18, %244
  br i1 %or.cond19, label %245, label %271

; <label>:245                                     ; preds = %220
  %246 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 0
  %247 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %248 = load float* %247, align 4, !tbaa !15
  store float %248, float* %246, align 4, !tbaa !15
  %249 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 1
  %250 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %251 = load float* %250, align 4, !tbaa !18
  store float %251, float* %249, align 4, !tbaa !18
  %252 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 2
  %253 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %254 = load float* %253, align 4, !tbaa !19
  store float %254, float* %252, align 4, !tbaa !19
  %255 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %204, float* %255, align 4, !tbaa !15
  %256 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %205, float* %256, align 4, !tbaa !18
  %257 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %206, float* %257, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer16compute_radianceILj3ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %4, i32* dereferenceable(4) %random_seed, i1 zeroext false)
  %258 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %259 = load float* %258, align 4, !tbaa !15
  %260 = fmul fast float %239, %259
  %261 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %262 = load float* %261, align 4, !tbaa !18
  %263 = fmul fast float %240, %262
  %264 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %265 = load float* %264, align 4, !tbaa !19
  %266 = fmul fast float %241, %265
  %267 = fmul fast float %58, %209
  %268 = fdiv fast float %260, %267, !fpmath !14
  %269 = fdiv fast float %263, %267, !fpmath !14
  %270 = fdiv fast float %266, %267, !fpmath !14
  br label %271

; <label>:271                                     ; preds = %245, %220, %140, %113, %66
  %272 = phi float [ 0.000000e+00, %140 ], [ %270, %245 ], [ %139, %113 ], [ %109, %66 ], [ %241, %220 ]
  %273 = phi float [ 0.000000e+00, %140 ], [ %269, %245 ], [ %138, %113 ], [ %108, %66 ], [ %240, %220 ]
  %274 = phi float [ 0.000000e+00, %140 ], [ %268, %245 ], [ %137, %113 ], [ %107, %66 ], [ %239, %220 ]
  %275 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %274, float* %275, align 4, !tbaa !15
  %276 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %273, float* %276, align 4, !tbaa !18
  %277 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %272, float* %277, align 4, !tbaa !19
  br label %278

; <label>:278                                     ; preds = %271, %216
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer16compute_radianceILj3ELi0EEE7vector3IfERK3rayRjb(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r, i32* nocapture dereferenceable(4) %random_seed, i1 zeroext %sample_emission) #2 align 2 {
  %p = alloca %"struct.simple_intersector::intersection", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %class.vector3, align 4
  %3 = alloca %class.vector3, align 4
  call void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* sret %p, %struct.ray* dereferenceable(24) %r)
  %4 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 3
  %5 = load i32* %4, align 4, !tbaa !20
  %6 = icmp ugt i32 %5, 7
  br i1 %6, label %7, label %11

; <label>:7                                       ; preds = %0
  %8 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %8, align 4, !tbaa !15
  %9 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %9, align 4, !tbaa !18
  %10 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %10, align 4, !tbaa !19
  br label %58

; <label>:11                                      ; preds = %0
  %12 = zext i32 %5 to i64
  %13 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12
  br i1 %sample_emission, label %14, label %21

; <label>:14                                      ; preds = %11
  %15 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 0
  %16 = load float* %15, align 4, !tbaa !15
  %17 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 1
  %18 = load float* %17, align 4, !tbaa !18
  %19 = getelementptr inbounds %"struct.std::__1::array.40"* inttoptr (i64 ptrtoint (%"struct.std::__1::array.40" addrspace(2)* @_ZL17cornell_materials to i64) to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 2
  %20 = load float* %19, align 4, !tbaa !19
  br label %21

; <label>:21                                      ; preds = %14, %11
  %22 = phi float [ %20, %14 ], [ 0.000000e+00, %11 ]
  %23 = phi float [ %18, %14 ], [ 0.000000e+00, %11 ]
  %24 = phi float [ %16, %14 ], [ 0.000000e+00, %11 ]
  %25 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %26 = load float* %25, align 4, !tbaa !15
  %27 = fsub fast float -0.000000e+00, %26
  %28 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %29 = load float* %28, align 4, !tbaa !18
  %30 = fsub fast float -0.000000e+00, %29
  %31 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %32 = load float* %31, align 4, !tbaa !19
  %33 = fsub fast float -0.000000e+00, %32
  %34 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 0
  store float %27, float* %34, align 4, !tbaa !15
  %35 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 1
  store float %30, float* %35, align 4, !tbaa !18
  %36 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 2
  store float %33, float* %36, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer27compute_direct_illuminationERK7vector3IfERKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %1, %class.vector3* dereferenceable(12) %2, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %37 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !15
  %39 = fadd fast float %24, %38
  %40 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !18
  %42 = fadd fast float %23, %41
  %43 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %44 = load float* %43, align 4, !tbaa !19
  %45 = fadd fast float %22, %44
  call void @_ZN18simple_path_tracer29compute_indirect_illuminationILj3EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %r, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %46 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %47 = load float* %46, align 4, !tbaa !15
  %48 = fadd fast float %39, %47
  %49 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %50 = load float* %49, align 4, !tbaa !18
  %51 = fadd fast float %42, %50
  %52 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %53 = load float* %52, align 4, !tbaa !19
  %54 = fadd fast float %45, %53
  %55 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %48, float* %55, align 4, !tbaa !15
  %56 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %51, float* %56, align 4, !tbaa !18
  %57 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %54, float* %57, align 4, !tbaa !19
  br label %58

; <label>:58                                      ; preds = %21, %7
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer29compute_indirect_illuminationILj3EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture  dereferenceable(24) %r, %"struct.simple_intersector::intersection"* nocapture  dereferenceable(32) %p, %struct.material* nocapture  dereferenceable(64) %mat, i32* nocapture dereferenceable(4) %random_seed) #2 align 2 {
  %dir.i5 = alloca %class.vector3, align 4
  %dir.i = alloca %class.vector3, align 4
  %normal = alloca %class.vector3, align 4
  %tb = alloca %"struct.std::__1::pair", align 4
  %rnormal = alloca %class.vector3, align 4
  %tb1 = alloca %"struct.std::__1::pair", align 4
  %1 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 0
  %2 = load float* %1, align 4, !tbaa !15
  %3 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %4 = load float* %3, align 4, !tbaa !15
  %5 = fmul fast float %2, %4
  %6 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 1
  %7 = load float* %6, align 4, !tbaa !18
  %8 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %9 = load float* %8, align 4, !tbaa !18
  %10 = fmul fast float %7, %9
  %11 = fadd fast float %5, %10
  %12 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 2
  %13 = load float* %12, align 4, !tbaa !19
  %14 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %15 = load float* %14, align 4, !tbaa !19
  %16 = fmul fast float %13, %15
  %17 = fadd fast float %11, %16
  %18 = fcmp ogt float %17, 0.000000e+00
  br i1 %18, label %19, label %25

; <label>:19                                      ; preds = %0
  %20 = fsub fast float -0.000000e+00, %2
  %21 = fsub fast float -0.000000e+00, %7
  %22 = fsub fast float -0.000000e+00, %13
  %23 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %20, float* %23, align 4, !tbaa !15
  %24 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %21, float* %24, align 4, !tbaa !18
  br label %28

; <label>:25                                      ; preds = %0
  %26 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %2, float* %26, align 4, !tbaa !15
  %27 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %7, float* %27, align 4, !tbaa !18
  br label %28

; <label>:28                                      ; preds = %25, %19
  %29 = phi float [ %7, %25 ], [ %21, %19 ]
  %30 = phi float [ %2, %25 ], [ %20, %19 ]
  %31 = phi float [ %13, %25 ], [ %22, %19 ]
  %32 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 2
  store float %31, float* %32, align 4
  %33 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 0
  %34 = load float* %33, align 4, !tbaa !15
  %35 = fmul fast float %34, 0x3FCC6A7F00000000
  %36 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 1
  %37 = load float* %36, align 4, !tbaa !18
  %38 = fmul fast float %37, 0x3FE69D4960000000
  %39 = fadd fast float %35, %38
  %40 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 2
  %41 = load float* %40, align 4, !tbaa !19
  %42 = fmul fast float %41, 0x3FB240B780000000
  %43 = fadd fast float %39, %42
  %44 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 0
  %45 = load float* %44, align 4, !tbaa !15
  %46 = fmul fast float %45, 0x3FCC6A7F00000000
  %47 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 1
  %48 = load float* %47, align 4, !tbaa !18
  %49 = fmul fast float %48, 0x3FE69D4960000000
  %50 = fadd fast float %46, %49
  %51 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 2
  %52 = load float* %51, align 4, !tbaa !19
  %53 = fmul fast float %52, 0x3FB240B780000000
  %54 = fadd fast float %50, %53
  %55 = load i32* %random_seed, align 4, !tbaa !13
  %56 = mul i32 %55, 16807
  store i32 %56, i32* %random_seed, align 4, !tbaa !13
  %57 = lshr i32 %56, 9
  %58 = or i32 %57, 1065353216
  %59 = bitcast i32 %58 to float
  %60 = fadd fast float %59, -1.000000e+00
  %61 = fcmp olt float %60, %43
  br i1 %61, label %62, label %113

; <label>:62                                      ; preds = %28
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb, %class.vector3* dereferenceable(12) %normal)
  %63 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 0
  %64 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 1
  %65 = load i32* %random_seed, align 4, !tbaa !13
  %66 = mul i32 %65, 16807
  %67 = lshr i32 %66, 9
  %68 = or i32 %67, 1065353216
  %69 = bitcast i32 %68 to float
  %70 = fadd fast float %69, -1.000000e+00
  %71 = mul i32 %65, 282475249
  store i32 %71, i32* %random_seed, align 4, !tbaa !13
  %72 = lshr i32 %71, 9
  %73 = or i32 %72, 1065353216
  %74 = bitcast i32 %73 to float
  %75 = fadd fast float %74, -1.000000e+00
  %76 = bitcast %class.vector3* %dir.i5 to i8*
  call void @llvm.lifetime.start(i64 12, i8* %76)
  %77 = fmul fast float %70, 0x401921FB60000000
  %78 = fsub fast float 2.000000e+00, %74
  %79 = tail call float @_Z4sqrtf(float %78) #4
  %80 = tail call float @_Z4sqrtf(float %75) #4
  %81 = tail call float @_Z3cosf(float %77) #4
  %82 = fmul fast float %79, %81
  %83 = tail call float @_Z3sinf(float %77) #4
  %84 = fmul fast float %79, %83
  %85 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 0
  store float %82, float* %85, align 4, !tbaa !15
  %86 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 1
  store float %80, float* %86, align 4, !tbaa !18
  %87 = getelementptr inbounds %class.vector3* %dir.i5, i64 0, i32 0, i32 0, i32 2
  store float %84, float* %87, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir.i5, %class.vector3* dereferenceable(12) %normal, %class.vector3* dereferenceable(12) %63, %class.vector3* dereferenceable(12) %64) #3
  %88 = load float* %85, align 4, !tbaa !15
  %89 = load float* %86, align 4, !tbaa !18
  %90 = load float* %87, align 4, !tbaa !19
  call void @llvm.lifetime.end(i64 12, i8* %76)
  %91 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 0
  %92 = load float* %91, align 4, !tbaa !15
  %93 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 1
  %94 = load float* %93, align 4, !tbaa !18
  %95 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 2
  %96 = load float* %95, align 4, !tbaa !19
  %97 = fmul fast float %88, %30
  %98 = fmul fast float %89, %29
  %99 = fadd fast float %97, %98
  %100 = fmul fast float %90, %31
  %101 = fadd fast float %99, %100
  %102 = fcmp ogt float %101, 0.000000e+00
  %..i4 = select i1 %102, float %101, float 0.000000e+00
  %103 = fmul fast float %92, %..i4
  %104 = fmul fast float %94, %..i4
  %105 = fmul fast float %96, %..i4
  %106 = fcmp ogt float %103, 0.000000e+00
  %107 = fcmp ogt float %104, 0.000000e+00
  %or.cond = or i1 %106, %107
  %108 = fcmp ogt float %105, 0.000000e+00
  %or.cond31 = or i1 %or.cond, %108
  br i1 %or.cond31, label %109, label %221

; <label>:109                                     ; preds = %62
  %110 = fmul fast float %80, 0x3FD45F3060000000
  %111 = fmul fast float %43, %110
  %112 = fdiv fast float 0.000000e+00, %111, !fpmath !14
  br label %221

; <label>:113                                     ; preds = %28
  %114 = fadd fast float %43, %54
  %115 = fcmp olt float %60, %114
  br i1 %115, label %116, label %221

; <label>:116                                     ; preds = %113
  %117 = fmul fast float %30, %4
  %118 = fmul fast float %29, %9
  %119 = fadd fast float %117, %118
  %120 = fmul fast float %31, %15
  %121 = fadd fast float %119, %120
  %122 = fmul fast float %121, 2.000000e+00
  %123 = fmul fast float %30, %122
  %124 = fmul fast float %29, %122
  %125 = fmul fast float %31, %122
  %126 = fsub fast float %4, %123
  %127 = fsub fast float %9, %124
  %128 = fsub fast float %15, %125
  %129 = fcmp oeq float %126, 0.000000e+00
  %130 = fcmp oeq float %127, 0.000000e+00
  %or.cond.i = and i1 %129, %130
  %131 = fcmp oeq float %128, 0.000000e+00
  %or.cond32 = and i1 %or.cond.i, %131
  br i1 %or.cond32, label %_ZNK7vector3IfE10normalizedEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %116
  %132 = fmul fast float %126, %126
  %133 = fmul fast float %127, %127
  %134 = fadd fast float %132, %133
  %135 = fmul fast float %128, %128
  %136 = fadd fast float %135, %134
  %137 = tail call float @_Z5rsqrtf(float %136) #4
  %138 = fmul fast float %126, %137
  %139 = fmul fast float %127, %137
  %140 = fmul fast float %128, %137
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i, %116
  %141 = phi float [ %138, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %126, %116 ]
  %142 = phi float [ %139, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %127, %116 ]
  %143 = phi float [ %140, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %128, %116 ]
  %144 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 0
  store float %141, float* %144, align 4
  %145 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 1
  store float %142, float* %145, align 4
  %146 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 2
  store float %143, float* %146, align 4
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb1, %class.vector3* dereferenceable(12) %rnormal)
  %147 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 0
  %148 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 1
  %149 = load i32* %random_seed, align 4, !tbaa !13
  %150 = mul i32 %149, 16807
  %151 = lshr i32 %150, 9
  %152 = or i32 %151, 1065353216
  %153 = bitcast i32 %152 to float
  %154 = fadd fast float %153, -1.000000e+00
  %155 = mul i32 %149, 282475249
  store i32 %155, i32* %random_seed, align 4, !tbaa !13
  %156 = lshr i32 %155, 9
  %157 = or i32 %156, 1065353216
  %158 = bitcast i32 %157 to float
  %159 = fadd fast float %158, -1.000000e+00
  %160 = getelementptr inbounds %struct.material* %mat, i64 0, i32 2
  %161 = load float* %160, align 4, !tbaa !30
  %162 = bitcast %class.vector3* %dir.i to i8*
  call void @llvm.lifetime.start(i64 12, i8* %162)
  %163 = fmul fast float %154, 0x401921FB60000000
  %164 = fadd fast float %161, 1.000000e+00
  %165 = fdiv fast float 1.000000e+00, %164, !fpmath !14
  %166 = tail call float @_Z3powff(float %159, float %165) #4
  %167 = fmul fast float %166, %166
  %168 = fsub fast float 1.000000e+00, %167
  %169 = tail call float @_Z4sqrtf(float %168) #4
  %170 = tail call float @_Z3cosf(float %163) #4
  %171 = fmul fast float %169, %170
  %172 = tail call float @_Z3sinf(float %163) #4
  %173 = fmul fast float %169, %172
  %174 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 0
  store float %171, float* %174, align 4, !tbaa !15
  %175 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 1
  store float %166, float* %175, align 4, !tbaa !18
  %176 = getelementptr inbounds %class.vector3* %dir.i, i64 0, i32 0, i32 0, i32 2
  store float %173, float* %176, align 4, !tbaa !19
  call void @_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_(%class.vector3* dereferenceable(12) %dir.i, %class.vector3* dereferenceable(12) %rnormal, %class.vector3* dereferenceable(12) %147, %class.vector3* dereferenceable(12) %148) #3
  %177 = load float* %174, align 4, !tbaa !15
  %178 = load float* %175, align 4, !tbaa !18
  %179 = load float* %176, align 4, !tbaa !19
  %180 = call float @_Z3powff(float %166, float %161) #4
  %181 = fmul fast float %164, %180
  %182 = fmul fast float %181, 0x3FC45F3060000000
  call void @llvm.lifetime.end(i64 12, i8* %162)
  %183 = fmul fast float %177, %30
  %184 = fmul fast float %178, %29
  %185 = fadd fast float %183, %184
  %186 = fmul fast float %179, %31
  %187 = fadd fast float %185, %186
  %188 = fcmp ugt float %187, 0.000000e+00
  br i1 %188, label %193, label %189

; <label>:189                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %190 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %190, align 4, !tbaa !15
  %191 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %191, align 4, !tbaa !18
  %192 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %192, align 4, !tbaa !19
  br label %228

; <label>:193                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %194 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 0
  %195 = load float* %194, align 4, !tbaa !15
  %196 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 1
  %197 = load float* %196, align 4, !tbaa !18
  %198 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 2
  %199 = load float* %198, align 4, !tbaa !19
  %200 = fmul fast float %177, %141
  %201 = fmul fast float %178, %142
  %202 = fadd fast float %200, %201
  %203 = fmul fast float %179, %143
  %204 = fadd fast float %202, %203
  %205 = fcmp ogt float %204, 0.000000e+00
  %..i3 = select i1 %205, float %204, float 0.000000e+00
  %206 = load float* %160, align 4, !tbaa !30
  %207 = call float @_Z3powff(float %..i3, float %206) #4
  %208 = fmul fast float %195, %207
  %209 = fmul fast float %197, %207
  %210 = fmul fast float %199, %207
  %211 = fcmp ogt float %187, 0.000000e+00
  %..i = select i1 %211, float %187, float 0.000000e+00
  %212 = fmul fast float %..i, %208
  %213 = fmul fast float %..i, %209
  %214 = fmul fast float %..i, %210
  %215 = fcmp ogt float %212, 0.000000e+00
  %216 = fcmp ogt float %213, 0.000000e+00
  %or.cond33 = or i1 %215, %216
  %217 = fcmp ogt float %214, 0.000000e+00
  %or.cond34 = or i1 %or.cond33, %217
  br i1 %or.cond34, label %218, label %221

; <label>:218                                     ; preds = %193
  %219 = fmul fast float %54, %182
  %220 = fdiv fast float 0.000000e+00, %219, !fpmath !14
  br label %221

; <label>:221                                     ; preds = %218, %193, %113, %109, %62
  %222 = phi float [ 0.000000e+00, %113 ], [ %220, %218 ], [ %112, %109 ], [ %105, %62 ], [ %214, %193 ]
  %223 = phi float [ 0.000000e+00, %113 ], [ %220, %218 ], [ %112, %109 ], [ %104, %62 ], [ %213, %193 ]
  %224 = phi float [ 0.000000e+00, %113 ], [ %220, %218 ], [ %112, %109 ], [ %103, %62 ], [ %212, %193 ]
  %225 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %224, float* %225, align 4, !tbaa !15
  %226 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %223, float* %226, align 4, !tbaa !18
  %227 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %222, float* %227, align 4, !tbaa !19
  br label %228

; <label>:228                                     ; preds = %221, %189
  ret void
}

attributes #0 = { alwaysinline nounwind readnone "less-precise-fpmad"="true" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { nounwind readnone "less-precise-fpmad"="true" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #2 = { nounwind "less-precise-fpmad"="true" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #3 = { nounwind }
attributes #4 = { nounwind readnone }

!opencl.kernels = !{!0}
!llvm.ident = !{!6}

!0 = metadata !{void (%class.vector3 addrspace(1)*, i32, i32, %class.vector2.23*)* @_Z10path_tracePU3AS17vector3IfEjj7vector2IjE, metadata !1, metadata !2, metadata !3, metadata !4, metadata !5}
!1 = metadata !{metadata !"kernel_arg_addr_space", i32 1, i32 0, i32 0, i32 0}
!2 = metadata !{metadata !"kernel_arg_access_qual", metadata !"none", metadata !"none", metadata !"none", metadata !"none"}
!3 = metadata !{metadata !"kernel_arg_type", metadata !"float3*", metadata !"uint32_t", metadata !"uint32_t", metadata !"uint2"}
!4 = metadata !{metadata !"kernel_arg_type_qual", metadata !"", metadata !"const", metadata !"const", metadata !"const"}
!5 = metadata !{metadata !"kernel_arg_name", metadata !"img", metadata !"iteration", metadata !"seed", metadata !"img_size"}
!6 = metadata !{metadata !"clang version 3.5.0 (tags/RELEASE_350/final)"}
!7 = metadata !{metadata !8, metadata !9, i64 0}
!8 = metadata !{metadata !"_ZTSN7vector2IjEUt_Ut_E", metadata !9, i64 0, metadata !9, i64 4}
!9 = metadata !{metadata !"int", metadata !10, i64 0}
!10 = metadata !{metadata !"omnipotent char", metadata !11, i64 0}
!11 = metadata !{metadata !"Simple C/C++ TBAA"}
!12 = metadata !{metadata !8, metadata !9, i64 4}
!13 = metadata !{metadata !9, metadata !9, i64 0}
!14 = metadata !{float 2.500000e+00}
!15 = metadata !{metadata !16, metadata !17, i64 0}
!16 = metadata !{metadata !"_ZTSN7vector3IfEUt_Ut_E", metadata !17, i64 0, metadata !17, i64 4, metadata !17, i64 8}
!17 = metadata !{metadata !"float", metadata !10, i64 0}
!18 = metadata !{metadata !16, metadata !17, i64 4}
!19 = metadata !{metadata !16, metadata !17, i64 8}
!20 = metadata !{metadata !21, metadata !23, i64 28}
!21 = metadata !{metadata !"_ZTSN18simple_intersector12intersectionE", metadata !22, i64 0, metadata !22, i64 12, metadata !17, i64 24, metadata !23, i64 28}
!22 = metadata !{metadata !"_ZTS7vector3IfE", metadata !10, i64 0}
!23 = metadata !{metadata !"_ZTS14CORNELL_OBJECT", metadata !10, i64 0}
!24 = metadata !{metadata !25, metadata !9, i64 0}
!25 = metadata !{metadata !"_ZTSN7vector3IjEUt_Ut_E", metadata !9, i64 0, metadata !9, i64 4, metadata !9, i64 8}
!26 = metadata !{metadata !25, metadata !9, i64 4}
!27 = metadata !{metadata !25, metadata !9, i64 8}
!28 = metadata !{metadata !23, metadata !23, i64 0}
!29 = metadata !{metadata !21, metadata !17, i64 24}
!30 = metadata !{metadata !31, metadata !17, i64 24}
!31 = metadata !{metadata !"_ZTS8material", metadata !22, i64 0, metadata !22, i64 12, metadata !17, i64 24, metadata !22, i64 28, metadata !22, i64 40, metadata !22, i64 52}
!32 = metadata !{metadata !17, metadata !17, i64 0}
