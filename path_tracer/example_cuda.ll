; ModuleID = '-'
target datalayout = "e-i64:64-v16:16-v32:32-n16:32:64"
target triple = "nvptx64-nvidia-cuda"

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
%union.anon.24 = type { %struct.anon.25 }
%struct.anon.25 = type { i32, i32 }
%struct.ray = type { %class.vector3, %class.vector3 }
%"struct.simple_intersector::intersection" = type { %class.vector3, %class.vector3, float, i32 }
%"struct.std::__1::pair" = type { %class.vector3, %class.vector3 }

@_ZL16cornell_vertices = internal addrspace(4) constant %"struct.std::__1::array" { [64 x %class.vector3] [%class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0.000000e+00 } } }, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4041266660000000, float 0x404B666660000000, float 0x4036B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4041266660000000, float 0x404B666660000000, float 0x40409999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40354CCCC0000000, float 0x404B666660000000, float 0x40409999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40354CCCC0000000, float 0x404B666660000000, float 0x4036B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0.000000e+00, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0x404BF5C280000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 5.500000e+01, float 0x404B70A3E0000000, float 0.000000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 1.650000e+01, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 1.650000e+01, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 1.650000e+01, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 1.650000e+01, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 0.000000e+00, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 1.650000e+01, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 1.650000e+01, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 0.000000e+00, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 0.000000e+00, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 1.650000e+01, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 1.650000e+01, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.900000e+01, float 0.000000e+00, float 0x4026CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 0.000000e+00, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 1.650000e+01, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 1.650000e+01, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 1.300000e+01, float 0.000000e+00, float 6.500000e+00 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 0.000000e+00, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.400000e+01, float 1.650000e+01, float 0x403B333340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 1.650000e+01, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4020666660000000, float 0.000000e+00, float 2.250000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 3.300000e+01, float 0x4038B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 3.300000e+01, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 3.300000e+01, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 3.300000e+01, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 0.000000e+00, float 0x4038B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 3.300000e+01, float 0x4038B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 3.300000e+01, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 0.000000e+00, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 0.000000e+00, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x40479999A0000000, float 3.300000e+01, float 0x40444CCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 3.300000e+01, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 0.000000e+00, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 0.000000e+00, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x403F666660000000, float 3.300000e+01, float 0x4046CCCCC0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 3.300000e+01, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 0.000000e+00, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 0.000000e+00, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 2.650000e+01, float 3.300000e+01, float 0x403D9999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 3.300000e+01, float 0x4038B33340000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x4045266660000000, float 0.000000e+00, float 0x4038B33340000000 } } }] }, align 4
@_ZL15cornell_indices = internal addrspace(4) constant %"struct.std::__1::array.39" { [32 x %class.vector3.38] [%class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 0, i32 1, i32 2 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 0, i32 2, i32 3 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 4, i32 5, i32 6 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 4, i32 6, i32 7 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 8, i32 9, i32 10 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 8, i32 10, i32 11 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 12, i32 13, i32 14 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 12, i32 14, i32 15 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 16, i32 17, i32 18 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 16, i32 18, i32 19 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 20, i32 21, i32 22 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 20, i32 22, i32 23 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 24, i32 25, i32 26 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 24, i32 26, i32 27 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 28, i32 29, i32 30 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 28, i32 30, i32 31 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 32, i32 33, i32 34 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 32, i32 34, i32 35 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 36, i32 37, i32 38 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 36, i32 38, i32 39 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 40, i32 41, i32 42 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 40, i32 42, i32 43 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 44, i32 45, i32 46 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 44, i32 46, i32 47 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 48, i32 49, i32 50 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 48, i32 50, i32 51 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 52, i32 53, i32 54 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 52, i32 54, i32 55 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 56, i32 57, i32 58 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 56, i32 58, i32 59 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 60, i32 61, i32 62 } } }, %class.vector3.38 { %union.anon.18 { %struct.anon.17 { i32 60, i32 62, i32 63 } } }] }, align 4
@_ZL17cornell_materials = internal addrspace(4) constant %"struct.std::__1::array.40" { [8 x %struct.material] [%struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0x3FDA1A1980000000, float 0x3FDA1A1980000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FC99999A0000000, float 0x3FC99999A0000000, float 0x3FC99999A0000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 { %union.anon { %struct.anon { float 1.500000e+01, float 1.500000e+01, float 1.500000e+01 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x3FB04C26C0000000, float 0x3FB04C26C0000000, float 0x3FB04C26C0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0x3FDA1A1980000000, float 0x3FDA1A1980000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0x3FDA1A1980000000, float 0x3FDA1A1980000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x3FDA1A1980000000, float 0.000000e+00 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0.000000e+00, float 0x3FC09DF4E0000000, float 0.000000e+00 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0.000000e+00, float 0.000000e+00 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0.000000e+00, float 0.000000e+00 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FDA1A1980000000, float 0x3FDA1A1980000000, float 0x3FDA1A1980000000 } } }, %class.vector3 zeroinitializer, float 1.000000e+01, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000, float 0x3FC09DF4E0000000 } } }, %class.vector3 zeroinitializer }, %struct.material { %class.vector3 { %union.anon { %struct.anon { float 0x3FB99999A0000000, float 0x3FB99999A0000000, float 0x3FB99999A0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x3FECCCCCC0000000, float 0x3FECCCCCC0000000, float 0x3FECCCCCC0000000 } } }, float 1.000000e+02, %class.vector3 zeroinitializer, %class.vector3 { %union.anon { %struct.anon { float 0x3FA04C26C0000000, float 0x3FA04C26C0000000, float 0x3FA04C26C0000000 } } }, %class.vector3 { %union.anon { %struct.anon { float 0x402D388940000000, float 0x402D388940000000, float 0x402D388940000000 } } } }] }, align 4
@_ZL18cornell_object_map = internal addrspace(4) constant %"struct.std::__1::array.63" { [32 x i32] [i32 0, i32 0, i32 1, i32 1, i32 2, i32 2, i32 3, i32 3, i32 4, i32 4, i32 5, i32 5, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 6, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7, i32 7] }, align 4

; Function Attrs: nounwind readnone
define float @_Z3powff(float %a, float %b) #0 {
  %1 = tail call float @llvm.nvvm.lg2.approx.ftz.f(float %a)
  %2 = fmul fast float %1, %b
  %3 = tail call float @llvm.nvvm.ex2.approx.ftz.f(float %2)
  ret float %3
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.ex2.approx.ftz.f(float) #1

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.lg2.approx.ftz.f(float) #1

; Function Attrs: nounwind readnone
define float @_Z4sqrtf(float %a) #0 {
  %1 = tail call float @llvm.nvvm.sqrt.rn.ftz.f(float %a)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.sqrt.rn.ftz.f(float) #1

; Function Attrs: nounwind readnone
define float @_Z3sinf(float %a) #0 {
  %1 = tail call float @llvm.nvvm.sin.approx.ftz.f(float %a)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.sin.approx.ftz.f(float) #1

; Function Attrs: nounwind readnone
define float @_Z3cosf(float %a) #0 {
  %1 = tail call float @llvm.nvvm.cos.approx.ftz.f(float %a)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.cos.approx.ftz.f(float) #1

