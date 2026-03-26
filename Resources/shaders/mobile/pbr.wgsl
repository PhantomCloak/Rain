struct VertexInput {
	@location(0) a_position: vec3f,
	@location(1) a_normal: vec3f,
	@location(2) a_uv: vec2f,
	@location(3) a_tangent: vec3f,
	@location(4) a_bitangent: vec3f
};

struct InstanceInput {
	@location(5) a_MRow0: vec4<f32>,
	@location(6) a_MRow1: vec4<f32>,
	@location(7) a_MRow2: vec4<f32>,
}

struct VertexOutput {
	@builtin(position) pos: vec4f,
	@location(2) Normal: vec3f,
	@location(3) Uv: vec2f,
	@location(4) FragPos: vec3f,
	@location(5) WorldPosition: vec3f,
	@location(6) WorldNormal: vec3f,
	@location(7) WorldTangent: vec3f,
	@location(8) WorldBitangent: vec3f,
	@location(9)  ShadowCoord0: vec3f,
	@location(10) ShadowCoord1: vec3f,
};

struct SceneData {
	viewProjection: mat4x4f,
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

@group(0) @binding(0) var<uniform> u_Scene: SceneData;


@group(1) @binding(0) var<uniform> uMaterial: MaterialUniform;
@group(1) @binding(1) var u_TextureSampler: sampler;
@group(1) @binding(2) var u_AlbedoTex: texture_2d<f32>;
@group(1) @binding(3) var u_MetallicTex: texture_2d<f32>;
@group(1) @binding(4) var u_NormalTex: texture_2d<f32>;

@group(2) @binding(0) var u_ShadowMap: texture_depth_2d_array;
@group(2) @binding(1) var u_ShadowSampler: sampler_comparison;
@group(2) @binding(2) var<uniform> u_ShadowData: ShadowData;

@group(3) @binding(0) var u_radianceMap: texture_cube<f32>;
@group(3) @binding(1) var u_radianceMapSampler: sampler;
@group(3) @binding(2) var u_BDRFLut: texture_2d<f32>;
@group(3) @binding(3) var u_irradianceMap: texture_cube<f32>;
@group(3) @binding(4) var u_irradianceMapSampler: sampler;
@group(3) @binding(5) var u_BRDFSampler: sampler;

@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
    var out: VertexOutput;

    let transform = mat4x4<f32>(
        vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
        vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
        vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
        vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
    );

    let worldPos = transform * vec4f(in.a_position, 1.0);

    out.Normal = normalize((transform * vec4<f32>(in.a_normal, 0.0)).xyz);
    out.WorldNormal = normalize((transform * vec4<f32>(in.a_normal, 0.0)).xyz);
    out.WorldTangent = normalize((transform * vec4<f32>(in.a_tangent, 0.0)).xyz);
    out.WorldBitangent = normalize((transform * vec4<f32>(in.a_bitangent, 0.0)).xyz);

    out.WorldPosition = worldPos.xyz;
    out.Uv = in.a_uv;

    out.pos = u_Scene.viewProjection * worldPos;

    // Mobile: only 2 shadow cascades
    let shadowCoords0 = u_ShadowData.ShadowViewProjection[0] * worldPos;
    let shadowCoords1 = u_ShadowData.ShadowViewProjection[1] * worldPos;

    out.ShadowCoord0 = shadowCoords0.xyz / shadowCoords0.w;
    out.ShadowCoord1 = shadowCoords1.xyz / shadowCoords1.w;

    out.FragPos = (u_Scene.cameraViewMatrix * vec4f(out.WorldPosition, 1.0)).xyz;

    return out;
}

