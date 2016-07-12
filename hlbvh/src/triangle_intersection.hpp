/*
 *  original source code (C) 1997 - 1999 Tomas Akenine-Möller: http://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/code/opttritri.txt
 *  C++14 adoption + compute simplification (C) 2016 Florian Ziesche
 *
 *  Notes:
 *   * this gets rid of any dynamic indexing to avoid stack allocations
 *   * coplanar triangle intersection has been removed for this reason (and unwanted in this use case)
 *  
 *  This is free and unencumbered software released into the public domain.
 *  
 *  Anyone is free to copy, modify, publish, use, compile, sell, or
 *  distribute this software, either in source code form or as a compiled
 *  binary, for any purpose, commercial or non-commercial, and by any
 *  means.
 *  
 *  In jurisdictions that recognize copyright laws, the author or authors
 *  of this software dedicate any and all copyright interest in the
 *  software to the public domain. We make this dedication for the benefit
 *  of the public at large and to the detriment of our heirs and
 *  successors. We intend this dedication to be an overt act of
 *  relinquishment in perpetuity of all present and future rights to this
 *  software under copyright law.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 *  OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 *  ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *  OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __FLOOR_HLBVH_TRIANGLE_INTERSECTION_HPP__
#define __FLOOR_HLBVH_TRIANGLE_INTERSECTION_HPP__

static bool check_triangle_intersection(const float3 v0_a, const float3 v1_a, const float3 v2_a,
										const float3 v0_b, const float3 v1_b, const float3 v2_b) {
	const auto compute_intervals = [](const float& VV0, const float& VV1, const float& VV2,
									  const float& D0, const float& D1, const float& D2,
									  const float& D0D1, const float& D0D2,
									  float& A, float& B, float& C,
									  float& X0, float& X1) {
		if(D0D1 > 0.0f) {
			// here we know that D0D2 <= 0.0
			// that is D0, D1 are on the same side, D2 on the other or on the plane
			A = VV2;
			B = (VV0 - VV2) * D2;
			C = (VV1 - VV2) * D2;
			X0 = D2 - D0;
			X1 = D2 - D1;
		}
		else if(D0D2 > 0.0f) {
			// here we know that d0d1 <= 0.0
			A = VV1;
			B = (VV0 - VV1) * D1;
			C = (VV2 - VV1) * D1;
			X0 = D1 - D0;
			X1 = D1 - D2;
		}
		else if(D1 * D2 > 0.0f || D0 != 0.0f) {
			// here we know that d0d1 <= 0.0 or that D0 != 0.0
			A = VV0;
			B = (VV1 - VV0) * D0;
			C = (VV2 - VV0) * D0;
			X0 = D0 - D1;
			X1 = D0 - D2;
		}
		else if(D1 != 0.0f) {
			A = VV1;
			B = (VV0 - VV1) * D1;
			C = (VV2 - VV1) * D1;
			X0 = D1 - D0;
			X1 = D1 - D2;
		}
		else if(D2 != 0.0f) {
			A = VV2;
			B = (VV0 - VV2) * D2;
			C = (VV1 - VV2) * D2;
			X0 = D2 - D0;
			X1 = D2 - D1;
		}
		// for brevity, ignore any coplanar intersections
		else return false;
		
		return true;
	};
	
	// compute plane equation of triangle(v0_a,v1_a,v2_a)
	const auto N1 = (v1_a - v0_a).cross(v2_a - v0_a);
	const auto d1 = N1.dot(v0_a);
	// plane equation 1: N1.X+d1=0
	
	// put v0_b,v1_b,v2_b into plane equation 1 to compute signed distances to the plane
	auto du0 = N1.dot(v0_b) - d1;
	auto du1 = N1.dot(v1_b) - d1;
	auto du2 = N1.dot(v2_b) - d1;
	
	// coplanarity robustness check
	static constexpr const float triangle_epsilon = 0.000001f;
	du0 = (abs(du0) < triangle_epsilon ? 0.0f : du0);
	du1 = (abs(du1) < triangle_epsilon ? 0.0f : du1);
	du2 = (abs(du2) < triangle_epsilon ? 0.0f : du2);
	
	const auto du0du1 = du0 * du1;
	const auto du0du2 = du0 * du2;
	
	if(du0du1 > 0.0f && du0du2 > 0.0f) { // same sign on all of them + not equal 0?
		return false; // no intersection occurs
	}
	
	// compute plane of triangle (v0_b,v1_b,v2_b)
	const auto N2 = (v1_b - v0_b).cross(v2_b - v0_b);
	const auto d2 = N2.dot(v0_b);
	// plane equation 2: N2.X+d2=0
	
	// put v0_a,v1_a,v2_a into plane equation 2
	auto dv0 = N2.dot(v0_a) - d2;
	auto dv1 = N2.dot(v1_a) - d2;
	auto dv2 = N2.dot(v2_a) - d2;
	
	dv0 = (abs(dv0) < triangle_epsilon ? 0.0f : dv0);
	dv1 = (abs(dv1) < triangle_epsilon ? 0.0f : dv1);
	dv2 = (abs(dv2) < triangle_epsilon ? 0.0f : dv2);
	
	const auto dv0dv1 = dv0 * dv1;
	const auto dv0dv2 = dv0 * dv2;
	
	if(dv0dv1 > 0.0f && dv0dv2 > 0.0f) { // same sign on all of them + not equal 0?
		return false; // no intersection occurs
	}
	
	// compute direction of intersection line,
	// then compute and index to the largest component of D
	const auto index = N1.crossed(N2).abs().max_element_index();
	
	// this is the simplified projection onto L
	const auto vp0 = (index == 0 ? v0_a.x : (index == 1 ? v0_a.y : v0_a.z));
	const auto vp1 = (index == 0 ? v1_a.x : (index == 1 ? v1_a.y : v1_a.z));
	const auto vp2 = (index == 0 ? v2_a.x : (index == 1 ? v2_a.y : v2_a.z));
	const auto up0 = (index == 0 ? v0_b.x : (index == 1 ? v0_b.y : v0_b.z));
	const auto up1 = (index == 0 ? v1_b.x : (index == 1 ? v1_b.y : v1_b.z));
	const auto up2 = (index == 0 ? v2_b.x : (index == 1 ? v2_b.y : v2_b.z));
	
	// compute interval for triangle 1
	float a, b, c, x0, x1;
	if(!compute_intervals(vp0, vp1, vp2, dv0, dv1, dv2, dv0dv1, dv0dv2, a, b, c, x0, x1)) return false;
	
	// compute interval for triangle 2
	float d, e, f, y0, y1;
	if(!compute_intervals(up0, up1, up2, du0, du1, du2, du0du1, du0du2, d, e, f, y0, y1)) return false;
	
	const auto xx = x0 * x1;
	const auto yy = y0 * y1;
	const auto xxyy = xx * yy;
	
	const auto tmp1 = a * xxyy;
	const float isect1[] {
		tmp1 + b * x1 * yy,
		tmp1 + c * x0 * yy
	};
	
	const auto tmp2 = d * xxyy;
	const float isect2[] {
		tmp2 + e * xx * y1,
		tmp2 + f * xx * y0
	};
	
	return (max(isect1[0], isect1[1]) < min(isect2[0], isect2[1]) ||
			max(isect2[0], isect2[1]) < min(isect1[0], isect1[1]) ? false : true);
}

#endif
