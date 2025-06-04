@group(0) @binding(0) var inputCubemapTexture: texture_cube<f32>;
@group(0) @binding(1) var outputCubemapTexture: texture_storage_2d_array<rgba16float, write>;
@group(0) @binding(2) var textureSampler: sampler;
@group(0) @binding(3) var<uniform> ud_uniforms: Uniforms;

const PI = 3.14159265359;
struct Uniforms {
    currentMipLevel: u32,
    mipLevelCount: u32,
}

const MIN_ROUGHNESS = 0.002025;
const SAMPLE_COUNT = 512u;

fn D_GGX(NoH: f32, roughness: f32) -> f32 {
    let a = NoH * roughness;
    let k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}
struct BasisVectors {
    S: vec3f,
    T: vec3f,
}


@compute @workgroup_size(32, 32, 1)
	fn prefilterCubeMap(@builtin(global_invocation_id) id: vec3u) {
		let outputDimensions = textureDimensions(outputCubemapTexture).xy;

		if (id.x >= outputDimensions.x || id.y >= outputDimensions.y) {
			return;
		}

		let layer = id.z;
		var color = vec3f(0.0);
		var total_weight = 0.0;

		let roughness = lodToAlpha(f32(ud_uniforms.currentMipLevel) / f32(ud_uniforms.mipLevelCount - 1));

		let uv = vec2f(id.xy) / vec2f(outputDimensions - 1u);
		let N = getCubeMapTexCoord(vec2f(textureDimensions(outputCubemapTexture).xy), id);
		let Lo = N;

		let basis = computeBasisVectors(N);

		for (var i = 0u ; i < SAMPLE_COUNT ; i++) {
			let u = hammersley(i, SAMPLE_COUNT);
			let Lh = tangentToWorld(sampleGGX(u.x, u.y, roughness), N, basis.S, basis.T);
			let Li = 2.0 * dot(Lo, Lh) * Lh - Lo;
			let cosLi = dot(N, Li);
			if(cosLi > 0.0) {
				let cosLh = max(dot(N, Lh), 0.0);
				let pdf = ndfGGX(cosLh, roughness) * 0.25;
				let ws = 1.0 / (f32(SAMPLE_COUNT) * pdf);

				color = color + textureSampleLevel(inputCubemapTexture, textureSampler, Li, 0.0).rgb * cosLi;
				total_weight += cosLi;
			}
		}
		color /= total_weight;
		textureStore(outputCubemapTexture, id.xy, layer, vec4f(color, 1.0));
	}

fn maxComponent(v: vec3f) -> f32 {
    return max(v.x, max(v.y, v.z));
}

/**
 * Compute the u/v weights corresponding to the bilinear mix of samples
 * returned by textureGather for a 2D array texture.
 */
fn textureGatherWeights_2darray(t: texture_2d_array<f32>, uv: vec2f, layer: u32) -> vec2f {
    let dim = textureDimensions(t).xy;
    var corrected_uv = uv;

    // Optional correction for specific layers, if needed (similar to the original cube map code)
    if (layer == 4u || layer == 5u) {
        corrected_uv.x = 1.0 - corrected_uv.x;
    }

    let scaled_uv = corrected_uv * vec2f(dim);

    // Compute weights based on fractional part of UV coordinates, adjusted for precision
    // See: https://www.reedbeta.com/blog/texture-gathers-and-coordinate-precision/
    return fract(scaled_uv - 0.5);
}
// Utility to convert direction to cube face and UV coordinates
fn directionToCubeFaceUVL(dir: vec3f) -> CubeMapUVL {
    let absDir = abs(dir);
    let maxDir = maxComponent(absDir);
    var faceIndex: u32;
    var uv = vec2f(0.0);

    if (maxDir == absDir.x) {
        faceIndex = select(0u, 1u, dir.x > 0.0); // Positive or Negative X
        uv = vec2f(-dir.z, dir.y) / absDir.x;
    } else if (maxDir == absDir.y) {
        faceIndex = select(2u, 3u, dir.y > 0.0); // Positive or Negative Y
        uv = vec2f(dir.x, -dir.z) / absDir.y;
    } else {
        faceIndex = select(4u, 5u, dir.z > 0.0); // Positive or Negative Z
        uv = vec2f(dir.x, dir.y) / absDir.z;
    }

    uv = uv * 0.5 + 0.5; // Convert to [0, 1] UV space
    return CubeMapUVL(uv, faceIndex);
}


fn textureGatherWeights_cubef(t: texture_cube<f32>, direction: vec3f) -> vec2f {
    // major axis direction
    let cubemap_uvl = cubeMapUVLFromDirection(direction);
    let dim = textureDimensions(t).xy;
    var uv = cubemap_uvl.uv;

    // Empirical fix...
    if (cubemap_uvl.layer == 4u || cubemap_uvl.layer == 5u) {
        uv.x = 1.0 - uv.x;
    }

    let scaled_uv = uv * vec2f(dim);
    // This is not accurate, see see https://www.reedbeta.com/blog/texture-gathers-and-coordinate-precision/
    // but bottom line is:
    //   "Unfortunately, if we need this to work, there seems to be no option but to check
    //    which hardware you are running on and apply the offset or not accordingly."
    return fract(scaled_uv - 0.5);
}

