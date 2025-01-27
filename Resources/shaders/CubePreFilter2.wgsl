@group(0) @binding(0) var outputIrradianceMap: texture_storage_2d_array<rgba32float, write>;
@group(0) @binding(1) var radianceMap: texture_cube<f32>;
@group(0) @binding(2) var radianceMapSampler: sampler;

@compute @workgroup_size(4, 4, 6)
fn prefilterCubeMap(@builtin(global_invocation_id) id: vec3u) {
    let outputDimensions = textureDimensions(outputIrradianceMap).xy;
    if (id.x >= outputDimensions.x || id.y >= outputDimensions.y) {
        return;
    }
    
    let N = getCubeMapTexCoord(vec2f(outputDimensions), id);
    let basis = computeBasisVectors(N);
    let totalSamples = 64u * 512;
    
    // Monte Carlo integration of hemispherical irradiance
    var irradiance = vec3f(0.0);
    for(var i = 0u; i < totalSamples; i++) {
        let u = hammersley(i, totalSamples);
        let Li = tangentToWorld(sampleHemisphere(u.x, u.y), N, basis.S, basis.T);
        let cosTheta = max(0.0, dot(Li, N));
        
        // Sample the radiance map at the highest resolution (lod 0)
        let radianceSample = textureSampleLevel(
            radianceMap,
            radianceMapSampler,
            Li,
            0.0
        ).rgb;
        
        irradiance += 2.0 * radianceSample * cosTheta;
    }
    
    irradiance /= vec3f(f32(totalSamples));
    textureStore(outputIrradianceMap, id.xy, id.z, vec4f(irradiance, 1.0));
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

fn tangentToWorld(v: vec3f, N: vec3f, S: vec3f, T: vec3f) -> vec3f {
    return S * v.x + T * v.y + N * v.z;
}

// Helper functions needed (assuming you have these from your prefilter shader):
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

struct BasisVectors {
    S: vec3f,
    T: vec3f,
}

fn computeBasisVectors(N: vec3f) -> BasisVectors {
    let up = select(vec3f(1.0, 0.0, 0.0), vec3f(0.0, 0.0, 1.0), abs(N.y) < 0.999);
    let S = normalize(cross(up, N));
    let T = cross(N, S);
    return BasisVectors(S, T);
}

const TWO_PI: f32 = 6.283185307179586;

fn sampleHemisphere(u1: f32, u2: f32) -> vec3f {
    let u1p = sqrt(max(0.0, 1.0 - u1 * u1));
    return vec3f(
        cos(TWO_PI * u2) * u1p,
        sin(TWO_PI * u2) * u1p,
        u1
    );
}