// PBR
fn GeometrySchlickGGX(NdotV: f32, roughness: f32) -> f32 {
	let r = roughness + 1.0;
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

fn GaSchlickG1(cosTheta: f32, k: f32) -> f32 {
    return cosTheta / (cosTheta * (1.0 - k) + k);
}

fn GaSchlickGGX(cosLi: f32, NdotV: f32, roughness: f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;
    return GaSchlickG1(cosLi, k) * GaSchlickG1(NdotV, k);
}

fn NdfGGX(cosLh: f32, roughness: f32) -> f32 {
	const PI: f32 = 3.141592653589793;

    let alpha = roughness * roughness;
    let alphaSq = alpha * alpha;

    let denom = (cosLh * cosLh) * (alphaSq - 1.0) + 1.0;
    return alphaSq / (PI * denom * denom);
}

fn FresnelSchlick(F0: vec3<f32>, cosTheta: f32) -> vec3<f32> {
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

fn FresnelSchlickRoughness(F0: vec3<f32>, cosTheta: f32, roughness: f32) -> vec3<f32> {
	return F0 + (max(vec3<f32>(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}


fn CalculateDirLights(F0: vec3<f32>, View: vec3<f32>, Normal: vec3<f32>, NdotV:f32, Albedo: vec3<f32>, Roughness: f32, Metalness: f32) -> vec3<f32> {
	var result: vec3<f32> = vec3<f32>(0.0);

	let Li: vec3<f32> = u_Scene.LightDirection;
	let Lradiance: vec3<f32> = vec3(1.0) * 1.5f;
	let Lh: vec3<f32> = normalize(Li + View);

	let cosLi: f32 = max(0.0, dot(Normal, Li));
	let cosLh: f32 = max(0.0, dot(Normal, Lh));

	let F: vec3<f32> = FresnelSchlickRoughness(F0, max(0.0, dot(Lh, View)), Roughness);
	let D: f32 = NdfGGX(cosLh, Roughness);
	let G: f32 = GaSchlickGGX(cosLi, NdotV, Roughness);

	let kd: vec3<f32> = (1.0 - F) * (1.0 - Metalness);
	let diffuseBRDF: vec3<f32> = kd * Albedo;

	const Epsilon: f32 = 1e-5;
	let specularBRDF: vec3<f32> = (F * D * G) / max(Epsilon, 4.0 * cosLi * NdotV);
	let clampedSpecularBRDF = clamp(specularBRDF, vec3<f32>(0.0), vec3<f32>(10.0));

	result += (diffuseBRDF + clampedSpecularBRDF) * Lradiance * cosLi;

	return result;
}

// Shadows
fn sampleShadow(in: VertexOutput, cascadeIndex: u32, bias: f32) -> f32 {
    let shadowCoords = GetShadowMapCoords(in, cascadeIndex);
    let projCoords = shadowCoords.xy * vec2(0.5, -0.5) + vec2(0.5);
    // Mobile: 1024 shadow map resolution
    let texelSize: vec2<f32> = vec2(1.0 / 1024.0);
    let halfKernelWidth: i32 = 1;

    var shadow: f32 = 0.0;
    let totalSamples: f32 = f32((halfKernelWidth * 2 + 1) * (halfKernelWidth * 2 + 1));

    for (var x: i32 = -halfKernelWidth; x <= halfKernelWidth; x = x + 1) {
        for (var y: i32 = -halfKernelWidth; y <= halfKernelWidth; y = y + 1) {
            let offset = vec2<f32>(f32(x), f32(y)) * texelSize;
			let sampleCoords = clamp(projCoords + offset, vec2(0.0), vec2(1.0));

            let inBoundsX = step(0.0, sampleCoords.x) * (1.0 - step(1.0, sampleCoords.x));
            let inBoundsY = step(0.0, sampleCoords.y) * (1.0 - step(1.0, sampleCoords.y));
            let inBounds = inBoundsX * inBoundsY;

            let depthComparison = textureSampleCompare(
                u_ShadowMap,
                u_ShadowSampler,
                sampleCoords,
                cascadeIndex,
                shadowCoords.z - bias
            );

            let adjustedDepthComparison = inBounds * depthComparison + (1.0 - inBounds) * 1.0;

            shadow += adjustedDepthComparison;
        }
    }

    shadow /= totalSamples;
    return shadow;
}


fn IBL(F0: vec3<f32>, Lr: vec3<f32>, Normal: vec3<f32>, NdotV: f32, Albedo: vec3<f32>, Roughness: f32, Metalness: f32) -> vec3<f32> {
    let irradiance: vec3<f32> = textureSample(u_irradianceMap, u_irradianceMapSampler, Normal).rgb;

    let F: vec3<f32> = FresnelSchlickRoughness(F0, NdotV, Roughness);

    let kd: vec3<f32> = (1.0 - F) * (1.0 - Metalness);
    let diffuseIBL: vec3<f32> = Albedo * irradiance;

    let envRadianceTexLevels: u32 = textureNumLevels(u_radianceMap);

    let specularIrradiance: vec3<f32> = textureSampleLevel(
        u_radianceMap,
        u_radianceMapSampler,
        Lr,
        Roughness * f32(envRadianceTexLevels - 1u)
    ).rgb;

    let specularBRDF: vec2<f32> = textureSample(u_BDRFLut, u_BRDFSampler, vec2<f32>(NdotV, Roughness)).rg;

    let specularIBL: vec3<f32> = specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);

    return kd * diffuseIBL + specularIBL;
}

fn acesFilm(x: vec3<f32>) -> vec3<f32> {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3<f32>(0.0, 0.0, 0.0), vec3<f32>(1.0, 1.0, 1.0));
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {

	// Sample PBR Resources
	let Albedo = textureSample(u_AlbedoTex, u_TextureSampler, in.Uv).rgb * uMaterial.Ao;
	let Metalness = textureSample(u_MetallicTex, u_TextureSampler, in.Uv).b * uMaterial.Metallic;
	let Roughness = textureSample(u_MetallicTex, u_TextureSampler, in.Uv).g * uMaterial.Roughness;

	// Cook our variables

	let Fdielectric = vec3(0.04);
	var Lo = vec3(0.0);

	var Normal = normalize(in.WorldNormal);

	if(uMaterial.UseNormalMap == 1)
	{
		let sampled_normal = normalize(textureSample(u_NormalTex, u_TextureSampler, in.Uv).rgb * 2.0 - 1.0);
		Normal = normalize(
				sampled_normal.x * in.WorldTangent +
				sampled_normal.y * in.WorldBitangent +
				sampled_normal.z * in.WorldNormal);
	}

	let View = normalize(u_Scene.CameraPosition - in.WorldPosition.xyz);
	let NdotV = max(dot(Normal, View), 0.0);
	let Lr = 2.0 * NdotV * Normal - View;
	let FO = mix(Fdielectric, Albedo, Metalness);

	let lightDir = normalize(u_Scene.LightDirection);

	// Shadow Mapping
	let MIN_BIAS = 0.005;
	let bias = max(MIN_BIAS * (1.0 - dot(Normal, lightDir)), MIN_BIAS);

	let viewDepth = -in.FragPos.z;
	// Mobile: 2 shadow cascades
	let SHADOW_MAP_CASCADE_COUNT = 2u;
	var layer = 0u;
	for (var i = 0u; i < SHADOW_MAP_CASCADE_COUNT - 1u; i = i + 1u) {
		if (viewDepth > u_ShadowData.CascadeDistances[i]) {
			layer = i + 1u;
		}
	}

	// Final Color
	var shadowScale = sampleShadow(in, layer, bias);
	shadowScale = 1.0 - clamp(1.0 - shadowScale, 0.0f, 1.0f);
	var lightContribution = CalculateDirLights(FO,
			View,
			Normal,
			NdotV,
			Albedo,
			Roughness,
			Metalness) * shadowScale;

	let iblContribution = IBL(FO,
			Lr,
			Normal,
			NdotV,
			Albedo,
			Roughness,
			Metalness);

	// Mobile: inline tonemap to SDR since framebuffer is RGBA8
	let hdrColor = iblContribution + lightContribution;
	let exposure = 0.84;
	let sdrColor = acesFilm(hdrColor * exposure);

	return vec4f(sdrColor, 1.0);
}

fn GetShadowMapCoords(
	in: VertexOutput,
    cascade: u32
) -> vec3<f32> {
    switch (cascade) {
        case 0: { return in.ShadowCoord0; }
        case 1: { return in.ShadowCoord1; }
        default: { return vec3<f32>(0.0); }
    }
}