; Function Attrs: nounwind readnone
define float @_Z3tanf(float %a) #0 {
  %1 = tail call float @llvm.nvvm.sin.approx.ftz.f(float %a) #4
  %2 = tail call float @llvm.nvvm.cos.approx.ftz.f(float %a) #4
  %3 = fdiv fast float %1, %2
  ret float %3
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_fmodf"(float %y, float %x) #2 {
  %1 = fdiv fast float %y, %x
  %2 = tail call float @llvm.nvvm.trunc.ftz.f(float %1)
  %3 = fmul fast float %2, %x
  %4 = fsub fast float %y, %3
  ret float %4
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.trunc.ftz.f(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_sqrtf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.sqrt.rz.ftz.f(float %val)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.sqrt.rz.ftz.f(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_inv_sqrtf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %val)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.rsqrt.approx.ftz.f(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_absf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.fabs.ftz.f(float %val)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.fabs.ftz.f(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_floorf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.floor.ftz.f(float %val)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.floor.ftz.f(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_ceilf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.ceil.ftz.f(float %val)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.ceil.ftz.f(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_roundf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.round.ftz.f(float %val)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.round.ftz.f(float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_truncf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.trunc.ftz.f(float %val)
  ret float %1
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_rintf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.trunc.ftz.f(float %val)
  ret float %1
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_sinf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.sin.approx.ftz.f(float %val)
  ret float %1
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_cosf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.cos.approx.ftz.f(float %val)
  ret float %1
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_tanf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.sin.approx.ftz.f(float %val)
  %2 = tail call float @llvm.nvvm.cos.approx.ftz.f(float %val)
  %3 = fdiv fast float %1, %2
  ret float %3
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_asinf"(float %val) #2 {
  ret float %val
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_acosf"(float %val) #2 {
  ret float %val
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_atanf"(float %val) #2 {
  ret float %val
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_atan2f"(float %y, float %x) #2 {
  %1 = fadd fast float %y, %x
  ret float %1
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_fmaf"(float %a, float %b, float %c) #2 {
  %1 = tail call float @llvm.nvvm.fma.rz.ftz.f(float %a, float %b, float %c)
  ret float %1
}

; Function Attrs: nounwind readnone
declare float @llvm.nvvm.fma.rz.ftz.f(float, float, float) #1

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_expf"(float %val) #2 {
  %1 = fmul fast float %val, 0x3FF7154760000000
  %2 = tail call float @llvm.nvvm.ex2.approx.ftz.f(float %1)
  ret float %2
}

; Function Attrs: alwaysinline nounwind readnone
define float @"\01floor__const_math_logf"(float %val) #2 {
  %1 = tail call float @llvm.nvvm.lg2.approx.ftz.f(float %val)
  %2 = fmul fast float %1, 0x3FF7154760000000
  ret float %2
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_fmodd"(double %y, double %x) #2 {
  %1 = fdiv fast double %y, %x
  %2 = tail call double @llvm.nvvm.trunc.d(double %1)
  %3 = fmul fast double %2, %x
  %4 = fsub fast double %y, %3
  ret double %4
}

; Function Attrs: nounwind readnone
declare double @llvm.nvvm.trunc.d(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_sqrtd"(double %val) #2 {
  %1 = tail call double @llvm.nvvm.sqrt.rz.d(double %val)
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @llvm.nvvm.sqrt.rz.d(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_inv_sqrtd"(double %val) #2 {
  %1 = tail call double @llvm.nvvm.rsqrt.approx.d(double %val)
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @llvm.nvvm.rsqrt.approx.d(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_absd"(double %val) #2 {
  %1 = tail call double @llvm.nvvm.fabs.d(double %val)
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @llvm.nvvm.fabs.d(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_floord"(double %val) #2 {
  %1 = tail call double @llvm.nvvm.floor.d(double %val)
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @llvm.nvvm.floor.d(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_ceild"(double %val) #2 {
  %1 = tail call double @llvm.nvvm.ceil.d(double %val)
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @llvm.nvvm.ceil.d(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_roundd"(double %val) #2 {
  %1 = tail call double @llvm.nvvm.round.d(double %val)
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @llvm.nvvm.round.d(double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_truncd"(double %val) #2 {
  %1 = tail call double @llvm.nvvm.trunc.d(double %val)
  ret double %1
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_rintd"(double %val) #2 {
  %1 = tail call double @llvm.nvvm.trunc.d(double %val)
  ret double %1
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_sind"(double %val) #2 {
  %1 = fptrunc double %val to float
  %2 = tail call float @llvm.nvvm.sin.approx.ftz.f(float %1)
  %3 = fpext float %2 to double
  ret double %3
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_cosd"(double %val) #2 {
  %1 = fptrunc double %val to float
  %2 = tail call float @llvm.nvvm.cos.approx.ftz.f(float %1)
  %3 = fpext float %2 to double
  ret double %3
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_tand"(double %val) #2 {
  %1 = fptrunc double %val to float
  %2 = tail call float @llvm.nvvm.sin.approx.ftz.f(float %1)
  %3 = tail call float @llvm.nvvm.cos.approx.ftz.f(float %1)
  %4 = fdiv fast float %2, %3
  %5 = fpext float %4 to double
  ret double %5
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_asind"(double %val) #2 {
  ret double %val
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_acosd"(double %val) #2 {
  ret double %val
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_atand"(double %val) #2 {
  ret double undef
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_atan2d"(double %y, double %x) #2 {
  %1 = fadd fast double %y, %x
  ret double %1
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_fmad"(double %a, double %b, double %c) #2 {
  %1 = tail call double @llvm.nvvm.fma.rz.d(double %a, double %b, double %c)
  ret double %1
}

; Function Attrs: nounwind readnone
declare double @llvm.nvvm.fma.rz.d(double, double, double) #1

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_expd"(double %val) #2 {
  %1 = fptrunc double %val to float
  %2 = fmul fast float %1, 0x3FF7154760000000
  %3 = tail call float @llvm.nvvm.ex2.approx.ftz.f(float %2)
  %4 = fpext float %3 to double
  ret double %4
}

; Function Attrs: alwaysinline nounwind readnone
define double @"\01floor__const_math_logd"(double %val) #2 {
  %1 = fptrunc double %val to float
  %2 = tail call float @llvm.nvvm.lg2.approx.ftz.f(float %1)
  %3 = fpext float %2 to double
  %4 = fmul fast double %3, 0x3FF715476533245E
  ret double %4
}

; Function Attrs: nounwind
define void @path_trace(%class.vector3* nocapture %img, i32 %iteration, i32 %seed, %union.anon.24 %img_size.coerce) #3 {
  %random_seed = alloca i32, align 4
  %r = alloca %struct.ray, align 4
  %color = alloca %class.vector3, align 4
  %1 = extractvalue %union.anon.24 %img_size.coerce, 0
  %2 = extractvalue %struct.anon.25 %1, 0
  %3 = extractvalue %struct.anon.25 %1, 1
  %4 = tail call i32 @llvm.ptx.read.ctaid.x()
  %5 = tail call i32 @llvm.ptx.read.ntid.x()
  %6 = mul nsw i32 %5, %4
  %7 = tail call i32 @llvm.ptx.read.tid.x()
  %8 = add nsw i32 %6, %7
  %9 = add i32 %8, %iteration
  %10 = and i32 %seed, 31
  %11 = and i32 %10, %9
  %12 = shl i32 %8, %11
  %13 = xor i32 %12, %seed
  %14 = add i32 %8, %seed
  %15 = mul i32 %3, %2
  %16 = mul i32 %15, %14
  %17 = add i32 %16, %seed
  %18 = add i32 %17, %13
  %19 = xor i32 %18, 1392237036
  %20 = urem i32 %8, %2
  %21 = udiv i32 %8, %3
  %22 = uitofp i32 %20 to float
  %23 = uitofp i32 %21 to float
  %24 = mul i32 %19, 16807
  %25 = lshr i32 %24, 9
  %26 = or i32 %25, 1065353216
  %27 = bitcast i32 %26 to float
  %28 = fadd fast float %27, -1.000000e+00
  %29 = mul i32 %19, 282475249
  store i32 %29, i32* %random_seed, align 4, !tbaa !2
  %30 = lshr i32 %29, 9
  %31 = or i32 %30, 1065353216
  %32 = bitcast i32 %31 to float
  %33 = fadd fast float %32, -1.000000e+00
  %34 = fadd fast float %22, %28
  %35 = fadd fast float %23, %33
  %36 = uitofp i32 %2 to float
  %37 = uitofp i32 %3 to float
  %38 = fdiv fast float %36, %37
  %39 = tail call float @llvm.nvvm.sin.approx.ftz.f(float 0x3FD38C3540000000) #4
  %40 = tail call float @llvm.nvvm.cos.approx.ftz.f(float 0x3FD38C3540000000) #4
  %41 = fdiv fast float %39, %40
  %42 = fmul fast float %41, -2.000000e+00
  %43 = fmul fast float %38, %42
  %44 = fdiv fast float %43, %36
  %45 = fdiv fast float 0.000000e+00, %36
  %46 = fdiv fast float -0.000000e+00, %37
  %47 = fdiv fast float %42, %37
  %48 = fdiv fast float 0.000000e+00, %37
  %49 = fmul fast float %43, 5.000000e-01
  %50 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 0
  store float 0x403BCCCCC0000000, float* %50, align 4, !tbaa !6
  %51 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 1
  store float 0x403B4CCCC0000000, float* %51, align 4, !tbaa !9
  %52 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 2
  store float -8.000000e+01, float* %52, align 4, !tbaa !10
  %53 = fmul fast float %44, %34
  %54 = fmul fast float %45, %34
  %55 = fsub fast float %53, %49
  %56 = fadd fast float %41, %54
  %57 = fadd fast float %54, 1.000000e+00
  %58 = fmul fast float %46, %35
  %59 = fmul fast float %47, %35
  %60 = fmul fast float %48, %35
  %61 = fadd fast float %58, %55
  %62 = fadd fast float %59, %56
  %63 = fadd fast float %60, %57
  %64 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %61, float* %64, align 4, !tbaa !6
  %65 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %62, float* %65, align 4, !tbaa !9
  %66 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %63, float* %66, align 4, !tbaa !10
  call void @_ZN18simple_path_tracer16compute_radianceILj0ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %color, %struct.ray* dereferenceable(24) %r, i32* dereferenceable(4) %random_seed, i1 zeroext true)
  %67 = icmp eq i32 %iteration, 0
  %68 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 0
  %69 = load float* %68, align 4
  br i1 %67, label %70, label %79

; <label>:70                                      ; preds = %0
  %71 = zext i32 %8 to i64
  %72 = getelementptr inbounds %class.vector3* %img, i64 %71, i32 0, i32 0, i32 0
  store float %69, float* %72, align 4, !tbaa !6
  %73 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 1
  %74 = load float* %73, align 4, !tbaa !9
  %75 = getelementptr inbounds %class.vector3* %img, i64 %71, i32 0, i32 0, i32 1
  store float %74, float* %75, align 4, !tbaa !9
  %76 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 2
  %77 = load float* %76, align 4, !tbaa !10
  %78 = getelementptr inbounds %class.vector3* %img, i64 %71, i32 0, i32 0, i32 2
  store float %77, float* %78, align 4, !tbaa !10
  br label %104

; <label>:79                                      ; preds = %0
  %80 = add i32 %iteration, 1
  %81 = uitofp i32 %80 to float
  %82 = fdiv fast float 1.000000e+00, %81
  %83 = fsub fast float 1.000000e+00, %82
  %84 = zext i32 %8 to i64
  %85 = getelementptr inbounds %class.vector3* %img, i64 %84, i32 0, i32 0, i32 0
  %86 = load float* %85, align 4, !tbaa !6
  %87 = getelementptr inbounds %class.vector3* %img, i64 %84, i32 0, i32 0, i32 1
  %88 = load float* %87, align 4, !tbaa !9
  %89 = getelementptr inbounds %class.vector3* %img, i64 %84, i32 0, i32 0, i32 2
  %90 = load float* %89, align 4, !tbaa !10
  %91 = fmul fast float %86, %83
  %92 = fmul fast float %83, %88
  %93 = fmul fast float %83, %90
  %94 = fmul fast float %82, %69
  %95 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 1
  %96 = load float* %95, align 4, !tbaa !9
  %97 = fmul fast float %82, %96
  %98 = getelementptr inbounds %class.vector3* %color, i64 0, i32 0, i32 0, i32 2
  %99 = load float* %98, align 4, !tbaa !10
  %100 = fmul fast float %82, %99
  %101 = fadd fast float %91, %94
  %102 = fadd fast float %92, %97
  %103 = fadd fast float %93, %100
  store float %101, float* %85, align 4, !tbaa !6
  store float %102, float* %87, align 4, !tbaa !9
  store float %103, float* %89, align 4, !tbaa !10
  br label %104

; <label>:104                                     ; preds = %79, %70
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @llvm.ptx.read.ctaid.x() #1

; Function Attrs: nounwind readnone
declare i32 @llvm.ptx.read.ntid.x() #1

; Function Attrs: nounwind readnone
declare i32 @llvm.ptx.read.tid.x() #1

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer16compute_radianceILj0ELi0EEE7vector3IfERK3rayRjb(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture readonly dereferenceable(24) %r, i32* nocapture dereferenceable(4) %random_seed, i1 zeroext %sample_emission) #3 align 2 {
  %p = alloca %"struct.simple_intersector::intersection", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %class.vector3, align 4
  %3 = alloca %class.vector3, align 4
  call void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* sret %p, %struct.ray* dereferenceable(24) %r)
  %4 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 3
  %5 = load i32* %4, align 4, !tbaa !11
  %6 = icmp ugt i32 %5, 7
  br i1 %6, label %7, label %11

; <label>:7                                       ; preds = %0
  %8 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %8, align 4, !tbaa !6
  %9 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %9, align 4, !tbaa !9
  %10 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %10, align 4, !tbaa !10
  br label %58

; <label>:11                                      ; preds = %0
  %12 = zext i32 %5 to i64
  %13 = getelementptr inbounds %"struct.std::__1::array.40"* addrspacecast (%"struct.std::__1::array.40" addrspace(4)* @_ZL17cornell_materials to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12
  br i1 %sample_emission, label %14, label %21

; <label>:14                                      ; preds = %11
  %15 = getelementptr inbounds %"struct.std::__1::array.40"* addrspacecast (%"struct.std::__1::array.40" addrspace(4)* @_ZL17cornell_materials to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 0
  %16 = load float* %15, align 4, !tbaa !6
  %17 = getelementptr inbounds %"struct.std::__1::array.40"* addrspacecast (%"struct.std::__1::array.40" addrspace(4)* @_ZL17cornell_materials to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 1
  %18 = load float* %17, align 4, !tbaa !9
  %19 = getelementptr inbounds %"struct.std::__1::array.40"* addrspacecast (%"struct.std::__1::array.40" addrspace(4)* @_ZL17cornell_materials to %"struct.std::__1::array.40"*), i64 0, i32 0, i64 %12, i32 3, i32 0, i32 0, i32 2
  %20 = load float* %19, align 4, !tbaa !10
  br label %21

; <label>:21                                      ; preds = %14, %11
  %22 = phi float [ %20, %14 ], [ 0.000000e+00, %11 ]
  %23 = phi float [ %18, %14 ], [ 0.000000e+00, %11 ]
  %24 = phi float [ %16, %14 ], [ 0.000000e+00, %11 ]
  %25 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %26 = load float* %25, align 4, !tbaa !6
  %27 = fsub fast float -0.000000e+00, %26
  %28 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %29 = load float* %28, align 4, !tbaa !9
  %30 = fsub fast float -0.000000e+00, %29
  %31 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %32 = load float* %31, align 4, !tbaa !10
  %33 = fsub fast float -0.000000e+00, %32
  %34 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 0
  store float %27, float* %34, align 4, !tbaa !6
  %35 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 1
  store float %30, float* %35, align 4, !tbaa !9
  %36 = getelementptr inbounds %class.vector3* %2, i64 0, i32 0, i32 0, i32 2
  store float %33, float* %36, align 4, !tbaa !10
  call void @_ZN18simple_path_tracer27compute_direct_illuminationERK7vector3IfERKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %1, %class.vector3* dereferenceable(12) %2, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %37 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !6
  %39 = fadd fast float %24, %38
  %40 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !9
  %42 = fadd fast float %23, %41
  %43 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %44 = load float* %43, align 4, !tbaa !10
  %45 = fadd fast float %22, %44
  call void @_ZN18simple_path_tracer29compute_indirect_illuminationILj0EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %r, %"struct.simple_intersector::intersection"* dereferenceable(32) %p, %struct.material* dereferenceable(64) %13, i32* dereferenceable(4) %random_seed)
  %46 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %47 = load float* %46, align 4, !tbaa !6
  %48 = fadd fast float %39, %47
  %49 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %50 = load float* %49, align 4, !tbaa !9
  %51 = fadd fast float %42, %50
  %52 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %53 = load float* %52, align 4, !tbaa !10
  %54 = fadd fast float %45, %53
  %55 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %48, float* %55, align 4, !tbaa !6
  %56 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %51, float* %56, align 4, !tbaa !9
  %57 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %54, float* %57, align 4, !tbaa !10
  br label %58

; <label>:58                                      ; preds = %21, %7
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* noalias nocapture sret %agg.result, %struct.ray* nocapture readonly dereferenceable(24) %r) #3 align 2 {
  %1 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %2 = load float* %1, align 4, !tbaa !9
  %3 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %4 = load float* %3, align 4, !tbaa !10
  %5 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %6 = load float* %5, align 4, !tbaa !6
  %7 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 0
  %8 = load float* %7, align 4, !tbaa !6
  %9 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 1
  %10 = load float* %9, align 4, !tbaa !9
  %11 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 2
  %12 = load float* %11, align 4, !tbaa !10
  br label %13

; <label>:13                                      ; preds = %_ZNK7vector3IbE3anyIbvEEbv.exit.thread, %0
  %object.019 = phi i32 [ 8, %0 ], [ %object.1, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %storemerge17 = phi i64 [ 0, %0 ], [ %141, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %14 = phi float [ 0x7FF0000000000000, %0 ], [ %140, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %15 = phi float [ 0.000000e+00, %0 ], [ %139, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %16 = phi float [ 0.000000e+00, %0 ], [ %138, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %17 = phi float [ 0.000000e+00, %0 ], [ %137, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %18 = getelementptr inbounds %"struct.std::__1::array.39"* addrspacecast (%"struct.std::__1::array.39" addrspace(4)* @_ZL15cornell_indices to %"struct.std::__1::array.39"*), i64 0, i32 0, i64 %storemerge17, i32 0, i32 0, i32 0
  %19 = load i32* %18, align 4, !tbaa !15
  %20 = zext i32 %19 to i64
  %21 = getelementptr inbounds %"struct.std::__1::array.39"* addrspacecast (%"struct.std::__1::array.39" addrspace(4)* @_ZL15cornell_indices to %"struct.std::__1::array.39"*), i64 0, i32 0, i64 %storemerge17, i32 0, i32 0, i32 1
  %22 = load i32* %21, align 4, !tbaa !17
  %23 = zext i32 %22 to i64
  %24 = getelementptr inbounds %"struct.std::__1::array.39"* addrspacecast (%"struct.std::__1::array.39" addrspace(4)* @_ZL15cornell_indices to %"struct.std::__1::array.39"*), i64 0, i32 0, i64 %storemerge17, i32 0, i32 0, i32 2
  %25 = load i32* %24, align 4, !tbaa !18
  %26 = zext i32 %25 to i64
  %27 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %20, i32 0, i32 0, i32 0
  %28 = load float* %27, align 4, !tbaa !6
  %29 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %26, i32 0, i32 0, i32 0
  %30 = load float* %29, align 4, !tbaa !6
  %31 = fsub fast float %28, %30
  %32 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %20, i32 0, i32 0, i32 1
  %33 = load float* %32, align 4, !tbaa !9
  %34 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %26, i32 0, i32 0, i32 1
  %35 = load float* %34, align 4, !tbaa !9
  %36 = fsub fast float %33, %35
  %37 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %20, i32 0, i32 0, i32 2
  %38 = load float* %37, align 4, !tbaa !10
  %39 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %26, i32 0, i32 0, i32 2
  %40 = load float* %39, align 4, !tbaa !10
  %41 = fsub fast float %38, %40
  %42 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %23, i32 0, i32 0, i32 0
  %43 = load float* %42, align 4, !tbaa !6
  %44 = fsub fast float %43, %30
  %45 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %23, i32 0, i32 0, i32 1
  %46 = load float* %45, align 4, !tbaa !9
  %47 = fsub fast float %46, %35
  %48 = getelementptr inbounds %"struct.std::__1::array"* addrspacecast (%"struct.std::__1::array" addrspace(4)* @_ZL16cornell_vertices to %"struct.std::__1::array"*), i64 0, i32 0, i64 %23, i32 0, i32 0, i32 2
  %49 = load float* %48, align 4, !tbaa !10
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

_ZN10const_math3absIfvEET_S1_.exit:               ; preds = %13, %66
  %68 = phi float [ %67, %66 ], [ %64, %13 ]
  %69 = fcmp ugt float %68, 0x3EE4F8B580000000
  br i1 %69, label %70, label %_ZNK7vector3IbE3anyIbvEEbv.exit.thread

; <label>:70                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit
  %71 = fdiv fast float 1.000000e+00, %64
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
  %128 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %127) #4
  %129 = fmul fast float %113, %128
  %130 = fmul fast float %116, %128
  %131 = fmul fast float %119, %128
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %110, %_ZNK7vector3IfE7is_nullEv.exit.thread.i
  %132 = phi float [ %131, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %119, %110 ]
  %133 = phi float [ %130, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %116, %110 ]
  %134 = phi float [ %129, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %113, %110 ]
  %135 = getelementptr inbounds %"struct.std::__1::array.63"* addrspacecast (%"struct.std::__1::array.63" addrspace(4)* @_ZL18cornell_object_map to %"struct.std::__1::array.63"*), i64 0, i32 0, i64 %storemerge17
  %136 = load i32* %135, align 4, !tbaa !19
  br label %_ZNK7vector3IbE3anyIbvEEbv.exit.thread

_ZNK7vector3IbE3anyIbvEEbv.exit.thread:           ; preds = %70, %101, %_ZN10const_math3absIfvEET_S1_.exit, %_ZNK7vector3IfE10normalizedEv.exit, %_ZNK7vector3IbE3anyIbvEEbv.exit
  %137 = phi float [ %17, %_ZN10const_math3absIfvEET_S1_.exit ], [ %17, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %132, %_ZNK7vector3IfE10normalizedEv.exit ], [ %17, %101 ], [ %17, %70 ]
  %138 = phi float [ %16, %_ZN10const_math3absIfvEET_S1_.exit ], [ %16, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %133, %_ZNK7vector3IfE10normalizedEv.exit ], [ %16, %101 ], [ %16, %70 ]
  %139 = phi float [ %15, %_ZN10const_math3absIfvEET_S1_.exit ], [ %15, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %134, %_ZNK7vector3IfE10normalizedEv.exit ], [ %15, %101 ], [ %15, %70 ]
  %140 = phi float [ %14, %_ZN10const_math3absIfvEET_S1_.exit ], [ %14, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %107, %_ZNK7vector3IfE10normalizedEv.exit ], [ %14, %101 ], [ %14, %70 ]
  %object.1 = phi i32 [ %object.019, %_ZN10const_math3absIfvEET_S1_.exit ], [ %object.019, %_ZNK7vector3IbE3anyIbvEEbv.exit ], [ %136, %_ZNK7vector3IfE10normalizedEv.exit ], [ %object.019, %101 ], [ %object.019, %70 ]
  %141 = add i64 %storemerge17, 1
  %exitcond = icmp eq i64 %141, 32
  br i1 %exitcond, label %142, label %13

; <label>:142                                     ; preds = %_ZNK7vector3IbE3anyIbvEEbv.exit.thread
  %object.1.lcssa = phi i32 [ %object.1, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %.lcssa28 = phi float [ %140, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %.lcssa27 = phi float [ %139, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %.lcssa26 = phi float [ %138, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %.lcssa = phi float [ %137, %_ZNK7vector3IbE3anyIbvEEbv.exit.thread ]
  %143 = fmul fast float %.lcssa28, %6
  %144 = fmul fast float %.lcssa28, %2
  %145 = fmul fast float %.lcssa28, %4
  %146 = fadd fast float %143, %8
  %147 = fadd fast float %144, %10
  %148 = fadd fast float %145, %12
  %149 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 0
  store float %146, float* %149, align 4, !tbaa !6
  %150 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 1
  store float %147, float* %150, align 4, !tbaa !9
  %151 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 2
  store float %148, float* %151, align 4, !tbaa !10
  %152 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %.lcssa27, float* %152, align 4, !tbaa !6
  %153 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %.lcssa26, float* %153, align 4, !tbaa !9
  %154 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %.lcssa, float* %154, align 4, !tbaa !10
  %155 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 2
  store float %.lcssa28, float* %155, align 4, !tbaa !20
  %156 = getelementptr inbounds %"struct.simple_intersector::intersection"* %agg.result, i64 0, i32 3
  store i32 %object.1.lcssa, i32* %156, align 4, !tbaa !11
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer27compute_direct_illuminationERK7vector3IfERKN18simple_intersector12intersectionERK8materialRj(%class.vector3* noalias nocapture sret %agg.result, %class.vector3* nocapture readonly dereferenceable(12) %eye_direction, %"struct.simple_intersector::intersection"* nocapture readonly dereferenceable(32) %p, %struct.material* nocapture readonly dereferenceable(64) %mat, i32* nocapture dereferenceable(4) %random_seed) #3 align 2 {
  %r = alloca %struct.ray, align 4
  %ret = alloca %"struct.simple_intersector::intersection", align 4
  %1 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %1, align 4, !tbaa !6
  %2 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %2, align 4, !tbaa !9
  %3 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %3, align 4, !tbaa !10
  %4 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 0
  %5 = load float* %4, align 4, !tbaa !6
  %6 = fcmp oeq float %5, 0.000000e+00
  %7 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 1
  %8 = load float* %7, align 4, !tbaa !9
  %9 = fcmp oeq float %8, 0.000000e+00
  %or.cond.i4 = and i1 %6, %9
  %10 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 2
  %11 = load float* %10, align 4, !tbaa !10
  %12 = fcmp oeq float %11, 0.000000e+00
  %or.cond34 = and i1 %or.cond.i4, %12
  br i1 %or.cond34, label %_ZNK7vector3IfE10normalizedEv.exit7, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i6

_ZNK7vector3IfE7is_nullEv.exit.thread.i6:         ; preds = %0
  %13 = fmul fast float %5, %5
  %14 = fmul fast float %8, %8
  %15 = fadd fast float %13, %14
  %16 = fmul fast float %11, %11
  %17 = fadd fast float %15, %16
  %18 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %17) #4
  %19 = fmul fast float %5, %18
  %20 = fmul fast float %8, %18
  %21 = fmul fast float %11, %18
  br label %_ZNK7vector3IfE10normalizedEv.exit7

_ZNK7vector3IfE10normalizedEv.exit7:              ; preds = %0, %_ZNK7vector3IfE7is_nullEv.exit.thread.i6
  %22 = phi float [ %21, %_ZNK7vector3IfE7is_nullEv.exit.thread.i6 ], [ %11, %0 ]
  %23 = phi float [ %20, %_ZNK7vector3IfE7is_nullEv.exit.thread.i6 ], [ %8, %0 ]
  %24 = phi float [ %19, %_ZNK7vector3IfE7is_nullEv.exit.thread.i6 ], [ %5, %0 ]
  %25 = getelementptr inbounds %class.vector3* %eye_direction, i64 0, i32 0, i32 0, i32 0
  %26 = load float* %25, align 4, !tbaa !6
  %27 = fcmp oeq float %26, 0.000000e+00
  %28 = getelementptr inbounds %class.vector3* %eye_direction, i64 0, i32 0, i32 0, i32 1
  %29 = load float* %28, align 4, !tbaa !9
  %30 = fcmp oeq float %29, 0.000000e+00
  %or.cond.i13 = and i1 %27, %30
  %31 = getelementptr inbounds %class.vector3* %eye_direction, i64 0, i32 0, i32 0, i32 2
  %32 = load float* %31, align 4, !tbaa !10
  %33 = fcmp oeq float %32, 0.000000e+00
  %or.cond35 = and i1 %or.cond.i13, %33
  br i1 %or.cond35, label %_ZNK7vector3IfE10normalizedEv.exit16, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i15

_ZNK7vector3IfE7is_nullEv.exit.thread.i15:        ; preds = %_ZNK7vector3IfE10normalizedEv.exit7
  %34 = fmul fast float %26, %26
  %35 = fmul fast float %29, %29
  %36 = fadd fast float %34, %35
  %37 = fmul fast float %32, %32
  %38 = fadd fast float %36, %37
  %39 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %38) #4
  %40 = fmul fast float %26, %39
  %41 = fmul fast float %29, %39
  %42 = fmul fast float %32, %39
  br label %_ZNK7vector3IfE10normalizedEv.exit16

_ZNK7vector3IfE10normalizedEv.exit16:             ; preds = %_ZNK7vector3IfE10normalizedEv.exit7, %_ZNK7vector3IfE7is_nullEv.exit.thread.i15
  %43 = phi float [ %42, %_ZNK7vector3IfE7is_nullEv.exit.thread.i15 ], [ %32, %_ZNK7vector3IfE10normalizedEv.exit7 ]
  %44 = phi float [ %41, %_ZNK7vector3IfE7is_nullEv.exit.thread.i15 ], [ %29, %_ZNK7vector3IfE10normalizedEv.exit7 ]
  %45 = phi float [ %40, %_ZNK7vector3IfE7is_nullEv.exit.thread.i15 ], [ %26, %_ZNK7vector3IfE10normalizedEv.exit7 ]
  %46 = load i32* %random_seed, align 4, !tbaa !2
  %47 = mul i32 %46, 16807
  %48 = lshr i32 %47, 9
  %49 = or i32 %48, 1065353216
  %50 = bitcast i32 %49 to float
  %51 = fadd fast float %50, -1.000000e+00
  %52 = mul i32 %46, 282475249
  store i32 %52, i32* %random_seed, align 4, !tbaa !2
  %53 = lshr i32 %52, 9
  %54 = or i32 %53, 1065353216
  %55 = bitcast i32 %54 to float
  %56 = fadd fast float %55, -1.000000e+00
  %57 = fmul fast float %51, 1.050000e+01
  %58 = fadd fast float %57, 0x4036B33340000000
  %59 = fmul fast float %56, -1.300000e+01
  %60 = fadd fast float %59, 0x4041266660000000
  %61 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %62 = load float* %61, align 4, !tbaa !6
  %63 = fsub fast float %60, %62
  %64 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %65 = load float* %64, align 4, !tbaa !9
  %66 = fsub fast float 0x404B666660000000, %65
  %67 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %68 = load float* %67, align 4, !tbaa !10
  %69 = fsub fast float %58, %68
  %70 = fmul fast float %63, %63
  %71 = fmul fast float %66, %66
  %72 = fadd fast float %71, %70
  %73 = fmul fast float %69, %69
  %74 = fadd fast float %73, %72
  %75 = fcmp ogt float %74, 0x3F1A36E2E0000000
  %..i12 = select i1 %75, float %74, float 0x3F1A36E2E0000000
  %76 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 0
  store float %62, float* %76, align 4, !tbaa !6
  %77 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 1
  store float %65, float* %77, align 4, !tbaa !9
  %78 = getelementptr inbounds %struct.ray* %r, i64 0, i32 0, i32 0, i32 0, i32 2
  store float %68, float* %78, align 4, !tbaa !10
  %79 = fcmp oeq float %63, 0.000000e+00
  %80 = fcmp oeq float %66, 0.000000e+00
  %or.cond.i8 = and i1 %79, %80
  %81 = fcmp oeq float %69, 0.000000e+00
  %or.cond28 = and i1 %or.cond.i8, %81
  br i1 %or.cond28, label %_ZNK7vector3IfE10normalizedEv.exit11, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i10

_ZNK7vector3IfE7is_nullEv.exit.thread.i10:        ; preds = %_ZNK7vector3IfE10normalizedEv.exit16
  %82 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %74) #4
  %83 = fmul fast float %63, %82
  %84 = fmul fast float %66, %82
  %85 = fmul fast float %69, %82
  br label %_ZNK7vector3IfE10normalizedEv.exit11

_ZNK7vector3IfE10normalizedEv.exit11:             ; preds = %_ZNK7vector3IfE10normalizedEv.exit16, %_ZNK7vector3IfE7is_nullEv.exit.thread.i10
  %86 = phi float [ %83, %_ZNK7vector3IfE7is_nullEv.exit.thread.i10 ], [ %63, %_ZNK7vector3IfE10normalizedEv.exit16 ]
  %87 = phi float [ %84, %_ZNK7vector3IfE7is_nullEv.exit.thread.i10 ], [ %66, %_ZNK7vector3IfE10normalizedEv.exit16 ]
  %88 = phi float [ %85, %_ZNK7vector3IfE7is_nullEv.exit.thread.i10 ], [ %69, %_ZNK7vector3IfE10normalizedEv.exit16 ]
  %89 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %86, float* %89, align 4
  %90 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %87, float* %90, align 4
  %91 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %88, float* %91, align 4
  call void @_ZN18simple_intersector9intersectERK3ray(%"struct.simple_intersector::intersection"* sret %ret, %struct.ray* dereferenceable(24) %r)
  %92 = getelementptr inbounds %"struct.simple_intersector::intersection"* %ret, i64 0, i32 3
  %93 = load i32* %92, align 4, !tbaa !11
  %94 = icmp ugt i32 %93, 7
  br i1 %94, label %102, label %95

; <label>:95                                      ; preds = %_ZNK7vector3IfE10normalizedEv.exit11
  %96 = getelementptr inbounds %"struct.simple_intersector::intersection"* %ret, i64 0, i32 2
  %97 = load float* %96, align 4, !tbaa !20
  %98 = tail call float @llvm.nvvm.sqrt.rn.ftz.f(float %..i12) #4
  %99 = fadd fast float %98, 0xBF1A36E2E0000000
  %100 = fcmp oge float %97, %99
  %101 = icmp eq i32 %93, 1
  %or.cond = or i1 %100, %101
  br i1 %or.cond, label %102, label %192

; <label>:102                                     ; preds = %95, %_ZNK7vector3IfE10normalizedEv.exit11
  %103 = fdiv fast float 1.000000e+00, %..i12
  %104 = fcmp ogt float %87, 0.000000e+00
  %..i3 = select i1 %104, float %87, float 0.000000e+00
  %105 = fmul fast float %24, %86
  %106 = fmul fast float %23, %87
  %107 = fadd fast float %106, %105
  %108 = fmul fast float %22, %88
  %109 = fadd fast float %107, %108
  %110 = fcmp ogt float %109, 0.000000e+00
  %..i2 = select i1 %110, float %109, float 0.000000e+00
  %111 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 0
  %112 = load float* %111, align 4, !tbaa !6
  %113 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 1
  %114 = load float* %113, align 4, !tbaa !9
  %115 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 2
  %116 = load float* %115, align 4, !tbaa !10
  %117 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 0
  %118 = load float* %117, align 4, !tbaa !6
  %119 = fcmp oeq float %118, 0.000000e+00
  br i1 %119, label %120, label %_ZNK7vector3IfE7is_nullEv.exit.thread

; <label>:120                                     ; preds = %102
  %121 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 1
  %122 = load float* %121, align 4, !tbaa !9
  %123 = fcmp oeq float %122, 0.000000e+00
  br i1 %123, label %_ZNK7vector3IfE7is_nullEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread

_ZNK7vector3IfE7is_nullEv.exit:                   ; preds = %120
  %124 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 2
  %125 = load float* %124, align 4, !tbaa !10
  %126 = fcmp oeq float %125, 0.000000e+00
  br i1 %126, label %180, label %_ZNK7vector3IfE7is_nullEv.exit.thread

_ZNK7vector3IfE7is_nullEv.exit.thread:            ; preds = %102, %120, %_ZNK7vector3IfE7is_nullEv.exit
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
  %or.cond.i = and i1 %142, %143
  %144 = fcmp oeq float %141, 0.000000e+00
  %or.cond29 = and i1 %or.cond.i, %144
  br i1 %or.cond29, label %_ZNK7vector3IfE10normalizedEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread
  %145 = fmul fast float %139, %139
  %146 = fmul fast float %140, %140
  %147 = fadd fast float %145, %146
  %148 = fmul fast float %141, %141
  %149 = fadd fast float %148, %147
  %150 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %149) #4
  %151 = fmul fast float %139, %150
  %152 = fmul fast float %140, %150
  %153 = fmul fast float %141, %150
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread, %_ZNK7vector3IfE7is_nullEv.exit.thread.i
  %154 = phi float [ %153, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %141, %_ZNK7vector3IfE7is_nullEv.exit.thread ]
  %155 = phi float [ %152, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %140, %_ZNK7vector3IfE7is_nullEv.exit.thread ]
  %156 = phi float [ %151, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %139, %_ZNK7vector3IfE7is_nullEv.exit.thread ]
  %157 = fmul fast float %45, %156
  %158 = fmul fast float %44, %155
  %159 = fadd fast float %158, %157
  %160 = fmul fast float %43, %154
  %161 = fadd fast float %160, %159
  %162 = fcmp ogt float %161, 0.000000e+00
  %..i = select i1 %162, float %161, float 0.000000e+00
  %163 = getelementptr inbounds %struct.material* %mat, i64 0, i32 2
  %164 = load float* %163, align 4, !tbaa !21
  %165 = tail call float @llvm.nvvm.lg2.approx.ftz.f(float %..i) #4
  %166 = fmul fast float %164, %165
  %167 = tail call float @llvm.nvvm.ex2.approx.ftz.f(float %166) #4
  %168 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 0
  %169 = load float* %168, align 4, !tbaa !6
  %170 = fmul fast float %167, %169
  %171 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 1
  %172 = load float* %171, align 4, !tbaa !9
  %173 = fmul fast float %167, %172
  %174 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 2
  %175 = load float* %174, align 4, !tbaa !10
  %176 = fmul fast float %167, %175
  %177 = fadd fast float %112, %170
  %178 = fadd fast float %114, %173
  %179 = fadd fast float %116, %176
  br label %180

; <label>:180                                     ; preds = %_ZNK7vector3IfE10normalizedEv.exit, %_ZNK7vector3IfE7is_nullEv.exit
  %181 = phi float [ %179, %_ZNK7vector3IfE10normalizedEv.exit ], [ %116, %_ZNK7vector3IfE7is_nullEv.exit ]
  %182 = phi float [ %178, %_ZNK7vector3IfE10normalizedEv.exit ], [ %114, %_ZNK7vector3IfE7is_nullEv.exit ]
  %183 = phi float [ %177, %_ZNK7vector3IfE10normalizedEv.exit ], [ %112, %_ZNK7vector3IfE7is_nullEv.exit ]
  %184 = fmul fast float %..i3, %..i2
  %185 = tail call float @llvm.nvvm.sqrt.rz.ftz.f(float 1.863225e+04) #4
  %186 = fmul fast float %184, %185
  %187 = fmul fast float %103, %186
  %188 = fmul fast float %187, 1.500000e+01
  %189 = fmul fast float %183, %188
  %190 = fmul fast float %182, %188
  %191 = fmul fast float %181, %188
  store float %189, float* %1, align 4, !tbaa !6
  store float %190, float* %2, align 4, !tbaa !9
  store float %191, float* %3, align 4, !tbaa !10
  br label %192

; <label>:192                                     ; preds = %95, %180
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer29compute_indirect_illuminationILj0EEE7vector3IfERK3rayRKN18simple_intersector12intersectionERK8materialRj(%class.vector3* noalias nocapture sret %agg.result, %struct.ray* nocapture readonly dereferenceable(24) %r, %"struct.simple_intersector::intersection"* nocapture readonly dereferenceable(32) %p, %struct.material* nocapture readonly dereferenceable(64) %mat, i32* nocapture dereferenceable(4) %random_seed) #3 align 2 {
  %normal = alloca %class.vector3, align 4
  %tb = alloca %"struct.std::__1::pair", align 4
  %1 = alloca %class.vector3, align 4
  %2 = alloca %struct.ray, align 4
  %rnormal = alloca %class.vector3, align 4
  %tb1 = alloca %"struct.std::__1::pair", align 4
  %3 = alloca %class.vector3, align 4
  %4 = alloca %struct.ray, align 4
  %5 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 0
  %6 = load float* %5, align 4, !tbaa !6
  %7 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 0
  %8 = load float* %7, align 4, !tbaa !6
  %9 = fmul fast float %6, %8
  %10 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 1
  %11 = load float* %10, align 4, !tbaa !9
  %12 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 1
  %13 = load float* %12, align 4, !tbaa !9
  %14 = fmul fast float %11, %13
  %15 = fadd fast float %9, %14
  %16 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 1, i32 0, i32 0, i32 2
  %17 = load float* %16, align 4, !tbaa !10
  %18 = getelementptr inbounds %struct.ray* %r, i64 0, i32 1, i32 0, i32 0, i32 2
  %19 = load float* %18, align 4, !tbaa !10
  %20 = fmul fast float %17, %19
  %21 = fadd fast float %15, %20
  %22 = fcmp ogt float %21, 0.000000e+00
  br i1 %22, label %23, label %29

; <label>:23                                      ; preds = %0
  %24 = fsub fast float -0.000000e+00, %6
  %25 = fsub fast float -0.000000e+00, %11
  %26 = fsub fast float -0.000000e+00, %17
  %27 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %24, float* %27, align 4, !tbaa !6
  %28 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %25, float* %28, align 4, !tbaa !9
  br label %32

; <label>:29                                      ; preds = %0
  %30 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  store float %6, float* %30, align 4, !tbaa !6
  %31 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  store float %11, float* %31, align 4, !tbaa !9
  br label %32

; <label>:32                                      ; preds = %29, %23
  %33 = phi float [ %11, %29 ], [ %25, %23 ]
  %34 = phi float [ %6, %29 ], [ %24, %23 ]
  %35 = phi float [ %17, %29 ], [ %26, %23 ]
  %36 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 2
  store float %35, float* %36, align 4
  %37 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 0
  %38 = load float* %37, align 4, !tbaa !6
  %39 = fmul fast float %38, 0x3FCC6A7F00000000
  %40 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 1
  %41 = load float* %40, align 4, !tbaa !9
  %42 = fmul fast float %41, 0x3FE69D4960000000
  %43 = fadd fast float %39, %42
  %44 = getelementptr inbounds %struct.material* %mat, i64 0, i32 0, i32 0, i32 0, i32 2
  %45 = load float* %44, align 4, !tbaa !10
  %46 = fmul fast float %45, 0x3FB240B780000000
  %47 = fadd fast float %43, %46
  %48 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 0
  %49 = load float* %48, align 4, !tbaa !6
  %50 = fmul fast float %49, 0x3FCC6A7F00000000
  %51 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 1
  %52 = load float* %51, align 4, !tbaa !9
  %53 = fmul fast float %52, 0x3FE69D4960000000
  %54 = fadd fast float %50, %53
  %55 = getelementptr inbounds %struct.material* %mat, i64 0, i32 1, i32 0, i32 0, i32 2
  %56 = load float* %55, align 4, !tbaa !10
  %57 = fmul fast float %56, 0x3FB240B780000000
  %58 = fadd fast float %54, %57
  %59 = load i32* %random_seed, align 4, !tbaa !2
  %60 = mul i32 %59, 16807
  store i32 %60, i32* %random_seed, align 4, !tbaa !2
  %61 = lshr i32 %60, 9
  %62 = or i32 %61, 1065353216
  %63 = bitcast i32 %62 to float
  %64 = fadd fast float %63, -1.000000e+00
  %65 = fcmp olt float %64, %47
  br i1 %65, label %66, label %188

; <label>:66                                      ; preds = %32
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb, %class.vector3* dereferenceable(12) %normal)
  %67 = load i32* %random_seed, align 4, !tbaa !2
  %68 = mul i32 %67, 16807
  %69 = lshr i32 %68, 9
  %70 = or i32 %69, 1065353216
  %71 = bitcast i32 %70 to float
  %72 = fadd fast float %71, -1.000000e+00
  %73 = mul i32 %67, 282475249
  store i32 %73, i32* %random_seed, align 4, !tbaa !2
  %74 = lshr i32 %73, 9
  %75 = or i32 %74, 1065353216
  %76 = bitcast i32 %75 to float
  %77 = fadd fast float %76, -1.000000e+00
  %78 = fmul fast float %72, 0x401921FB60000000
  %79 = fsub fast float 2.000000e+00, %76
  %80 = tail call float @llvm.nvvm.sqrt.rn.ftz.f(float %79) #4
  %81 = tail call float @llvm.nvvm.sqrt.rn.ftz.f(float %77) #4
  %82 = tail call float @llvm.nvvm.cos.approx.ftz.f(float %78) #4
  %83 = fmul fast float %80, %82
  %84 = tail call float @llvm.nvvm.sin.approx.ftz.f(float %78) #4
  %85 = fmul fast float %80, %84
  %86 = fcmp oeq float %83, 0.000000e+00
  %87 = fcmp oeq float %81, 0.000000e+00
  %or.cond.i.i = and i1 %86, %87
  %88 = fcmp oeq float %85, 0.000000e+00
  %or.cond5.i = and i1 %or.cond.i.i, %88
  br i1 %or.cond5.i, label %_ZN7vector3IfE9normalizeEv.exit.i, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i.i:        ; preds = %66
  %89 = fmul fast float %83, %83
  %90 = fmul fast float %81, %81
  %91 = fadd fast float %90, %89
  %92 = fmul fast float %85, %85
  %93 = fadd fast float %91, %92
  %94 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %93) #4
  %95 = fmul fast float %83, %94
  %96 = fmul fast float %81, %94
  %97 = fmul fast float %85, %94
  br label %_ZN7vector3IfE9normalizeEv.exit.i

_ZN7vector3IfE9normalizeEv.exit.i:                ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i, %66
  %98 = phi float [ %97, %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i ], [ %85, %66 ]
  %99 = phi float [ %96, %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i ], [ %81, %66 ]
  %100 = phi float [ %95, %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i ], [ %83, %66 ]
  %101 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 0, i32 0, i32 0, i32 0
  %102 = load float* %101, align 4, !tbaa !23
  %103 = fmul fast float %100, %102
  %104 = fmul fast float %99, %34
  %105 = fadd fast float %103, %104
  %106 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 1, i32 0, i32 0, i32 0
  %107 = load float* %106, align 4, !tbaa !23
  %108 = fmul fast float %98, %107
  %109 = fadd fast float %105, %108
  %110 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 0, i32 0, i32 0, i32 1
  %111 = load float* %110, align 4, !tbaa !23
  %112 = fmul fast float %100, %111
  %113 = fmul fast float %99, %33
  %114 = fadd fast float %112, %113
  %115 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 1, i32 0, i32 0, i32 1
  %116 = load float* %115, align 4, !tbaa !23
  %117 = fmul fast float %98, %116
  %118 = fadd fast float %114, %117
  %119 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 0, i32 0, i32 0, i32 2
  %120 = load float* %119, align 4, !tbaa !23
  %121 = fmul fast float %100, %120
  %122 = fmul fast float %99, %35
  %123 = fadd fast float %121, %122
  %124 = getelementptr inbounds %"struct.std::__1::pair"* %tb, i64 0, i32 1, i32 0, i32 0, i32 2
  %125 = load float* %124, align 4, !tbaa !23
  %126 = fmul fast float %98, %125
  %127 = fadd fast float %123, %126
  %128 = fcmp oeq float %109, 0.000000e+00
  %129 = fcmp oeq float %118, 0.000000e+00
  %or.cond.i1.i = and i1 %128, %129
  %130 = fcmp oeq float %127, 0.000000e+00
  %or.cond.i6 = and i1 %or.cond.i1.i, %130
  br i1 %or.cond.i6, label %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i:       ; preds = %_ZN7vector3IfE9normalizeEv.exit.i
  %131 = fmul fast float %109, %109
  %132 = fmul fast float %118, %118
  %133 = fadd fast float %131, %132
  %134 = fmul fast float %127, %127
  %135 = fadd fast float %133, %134
  %136 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %135) #4
  %137 = fmul fast float %109, %136
  %138 = fmul fast float %118, %136
  %139 = fmul fast float %127, %136
  br label %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit

_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit: ; preds = %_ZN7vector3IfE9normalizeEv.exit.i, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i
  %140 = phi float [ %127, %_ZN7vector3IfE9normalizeEv.exit.i ], [ %139, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i ]
  %141 = phi float [ %118, %_ZN7vector3IfE9normalizeEv.exit.i ], [ %138, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i ]
  %142 = phi float [ %109, %_ZN7vector3IfE9normalizeEv.exit.i ], [ %137, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i ]
  %143 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 0
  %144 = load float* %143, align 4, !tbaa !6
  %145 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 1
  %146 = load float* %145, align 4, !tbaa !9
  %147 = getelementptr inbounds %struct.material* %mat, i64 0, i32 4, i32 0, i32 0, i32 2
  %148 = load float* %147, align 4, !tbaa !10
  %149 = fmul fast float %142, %34
  %150 = fmul fast float %141, %33
  %151 = fadd fast float %149, %150
  %152 = fmul fast float %140, %35
  %153 = fadd fast float %151, %152
  %154 = fcmp ogt float %153, 0.000000e+00
  %..i = select i1 %154, float %153, float 0.000000e+00
  %155 = fmul fast float %144, %..i
  %156 = fmul fast float %146, %..i
  %157 = fmul fast float %148, %..i
  %158 = fcmp ogt float %155, 0.000000e+00
  %159 = fcmp ogt float %156, 0.000000e+00
  %or.cond = or i1 %158, %159
  %160 = fcmp ogt float %157, 0.000000e+00
  %or.cond25 = or i1 %or.cond, %160
  br i1 %or.cond25, label %161, label %371

; <label>:161                                     ; preds = %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit
  %162 = fmul fast float %81, 0x3FD45F3060000000
  %163 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 0
  %164 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %165 = load float* %164, align 4, !tbaa !6
  store float %165, float* %163, align 4, !tbaa !6
  %166 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 1
  %167 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %168 = load float* %167, align 4, !tbaa !9
  store float %168, float* %166, align 4, !tbaa !9
  %169 = getelementptr inbounds %struct.ray* %2, i64 0, i32 0, i32 0, i32 0, i32 2
  %170 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %171 = load float* %170, align 4, !tbaa !10
  store float %171, float* %169, align 4, !tbaa !10
  %172 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %142, float* %172, align 4, !tbaa !6
  %173 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %141, float* %173, align 4, !tbaa !9
  %174 = getelementptr inbounds %struct.ray* %2, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %140, float* %174, align 4, !tbaa !10
  call void @_ZN18simple_path_tracer16compute_radianceILj0ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %1, %struct.ray* dereferenceable(24) %2, i32* dereferenceable(4) %random_seed, i1 zeroext false)
  %175 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 0
  %176 = load float* %175, align 4, !tbaa !6
  %177 = fmul fast float %155, %176
  %178 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 1
  %179 = load float* %178, align 4, !tbaa !9
  %180 = fmul fast float %156, %179
  %181 = getelementptr inbounds %class.vector3* %1, i64 0, i32 0, i32 0, i32 2
  %182 = load float* %181, align 4, !tbaa !10
  %183 = fmul fast float %157, %182
  %184 = fmul fast float %47, %162
  %185 = fdiv fast float %177, %184
  %186 = fdiv fast float %180, %184
  %187 = fdiv fast float %183, %184
  br label %371

; <label>:188                                     ; preds = %32
  %189 = fadd fast float %47, %58
  %190 = fcmp olt float %64, %189
  br i1 %190, label %191, label %371

; <label>:191                                     ; preds = %188
  %192 = fmul fast float %34, %8
  %193 = fmul fast float %33, %13
  %194 = fadd fast float %192, %193
  %195 = fmul fast float %35, %19
  %196 = fadd fast float %194, %195
  %197 = fmul fast float %196, 2.000000e+00
  %198 = fmul fast float %34, %197
  %199 = fmul fast float %33, %197
  %200 = fmul fast float %35, %197
  %201 = fsub fast float %8, %198
  %202 = fsub fast float %13, %199
  %203 = fsub fast float %19, %200
  %204 = fcmp oeq float %201, 0.000000e+00
  %205 = fcmp oeq float %202, 0.000000e+00
  %or.cond.i = and i1 %204, %205
  %206 = fcmp oeq float %203, 0.000000e+00
  %or.cond26 = and i1 %or.cond.i, %206
  br i1 %or.cond26, label %_ZNK7vector3IfE10normalizedEv.exit, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %191
  %207 = fmul fast float %201, %201
  %208 = fmul fast float %202, %202
  %209 = fadd fast float %207, %208
  %210 = fmul fast float %203, %203
  %211 = fadd fast float %210, %209
  %212 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %211) #4
  %213 = fmul fast float %201, %212
  %214 = fmul fast float %202, %212
  %215 = fmul fast float %203, %212
  br label %_ZNK7vector3IfE10normalizedEv.exit

_ZNK7vector3IfE10normalizedEv.exit:               ; preds = %191, %_ZNK7vector3IfE7is_nullEv.exit.thread.i
  %216 = phi float [ %213, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %201, %191 ]
  %217 = phi float [ %214, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %202, %191 ]
  %218 = phi float [ %215, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %203, %191 ]
  %219 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 0
  store float %216, float* %219, align 4
  %220 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 1
  store float %217, float* %220, align 4
  %221 = getelementptr inbounds %class.vector3* %rnormal, i64 0, i32 0, i32 0, i32 2
  store float %218, float* %221, align 4
  call void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* sret %tb1, %class.vector3* dereferenceable(12) %rnormal)
  %222 = load i32* %random_seed, align 4, !tbaa !2
  %223 = mul i32 %222, 16807
  %224 = lshr i32 %223, 9
  %225 = or i32 %224, 1065353216
  %226 = bitcast i32 %225 to float
  %227 = fadd fast float %226, -1.000000e+00
  %228 = mul i32 %222, 282475249
  store i32 %228, i32* %random_seed, align 4, !tbaa !2
  %229 = lshr i32 %228, 9
  %230 = or i32 %229, 1065353216
  %231 = bitcast i32 %230 to float
  %232 = fadd fast float %231, -1.000000e+00
  %233 = getelementptr inbounds %struct.material* %mat, i64 0, i32 2
  %234 = load float* %233, align 4, !tbaa !21
  %235 = fmul fast float %227, 0x401921FB60000000
  %236 = fadd fast float %234, 1.000000e+00
  %237 = fdiv fast float 1.000000e+00, %236
  %238 = tail call float @llvm.nvvm.lg2.approx.ftz.f(float %232) #4
  %239 = fmul fast float %237, %238
  %240 = tail call float @llvm.nvvm.ex2.approx.ftz.f(float %239) #4
  %241 = fmul fast float %240, %240
  %242 = fsub fast float 1.000000e+00, %241
  %243 = tail call float @llvm.nvvm.sqrt.rn.ftz.f(float %242) #4
  %244 = tail call float @llvm.nvvm.cos.approx.ftz.f(float %235) #4
  %245 = fmul fast float %243, %244
  %246 = tail call float @llvm.nvvm.sin.approx.ftz.f(float %235) #4
  %247 = fmul fast float %243, %246
  %248 = fcmp oeq float %245, 0.000000e+00
  %249 = fcmp oeq float %240, 0.000000e+00
  %or.cond.i.i7 = and i1 %248, %249
  %250 = fcmp oeq float %247, 0.000000e+00
  %or.cond5.i8 = and i1 %or.cond.i.i7, %250
  br i1 %or.cond5.i8, label %_ZN7vector3IfE9normalizeEv.exit.i12, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i9

_ZNK7vector3IfE7is_nullEv.exit.thread.i.i9:       ; preds = %_ZNK7vector3IfE10normalizedEv.exit
  %251 = fmul fast float %245, %245
  %252 = fadd fast float %241, %251
  %253 = fmul fast float %247, %247
  %254 = fadd fast float %252, %253
  %255 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %254) #4
  %256 = fmul fast float %245, %255
  %257 = fmul fast float %240, %255
  %258 = fmul fast float %247, %255
  br label %_ZN7vector3IfE9normalizeEv.exit.i12

_ZN7vector3IfE9normalizeEv.exit.i12:              ; preds = %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i9, %_ZNK7vector3IfE10normalizedEv.exit
  %259 = phi float [ %258, %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i9 ], [ %247, %_ZNK7vector3IfE10normalizedEv.exit ]
  %260 = phi float [ %257, %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i9 ], [ %240, %_ZNK7vector3IfE10normalizedEv.exit ]
  %261 = phi float [ %256, %_ZNK7vector3IfE7is_nullEv.exit.thread.i.i9 ], [ %245, %_ZNK7vector3IfE10normalizedEv.exit ]
  %262 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 0, i32 0, i32 0, i32 0
  %263 = load float* %262, align 4, !tbaa !23
  %264 = fmul fast float %261, %263
  %265 = fmul fast float %260, %216
  %266 = fadd fast float %264, %265
  %267 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 1, i32 0, i32 0, i32 0
  %268 = load float* %267, align 4, !tbaa !23
  %269 = fmul fast float %259, %268
  %270 = fadd fast float %266, %269
  %271 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 0, i32 0, i32 0, i32 1
  %272 = load float* %271, align 4, !tbaa !23
  %273 = fmul fast float %261, %272
  %274 = fmul fast float %260, %217
  %275 = fadd fast float %273, %274
  %276 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 1, i32 0, i32 0, i32 1
  %277 = load float* %276, align 4, !tbaa !23
  %278 = fmul fast float %259, %277
  %279 = fadd fast float %275, %278
  %280 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 0, i32 0, i32 0, i32 2
  %281 = load float* %280, align 4, !tbaa !23
  %282 = fmul fast float %261, %281
  %283 = fmul fast float %260, %218
  %284 = fadd fast float %282, %283
  %285 = getelementptr inbounds %"struct.std::__1::pair"* %tb1, i64 0, i32 1, i32 0, i32 0, i32 2
  %286 = load float* %285, align 4, !tbaa !23
  %287 = fmul fast float %259, %286
  %288 = fadd fast float %284, %287
  %289 = fcmp oeq float %270, 0.000000e+00
  %290 = fcmp oeq float %279, 0.000000e+00
  %or.cond.i1.i10 = and i1 %289, %290
  %291 = fcmp oeq float %288, 0.000000e+00
  %or.cond.i11 = and i1 %or.cond.i1.i10, %291
  br i1 %or.cond.i11, label %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit14, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i13

_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i13:     ; preds = %_ZN7vector3IfE9normalizeEv.exit.i12
  %292 = fmul fast float %270, %270
  %293 = fmul fast float %279, %279
  %294 = fadd fast float %292, %293
  %295 = fmul fast float %288, %288
  %296 = fadd fast float %294, %295
  %297 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %296) #4
  %298 = fmul fast float %270, %297
  %299 = fmul fast float %279, %297
  %300 = fmul fast float %288, %297
  br label %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit14

_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit14: ; preds = %_ZN7vector3IfE9normalizeEv.exit.i12, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i13
  %301 = phi float [ %288, %_ZN7vector3IfE9normalizeEv.exit.i12 ], [ %300, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i13 ]
  %302 = phi float [ %279, %_ZN7vector3IfE9normalizeEv.exit.i12 ], [ %299, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i13 ]
  %303 = phi float [ %270, %_ZN7vector3IfE9normalizeEv.exit.i12 ], [ %298, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3.i13 ]
  %304 = tail call float @llvm.nvvm.lg2.approx.ftz.f(float %240) #4
  %305 = fmul fast float %234, %304
  %306 = tail call float @llvm.nvvm.ex2.approx.ftz.f(float %305) #4
  %307 = fmul fast float %236, %306
  %308 = fmul fast float %307, 0x3FC45F3060000000
  %309 = fmul fast float %303, %34
  %310 = fmul fast float %302, %33
  %311 = fadd fast float %309, %310
  %312 = fmul fast float %301, %35
  %313 = fadd fast float %311, %312
  %314 = fcmp ugt float %313, 0.000000e+00
  br i1 %314, label %319, label %315

; <label>:315                                     ; preds = %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit14
  %316 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %316, align 4, !tbaa !6
  %317 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %317, align 4, !tbaa !9
  %318 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float 0.000000e+00, float* %318, align 4, !tbaa !10
  br label %378

; <label>:319                                     ; preds = %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit14
  %320 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 0
  %321 = load float* %320, align 4, !tbaa !6
  %322 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 1
  %323 = load float* %322, align 4, !tbaa !9
  %324 = getelementptr inbounds %struct.material* %mat, i64 0, i32 5, i32 0, i32 0, i32 2
  %325 = load float* %324, align 4, !tbaa !10
  %326 = fmul fast float %303, %216
  %327 = fmul fast float %302, %217
  %328 = fadd fast float %326, %327
  %329 = fmul fast float %301, %218
  %330 = fadd fast float %328, %329
  %331 = fcmp ogt float %330, 0.000000e+00
  %..i4 = select i1 %331, float %330, float 0.000000e+00
  %332 = tail call float @llvm.nvvm.lg2.approx.ftz.f(float %..i4) #4
  %333 = fmul fast float %234, %332
  %334 = tail call float @llvm.nvvm.ex2.approx.ftz.f(float %333) #4
  %335 = fmul fast float %321, %334
  %336 = fmul fast float %323, %334
  %337 = fmul fast float %325, %334
  %338 = fcmp ogt float %313, 0.000000e+00
  %..i5 = select i1 %338, float %313, float 0.000000e+00
  %339 = fmul fast float %..i5, %335
  %340 = fmul fast float %..i5, %336
  %341 = fmul fast float %..i5, %337
  %342 = fcmp ogt float %339, 0.000000e+00
  %343 = fcmp ogt float %340, 0.000000e+00
  %or.cond27 = or i1 %342, %343
  %344 = fcmp ogt float %341, 0.000000e+00
  %or.cond28 = or i1 %or.cond27, %344
  br i1 %or.cond28, label %345, label %371

; <label>:345                                     ; preds = %319
  %346 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 0
  %347 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 0
  %348 = load float* %347, align 4, !tbaa !6
  store float %348, float* %346, align 4, !tbaa !6
  %349 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 1
  %350 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 1
  %351 = load float* %350, align 4, !tbaa !9
  store float %351, float* %349, align 4, !tbaa !9
  %352 = getelementptr inbounds %struct.ray* %4, i64 0, i32 0, i32 0, i32 0, i32 2
  %353 = getelementptr inbounds %"struct.simple_intersector::intersection"* %p, i64 0, i32 0, i32 0, i32 0, i32 2
  %354 = load float* %353, align 4, !tbaa !10
  store float %354, float* %352, align 4, !tbaa !10
  %355 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %303, float* %355, align 4, !tbaa !6
  %356 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %302, float* %356, align 4, !tbaa !9
  %357 = getelementptr inbounds %struct.ray* %4, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %301, float* %357, align 4, !tbaa !10
  call void @_ZN18simple_path_tracer16compute_radianceILj0ELi0EEE7vector3IfERK3rayRjb(%class.vector3* sret %3, %struct.ray* dereferenceable(24) %4, i32* dereferenceable(4) %random_seed, i1 zeroext false)
  %358 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 0
  %359 = load float* %358, align 4, !tbaa !6
  %360 = fmul fast float %339, %359
  %361 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 1
  %362 = load float* %361, align 4, !tbaa !9
  %363 = fmul fast float %340, %362
  %364 = getelementptr inbounds %class.vector3* %3, i64 0, i32 0, i32 0, i32 2
  %365 = load float* %364, align 4, !tbaa !10
  %366 = fmul fast float %341, %365
  %367 = fmul fast float %58, %308
  %368 = fdiv fast float %360, %367
  %369 = fdiv fast float %363, %367
  %370 = fdiv fast float %366, %367
  br label %371

; <label>:371                                     ; preds = %319, %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit, %188, %345, %161
  %372 = phi float [ 0.000000e+00, %188 ], [ %370, %345 ], [ %187, %161 ], [ %157, %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit ], [ %341, %319 ]
  %373 = phi float [ 0.000000e+00, %188 ], [ %369, %345 ], [ %186, %161 ], [ %156, %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit ], [ %340, %319 ]
  %374 = phi float [ 0.000000e+00, %188 ], [ %368, %345 ], [ %185, %161 ], [ %155, %_ZN18simple_path_tracer13transform_dirER7vector3IfERKS1_S4_S4_.exit ], [ %339, %319 ]
  %375 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 0
  store float %374, float* %375, align 4, !tbaa !6
  %376 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 1
  store float %373, float* %376, align 4, !tbaa !9
  %377 = getelementptr inbounds %class.vector3* %agg.result, i64 0, i32 0, i32 0, i32 2
  store float %372, float* %377, align 4, !tbaa !10
  br label %378

; <label>:378                                     ; preds = %371, %315
  ret void
}

; Function Attrs: nounwind
define linkonce_odr void @_ZN18simple_path_tracer16get_local_systemERK7vector3IfE(%"struct.std::__1::pair"* noalias nocapture sret %agg.result, %class.vector3* nocapture readonly dereferenceable(12) %normal) #3 align 2 {
  %tangent = alloca %class.vector3, align 4
  %1 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 0
  %2 = load float* %1, align 4, !tbaa !6
  %3 = fcmp olt float %2, 0.000000e+00
  br i1 %3, label %4, label %_ZN10const_math3absIfvEET_S1_.exit

; <label>:4                                       ; preds = %0
  %5 = fsub fast float -0.000000e+00, %2
  br label %_ZN10const_math3absIfvEET_S1_.exit

_ZN10const_math3absIfvEET_S1_.exit:               ; preds = %0, %4
  %6 = phi float [ %5, %4 ], [ %2, %0 ]
  %7 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 1
  %8 = load float* %7, align 4, !tbaa !9
  %9 = fcmp olt float %8, 0.000000e+00
  br i1 %9, label %10, label %_ZN10const_math3absIfvEET_S1_.exit9

; <label>:10                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit
  %11 = fsub fast float -0.000000e+00, %8
  br label %_ZN10const_math3absIfvEET_S1_.exit9

_ZN10const_math3absIfvEET_S1_.exit9:              ; preds = %_ZN10const_math3absIfvEET_S1_.exit, %10
  %12 = phi float [ %11, %10 ], [ %8, %_ZN10const_math3absIfvEET_S1_.exit ]
  %not. = fcmp ule float %6, %12
  %13 = zext i1 %not. to i64
  br i1 %3, label %14, label %_ZN10const_math3absIfvEET_S1_.exit8

; <label>:14                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit9
  %15 = fsub fast float -0.000000e+00, %2
  br label %_ZN10const_math3absIfvEET_S1_.exit8

_ZN10const_math3absIfvEET_S1_.exit8:              ; preds = %_ZN10const_math3absIfvEET_S1_.exit9, %14
  %16 = phi float [ %15, %14 ], [ %2, %_ZN10const_math3absIfvEET_S1_.exit9 ]
  br i1 %9, label %17, label %_ZN10const_math3absIfvEET_S1_.exit7

; <label>:17                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit8
  %18 = fsub fast float -0.000000e+00, %8
  br label %_ZN10const_math3absIfvEET_S1_.exit7

_ZN10const_math3absIfvEET_S1_.exit7:              ; preds = %_ZN10const_math3absIfvEET_S1_.exit8, %17
  %19 = phi float [ %18, %17 ], [ %8, %_ZN10const_math3absIfvEET_S1_.exit8 ]
  %20 = fcmp ogt float %16, %19
  %21 = zext i1 %20 to i64
  br i1 %3, label %22, label %_ZN10const_math3absIfvEET_S1_.exit6

; <label>:22                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit7
  %23 = fsub fast float -0.000000e+00, %2
  br label %_ZN10const_math3absIfvEET_S1_.exit6

_ZN10const_math3absIfvEET_S1_.exit6:              ; preds = %_ZN10const_math3absIfvEET_S1_.exit7, %22
  %24 = phi float [ %23, %22 ], [ %2, %_ZN10const_math3absIfvEET_S1_.exit7 ]
  br i1 %9, label %25, label %_ZN10const_math3absIfvEET_S1_.exit5

; <label>:25                                      ; preds = %_ZN10const_math3absIfvEET_S1_.exit6
  %26 = fsub fast float -0.000000e+00, %8
  br label %_ZN10const_math3absIfvEET_S1_.exit5

_ZN10const_math3absIfvEET_S1_.exit5:              ; preds = %_ZN10const_math3absIfvEET_S1_.exit6, %25
  %27 = phi float [ %26, %25 ], [ %8, %_ZN10const_math3absIfvEET_S1_.exit6 ]
  %28 = fcmp ogt float %24, %27
  %29 = select i1 %28, float -1.000000e+00, float 1.000000e+00
  %30 = getelementptr inbounds float* %1, i64 %13
  %31 = load float* %30, align 4, !tbaa !23
  %32 = fmul fast float %31, %31
  %33 = getelementptr inbounds %class.vector3* %normal, i64 0, i32 0, i32 0, i32 2
  %34 = load float* %33, align 4, !tbaa !10
  %35 = fmul fast float %34, %34
  %36 = fadd fast float %32, %35
  %37 = fdiv fast float 1.000000e+00, %36
  %38 = getelementptr inbounds %class.vector3* %tangent, i64 0, i32 0, i32 0, i32 0
  store float 0.000000e+00, float* %38, align 4, !tbaa !6
  %39 = getelementptr inbounds %class.vector3* %tangent, i64 0, i32 0, i32 0, i32 1
  store float 0.000000e+00, float* %39, align 4, !tbaa !9
  %40 = getelementptr inbounds %class.vector3* %tangent, i64 0, i32 0, i32 0, i32 2
  %41 = fmul fast float %29, %34
  %42 = fmul fast float %37, %41
  %43 = getelementptr inbounds float* %38, i64 %13
  store float %42, float* %43, align 4, !tbaa !23
  %44 = getelementptr inbounds float* %38, i64 %21
  store float 0.000000e+00, float* %44, align 4, !tbaa !23
  %45 = fmul fast float %29, %31
  %46 = fmul fast float %37, %45
  %47 = fsub fast float -0.000000e+00, %46
  store float %47, float* %40, align 4, !tbaa !10
  %48 = load float* %38, align 4, !tbaa !6
  %49 = fcmp oeq float %48, 0.000000e+00
  %50 = load float* %39, align 4, !tbaa !9
  %51 = fcmp oeq float %50, 0.000000e+00
  %or.cond.i1 = and i1 %49, %51
  %52 = fcmp oeq float %46, -0.000000e+00
  %or.cond = and i1 %or.cond.i1, %52
  br i1 %or.cond, label %_ZN7vector3IfE9normalizeEv.exit4, label %_ZNK7vector3IfE7is_nullEv.exit.thread.i3

_ZNK7vector3IfE7is_nullEv.exit.thread.i3:         ; preds = %_ZN10const_math3absIfvEET_S1_.exit5
  %53 = fmul fast float %48, %48
  %54 = fmul fast float %50, %50
  %55 = fadd fast float %53, %54
  %56 = fmul fast float %46, %46
  %57 = fadd fast float %55, %56
  %58 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %57) #4
  %59 = fmul fast float %48, %58
  store float %59, float* %38, align 4, !tbaa !6
  %60 = fmul fast float %50, %58
  store float %60, float* %39, align 4, !tbaa !9
  %61 = fmul fast float %58, %47
  store float %61, float* %40, align 4, !tbaa !10
  br label %_ZN7vector3IfE9normalizeEv.exit4

_ZN7vector3IfE9normalizeEv.exit4:                 ; preds = %_ZN10const_math3absIfvEET_S1_.exit5, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3
  %62 = phi float [ %48, %_ZN10const_math3absIfvEET_S1_.exit5 ], [ %59, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3 ]
  %63 = phi float [ %50, %_ZN10const_math3absIfvEET_S1_.exit5 ], [ %60, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3 ]
  %64 = phi float [ %47, %_ZN10const_math3absIfvEET_S1_.exit5 ], [ %61, %_ZNK7vector3IfE7is_nullEv.exit.thread.i3 ]
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

_ZNK7vector3IfE7is_nullEv.exit.thread.i:          ; preds = %_ZN7vector3IfE9normalizeEv.exit4
  %77 = fmul fast float %67, %67
  %78 = fmul fast float %70, %70
  %79 = fadd fast float %77, %78
  %80 = fmul fast float %73, %73
  %81 = fadd fast float %80, %79
  %82 = tail call float @llvm.nvvm.rsqrt.approx.ftz.f(float %81) #4
  %83 = fmul fast float %67, %82
  %84 = fmul fast float %70, %82
  %85 = fmul fast float %73, %82
  br label %_ZN7vector3IfE9normalizeEv.exit

_ZN7vector3IfE9normalizeEv.exit:                  ; preds = %_ZN7vector3IfE9normalizeEv.exit4, %_ZNK7vector3IfE7is_nullEv.exit.thread.i
  %86 = phi float [ %85, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %73, %_ZN7vector3IfE9normalizeEv.exit4 ]
  %87 = phi float [ %84, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %70, %_ZN7vector3IfE9normalizeEv.exit4 ]
  %88 = phi float [ %83, %_ZNK7vector3IfE7is_nullEv.exit.thread.i ], [ %67, %_ZN7vector3IfE9normalizeEv.exit4 ]
  %89 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 0
  store float %62, float* %89, align 4, !tbaa !6
  %90 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 1
  store float %63, float* %90, align 4, !tbaa !9
  %91 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 0, i32 0, i32 0, i32 2
  store float %64, float* %91, align 4, !tbaa !10
  %92 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 0
  store float %88, float* %92, align 4, !tbaa !6
  %93 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 1
  store float %87, float* %93, align 4, !tbaa !9
  %94 = getelementptr inbounds %"struct.std::__1::pair"* %agg.result, i64 0, i32 1, i32 0, i32 0, i32 2
  store float %86, float* %94, align 4, !tbaa !10
  ret void
}

