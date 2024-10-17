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
	//@location(4) WorldPosition: vec4f,
	@location(4) LightPos: vec3f,
	@location(5) FragPos: vec3f,
	@location(6) FragPosLightSpace: vec4f,
};

struct SceneData {
	viewProjection: mat4x4f,
	shadowViewProjection: mat4x4f,
	cameraViewMatrix: mat4x4f,
	CameraPosition: vec3<f32>,
	LightPosition: vec3<f32>
};

struct MaterialUniform {
    Metallic: f32,
    Roughness: f32,
    Ao: f32,
	UseNormalMap: i32
};

@group(0) @binding(0) var<uniform> u_scene: SceneData;

@group(1) @binding(0) var gradientTexture: texture_2d<f32>;
@group(1) @binding(1) var textureSampler: sampler;
@group(1) @binding(2) var<uniform> uMaterial: MaterialUniform;
@group(1) @binding(3) var metalicRoughnessTexture: texture_2d<f32>;
@group(1) @binding(4) var heightTexture: texture_2d<f32>;

@group(2) @binding(0) var shadowMap: texture_depth_2d;
@group(2) @binding(1) var shadowSampler: sampler_comparison;

@group(3) @binding(0) var irradianceMap: texture_cube<f32>;
@group(3) @binding(1) var irradianceMapSampler: sampler;

@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
	var out: VertexOutput;

	let transform = mat4x4<f32>(
			vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
			vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
			vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
			vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
	);

	let fragPos = (transform * vec4f(in.a_position, 1.0)).xyz;
	out.pos = u_scene.viewProjection * transform * vec4f(in.a_position, 1.0);
	out.Normal = (u_scene.cameraViewMatrix * transform * vec4f(in.a_normal, 0.0)).xyz;

	out.Uv = in.a_uv;

	out.LightPos = (u_scene.cameraViewMatrix * vec4f(u_scene.LightPosition, 1.0)).xyz;
	out.FragPos = (u_scene.cameraViewMatrix * vec4f(fragPos, 1.0)).xyz;
	out.FragPosLightSpace = u_scene.shadowViewProjection * vec4f(fragPos, 1.0);

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

fn ShadowCalculation(
    fragPosLightSpace: vec4<f32>,
    normal: vec3<f32>,
    lightDir: vec3<f32>
) -> f32 {
    var projCoords: vec3<f32> = fragPosLightSpace.xyz;
    projCoords = vec3(projCoords.xy * vec2(0.5, -0.5) + vec2(0.5), projCoords.z);

    let currentDepth: f32 = projCoords.z;
    let bias: f32 = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);

    var shadow: f32 = 0.0;

    let texelSize: vec2<f32> = vec2(1.0 / 2048.0);
    const halfKernelWidth: i32 = 1;

    for (var x: i32 = -halfKernelWidth; x <= halfKernelWidth; x++) {
        for (var y: i32 = -halfKernelWidth; y <= halfKernelWidth; y++) {
            let sampleCoords: vec2<f32> = projCoords.xy + vec2<f32>(f32(x), f32(y)) * texelSize;
            let pcfDepth: f32 = textureSampleCompare(shadowMap, shadowSampler, sampleCoords, currentDepth - bias);
            shadow += pcfDepth;
        }
    }
    shadow /= f32((halfKernelWidth * 2 + 1) * (halfKernelWidth * 2 + 1));

    return shadow;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let textureColor = textureSample(gradientTexture, textureSampler, in.Uv).rgba;
	let ambient = vec3f(0.3);

	if(uMaterial.UseNormalMap == 1)
	{
	}

	let d = vec3(1.0, 1.0, 1.0);

	// Diffuse
	let norm = normalize(in.Normal);
	let lightDir = normalize(in.LightPos - in.FragPos);
	var diffuse = vec3f(max(dot(norm, lightDir), 0.0)) * d;

	// Specular
	let viewDir = normalize(-in.FragPos);
	let halfwayDir = normalize(lightDir + viewDir);
	var specular = vec3f(pow(max(dot(norm, halfwayDir), 0.0), 64));

	var shadow = ShadowCalculation(in.FragPosLightSpace, in.Normal , lightDir);
	let finalColor = (ambient + (shadow)  * (diffuse + specular)) * textureColor.rgb;
	//let finalColor = (ambient + diffuse + specular) * textureColor.rgb;
	return vec4f(finalColor, 1.0);
}

