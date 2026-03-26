@group(0) @binding(0) var inputCubemapTexture: texture_cube<f32>;
@group(0) @binding(1) var outputCubemapTexture: texture_storage_2d_array<rgba8unorm, write>;
@group(0) @binding(2) var textureSampler: sampler;
@group(0) @binding(3) var<uniform> ud_uniforms: Uniforms;

const PI = 3.14159265359;
struct Uniforms {
    currentMipLevel: u32,
    mipLevelCount: u32,
}

const MIN_ROUGHNESS = 0.002025;
// Mobile: reduced sample count for performance
const SAMPLE_COUNT = 256u;

fn D_GGX(NoH: f32, roughness: f32) -> f32 {
    let a = NoH * roughness;
    let k = roughness / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / PI);
}
struct BasisVectors {
    S: vec3f,
    T: vec3f,
}


@compute @workgroup_size(16, 16, 1)
	fn prefilterCubeMap(@builtin(global_invocation_id) id: vec3u) {
		let outputDimensions = textureDimensions(outputCubemapTexture).xy;

		if (id.x >= outputDimensions.x || id.y >= outputDimensions.y) {
			return;
		}

		let layer = id.z;
		var color = vec3f(0.0);
		var total_weight = 0.0;

		let roughness = lodToAlpha(f32(ud_uniforms.currentMipLevel) / f32(ud_uniforms.mipLevelCount - 1));

		// Solid angle associated with a single cubemap texel at mip 0.
		// Used for mip-filtered importance sampling (GPU Gems 3, Ch. 20.4).
		let inputSize = vec2f(textureDimensions(inputCubemapTexture, 0));
		let wt = 4.0 * PI / (6.0 * inputSize.x * inputSize.y);

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

				// GGX NDF pdf, scaled by 1/4 for Lh -> Li change of variable
				let pdf = ndfGGX(cosLh, roughness) * 0.25;

				// Solid angle associated with this sample
				let ws = 1.0 / (f32(SAMPLE_COUNT) * pdf);

				// Mip level to sample from: reduces noise by sampling pre-blurred mips
				let mipLevel = max(0.5 * log2(ws / wt) + 1.0, 0.0);

				color = color + textureSampleLevel(inputCubemapTexture, textureSampler, Li, f32(mipLevel)).rgb * cosLi;
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
