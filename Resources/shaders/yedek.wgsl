struct VertexInput {
	@location(0) a_position: vec3f,
	@location(1) a_normal: vec3f,
	@location(2) a_uv: vec2f,
	@location(3) tangent: vec3f
};

struct InstanceInput {
	@location(4) a_MRow0: vec4<f32>,
	@location(5) a_MRow1: vec4<f32>,
	@location(6) a_MRow2: vec4<f32>,
}

struct VertexOutput {
	@builtin(position) pos: vec4f,
	@location(2) Normal: vec3f,
	@location(3) Uv: vec2f,
	@location(4) FragPos: vec3f,
	@location(5) FragPosWorldSpace: vec4f,
    @location(6) ShadowCoord0: vec3f,
    @location(7) ShadowCoord1: vec3f,
    @location(8) ShadowCoord2: vec3f,
    @location(9) ShadowCoord3: vec3f,
};

struct SceneData {
	viewProjection: mat4x4f,
	shadowViewProjection: mat4x4f,
	cameraViewMatrix: mat4x4f,
	CameraPosition: vec3<f32>,
	LightDirection: vec3<f32>
};

struct ShadowData {
	ShadowViewProjection: array<mat4x4<f32>, 4>,
	CascadeDistances: vec4<f32>
};

struct MaterialUniform {
    Metallic: f32,
    Roughness: f32,
    Ao: f32,
	UseNormalMap: i32
};
struct ShadowResult {
    shadow: f32,
    layer: u32,
}

@group(0) @binding(0) var<uniform> u_scene: SceneData;

@group(1) @binding(0) var gradientTexture: texture_2d<f32>;
@group(1) @binding(1) var textureSampler: sampler;
@group(1) @binding(2) var<uniform> uMaterial: MaterialUniform;
@group(1) @binding(3) var metalicRoughnessTexture: texture_2d<f32>;
@group(1) @binding(4) var heightTexture: texture_2d<f32>;

//@group(2) @binding(0) var shadowMap: texture_depth_2d;
@group(2) @binding(0) var shadowMap: texture_depth_2d_array;
@group(2) @binding(1) var shadowSampler: sampler_comparison;
@group(2) @binding(2) var<uniform> u_ShadowData: ShadowData;

@group(3) @binding(0) var irradianceMap: texture_cube<f32>;
@group(3) @binding(1) var irradianceMapSampler: sampler;



@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
    var out: VertexOutput;

    // Instance transformation matrix
    let transform = mat4x4<f32>(
        vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
        vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
        vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
        vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
    );

    // Transform position to world space
    let worldPos = (transform * vec4f(in.a_position, 1.0)).xyz;
    out.FragPosWorldSpace = vec4f(worldPos, 1.0);

    // Manually construct a mat3x3 for normal transformation
    let normalMatrix = mat3x3<f32>(
        transform[0].xyz,
        transform[1].xyz,
        transform[2].xyz
    );
    out.Normal = normalize(normalMatrix * in.a_normal);

    // Pass UV coordinates
    out.Uv = in.a_uv;

    // Transform to clip space for position output
    out.pos = u_scene.viewProjection * vec4f(worldPos, 1.0);

  // Calculate shadow coordinates for each cascade
    let shadowCoords0 = u_ShadowData.ShadowViewProjection[0] * vec4f(worldPos, 1.0);
    let shadowCoords1 = u_ShadowData.ShadowViewProjection[1] * vec4f(worldPos, 1.0);
    let shadowCoords2 = u_ShadowData.ShadowViewProjection[2] * vec4f(worldPos, 1.0);
    let shadowCoords3 = u_ShadowData.ShadowViewProjection[3] * vec4f(worldPos, 1.0);

    // Perspective divide and store as vec3 for each shadow coordinate
    out.ShadowCoord0 = shadowCoords0.xyz / shadowCoords0.w;
    out.ShadowCoord1 = shadowCoords1.xyz / shadowCoords1.w;
    out.ShadowCoord2 = shadowCoords2.xyz / shadowCoords2.w;
    out.ShadowCoord3 = shadowCoords3.xyz / shadowCoords3.w;

    return out;
}

fn DistributionGGX(N: vec3<f32>, H: vec3<f32>, roughness: f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let NdotH = max(dot(N, H), 0.0);
    let NdotH2 = NdotH * NdotH;

    let nom = a2;
    var denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = denom * denom * 3.14159265359; // PI

    return nom / denom;
}