attributes #0 = { nounwind readnone "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #1 = { nounwind readnone }
attributes #2 = { alwaysinline nounwind readnone "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #3 = { nounwind "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="true" "no-nans-fp-math"="true" "stack-protector-buffer-size"="8" "unsafe-fp-math"="true" "use-soft-float"="false" }
attributes #4 = { nounwind }

!nvvm.annotations = !{!0}
!llvm.ident = !{!1}

!0 = metadata !{void (%class.vector3*, i32, i32, %union.anon.24)* @path_trace, metadata !"kernel", i32 1}
!1 = metadata !{metadata !"clang version 3.5.0 (tags/RELEASE_350/final)"}
!2 = metadata !{metadata !3, metadata !3, i64 0}
!3 = metadata !{metadata !"int", metadata !4, i64 0}
!4 = metadata !{metadata !"omnipotent char", metadata !5, i64 0}
!5 = metadata !{metadata !"Simple C/C++ TBAA"}
!6 = metadata !{metadata !7, metadata !8, i64 0}
!7 = metadata !{metadata !"_ZTSN7vector3IfEUt_Ut_E", metadata !8, i64 0, metadata !8, i64 4, metadata !8, i64 8}
!8 = metadata !{metadata !"float", metadata !4, i64 0}
!9 = metadata !{metadata !7, metadata !8, i64 4}
!10 = metadata !{metadata !7, metadata !8, i64 8}
!11 = metadata !{metadata !12, metadata !14, i64 28}
!12 = metadata !{metadata !"_ZTSN18simple_intersector12intersectionE", metadata !13, i64 0, metadata !13, i64 12, metadata !8, i64 24, metadata !14, i64 28}
!13 = metadata !{metadata !"_ZTS7vector3IfE", metadata !4, i64 0}
!14 = metadata !{metadata !"_ZTS14CORNELL_OBJECT", metadata !4, i64 0}
!15 = metadata !{metadata !16, metadata !3, i64 0}
!16 = metadata !{metadata !"_ZTSN7vector3IjEUt_Ut_E", metadata !3, i64 0, metadata !3, i64 4, metadata !3, i64 8}
!17 = metadata !{metadata !16, metadata !3, i64 4}
!18 = metadata !{metadata !16, metadata !3, i64 8}
!19 = metadata !{metadata !14, metadata !14, i64 0}
!20 = metadata !{metadata !12, metadata !8, i64 24}
!21 = metadata !{metadata !22, metadata !8, i64 24}
!22 = metadata !{metadata !"_ZTS8material", metadata !13, i64 0, metadata !13, i64 12, metadata !8, i64 24, metadata !13, i64 28, metadata !13, i64 40, metadata !13, i64 52}
!23 = metadata !{metadata !8, metadata !8, i64 0}