fn sampleCubeMap(cubemapTexture: texture_cube<f32>, direction: vec3f) -> vec4f {
    let samples = array<vec4f, 4>(
        textureGather(0, cubemapTexture, textureSampler, direction),
        textureGather(1, cubemapTexture, textureSampler, direction),
        textureGather(2, cubemapTexture, textureSampler, direction),
        textureGather(3, cubemapTexture, textureSampler, direction),
    );

    let w = textureGatherWeights_cubef(cubemapTexture, direction);
    
    return vec4f(
        mix(mix(samples[0].w, samples[0].z, w.x), mix(samples[0].x, samples[0].y, w.x), w.y),
        mix(mix(samples[1].w, samples[1].z, w.x), mix(samples[1].x, samples[1].y, w.x), w.y),
        mix(mix(samples[2].w, samples[2].z, w.x), mix(samples[2].x, samples[2].y, w.x), w.y),
        mix(mix(samples[3].w, samples[3].z, w.x), mix(samples[3].x, samples[3].y, w.x), w.y),
    );
}

// Utility to convert direction to cube face and UV coordinates
fn cubeMapUVLFromDirection(direction: vec3f) -> CubeMapUVL {
    let abs_direction = abs(direction);
    var major_axis_idx = 0u;
    //  Using the notations of https://www.khronos.org/registry/OpenGL/specs/gl/glspec46.core.pdf page 253
    // as suggested here: https://stackoverflow.com/questions/55558241/opengl-cubemap-face-order-sampling-issue
    var ma = 0.0;
    var sc = 0.0;
    var tc = 0.0;
    if (abs_direction.x > abs_direction.y && abs_direction.x > abs_direction.z) {
        major_axis_idx = 0u;
        ma = direction.x;
        if (ma >= 0) {
            sc = -direction.z;
        } else {
            sc = direction.z;
        }
        tc = -direction.y;
    } else if (abs_direction.y > abs_direction.x && abs_direction.y > abs_direction.z) {
        major_axis_idx = 1u;
        ma = direction.y;
        sc = direction.x;
        if (ma >= 0) {
            tc = direction.z;
        } else {
            tc = -direction.z;
        }
    } else {
        major_axis_idx = 2u;
        ma = direction.z;
        if (ma >= 0) {
            sc = -direction.x;
        } else {
            sc = direction.x;
        }
        tc = -direction.y;
    }
    var sign_offset = 0u;
    if (ma < 0) {
        sign_offset = 1u;
    }
    let s = 0.5 * (sc / abs(ma) + 1.0);
    let t = 0.5 * (tc / abs(ma) + 1.0);
    return CubeMapUVL(
        vec2f(s, t),
        2 * major_axis_idx + sign_offset,
    );
}

/**
 * lod is linearly mapped from 0.0 at MIP level #0 to 1.0 at MIP level #mipLevelCount-1
 * alpha = perceptualRoughness²
 */
fn lodToAlpha(lod: f32) -> f32 {
    return lod;
}

fn getCubeMapTexCoord(imageSize: vec2f, id: vec3u) -> vec3f {
    let st = vec2f(id.xy) / imageSize;
    let uv = 2.0 * vec2f(st.x, 1.0 - st.y) - vec2f(1.0);

    var ret: vec3f;
    if (id.z == 0u) {
        ret = vec3f(1.0, uv.y, -uv.x);
    } else if (id.z == 1u) {
        ret = vec3f(-1.0, uv.y, uv.x);
    } else if (id.z == 2u) {
        ret = vec3f(uv.x, 1.0, -uv.y);
    } else if (id.z == 3u) {
        ret = vec3f(uv.x, -1.0, uv.y);
    } else if (id.z == 4u) {
        ret = vec3f(uv.x, uv.y, 1.0);
    } else if (id.z == 5u) {
        ret = vec3f(-uv.x, uv.y, -1.0);
    }

    return normalize(ret);
}

// Struct for UV and face index
struct CubeMapUVL {
    uv: vec2f,
    layer: u32,
}

const TOF = 0.5 / f32(0x80000000u);
fn hammersley(i: u32, numSamples: u32) -> vec2f {
    var bits = i;
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);
    return vec2(f32(i) / f32(numSamples), f32(bits) * TOF);
}

fn computeBasisVectors(N: vec3f) -> BasisVectors {
    // Branchless select non-degenerate T.
    var T = cross(N, vec3f(0.0, 1.0, 0.0));
		const Epsilon: f32 = 1e-5;
    T = mix(cross(N, vec3f(1.0, 0.0, 0.0)), T, step(Epsilon, dot(T, T)));

    T = normalize(T);
    let S = normalize(cross(N, T));

    return BasisVectors(S, T);
}

fn tangentToWorld(v: vec3f, N: vec3f, S: vec3f, T: vec3f) -> vec3f {
    return S * v.x + T * v.y + N * v.z;
}

fn ndfGGX(cosLh: f32, roughness: f32) -> f32 {
    let alpha = roughness * roughness;
    let alphaSq = alpha * alpha;

    let denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}
fn sampleGGX(u1: f32, u2: f32, roughness: f32) -> vec3f {
    let alpha = roughness * roughness;

    let cosTheta = sqrt((1.0 - u2) / (1.0 + (alpha * alpha - 1.0) * u2));
    let sinTheta = sqrt(1.0 - cosTheta * cosTheta); // Trigonometric identity
    let phi = 2.0 * PI * u1;

    // Convert to Cartesian coordinates and return
    return vec3f(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);
}