fn GeometrySchlickGGX(NdotV: f32, roughness: f32) -> f32 {
    let r = (roughness + 1.0);
    let k = (r * r) / 8.0;

    let nom = NdotV;
    let denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

fn GeometrySmith(N: vec3<f32>, V: vec3<f32>, L: vec3<f32>, roughness: f32) -> f32 {
    let NdotV = max(dot(N, V), 0.0);
    let NdotL = max(dot(N, L), 0.0);
    let ggx2 = GeometrySchlickGGX(NdotV, roughness);
    let ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

fn fresnelSchlick(cosTheta: f32, F0: vec3<f32>) -> vec3<f32> {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

fn getNormalFromMap(normalMap: texture_2d<f32>, defaultSampler: sampler, TexCoords: vec2<f32>, WorldPos: vec3<f32>, Normal: vec3<f32>) -> vec3<f32> {
    let tangentNormal = textureSample(normalMap, defaultSampler, TexCoords).xyz * 2.0 - 1.0;

    let Q1 = dpdx(WorldPos);
    let Q2 = dpdy(WorldPos);
    let st1 = dpdx(TexCoords);
    let st2 = dpdy(TexCoords);

    let N = normalize(Normal);
    let T = normalize(Q1 * st2.y - Q2 * st1.y);
    let B = -normalize(cross(N, T));
    let TBN = mat3x3<f32>(T, B, N);

    return normalize(TBN * tangentNormal);
}


fn GetShadowMapCoords(
	in: VertexOutput,
    cascade: u32
) -> vec3<f32> {
    switch (cascade) {
        case 0: { return in.ShadowCoord0; }
        case 1: { return in.ShadowCoord1; }
        case 2: { return in.ShadowCoord2; }
        case 3: { return in.ShadowCoord3; }
        default: { return vec3<f32>(0.0); }
    }
}

fn ShadowCalculation(
    fragPosWorldSpace: vec4<f32>,
    normal: vec3<f32>,
    lightDir: vec3<f32>,
	vin: VertexOutput
) -> ShadowResult {

    let fragPosViewSpace = u_scene.cameraViewMatrix * fragPosWorldSpace;
    let depthVal = -fragPosViewSpace.z;

	var layer = 3u;  // Default to the last layer
	for (var i = 0u; i < 3u; i = i + 1u) {
		if (depthVal < u_ShadowData.CascadeDistances[i]) {
			layer = i;
			break;
		}
	}

    let fragPosLightSpace = u_ShadowData.ShadowViewProjection[layer] * fragPosWorldSpace;
    var projCoords = vec3(fragPosLightSpace.xy * vec2(0.5, -0.5) + vec2(0.5), fragPosLightSpace.z);

    let currentDepth: f32 = projCoords.z;
    let MINIMUM_SHADOW_BIAS = 0.001;
    let bias = max(MINIMUM_SHADOW_BIAS * (1.0 - dot(normal, lightDir)), MINIMUM_SHADOW_BIAS);
    
    var shadow: f32 = 0.0;
    let texelSize: vec2<f32> = vec2(1.0 / 4096.0);
    const halfKernelWidth: i32 = 1;

	let shadowCoords = GetShadowMapCoords(vin, layer);

    for (var x: i32 = -halfKernelWidth; x <= halfKernelWidth; x = x + 1) {
        for (var y: i32 = -halfKernelWidth; y <= halfKernelWidth; y = y + 1) {
            let sampleCoords: vec2<f32> = projCoords.xy + vec2<f32>(f32(x), f32(y)) * texelSize;
            let pcfDepth: f32 = textureSampleCompare(shadowMap, shadowSampler, sampleCoords, layer, currentDepth - bias);
            shadow += pcfDepth;
        }
    }
    shadow /= f32((halfKernelWidth * 2 + 1) * (halfKernelWidth * 2 + 1));

    return ShadowResult(shadow, layer);
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let albedo = textureSample(gradientTexture, textureSampler, in.Uv).rgb;

    let ambientStrength = 0.25;
    let ambient = ambientStrength * albedo;

    let norm = normalize(in.Normal);
	let lightDir = u_scene.LightDirection;

    let lightColor = vec3f(1.0, 1.0, 1.0);
    let diff = max(dot(norm, lightDir), 0.0);
    let diffuse = diff * lightColor * albedo;

    let shadowResult = ShadowCalculation(in.FragPosWorldSpace, in.Normal, lightDir, in);
    let shadow = shadowResult.shadow;
    let layer = shadowResult.layer;

    let cascadeColors = array<vec3<f32>, 4>(
        vec3<f32>(1.0, 0.0, 0.0),  // Red
        vec3<f32>(0.0, 1.0, 0.0),  // Green
        vec3<f32>(0.0, 0.0, 1.0),  // Blue
        vec3<f32>(1.0, 1.0, 0.0)   // Yellow
    );

    var cascadeColor: vec3<f32> = vec3<f32>(1.0); // Default color
    let litColor = ambient + shadow * diffuse;

    if (layer >= 0 && layer < 4) {
        cascadeColor = cascadeColors[layer];
    }

    // Final color output
    return vec4f(litColor, 1.0);
}

