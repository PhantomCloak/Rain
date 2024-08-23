struct VertexInput {
	@location(0) a_position: vec3f,
	@location(1) a_normal: vec3f,
	@location(2) a_uv: vec2f,
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
	@location(4) WorldPosition: vec4f,
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
    Ao: f32
};

@group(0) @binding(0) var<uniform> u_scene: SceneData;

@group(1) @binding(0) var gradientTexture: texture_2d<f32>;
@group(1) @binding(1) var textureSampler: sampler;
@group(1) @binding(2) var<uniform> uMaterial: MaterialUniform;
@group(1) @binding(3) var metalicRoughnessTexture: texture_2d<f32>;
@group(1) @binding(4) var heightTexture: texture_2d<f32>;

@group(2) @binding(0) var shadowMap: texture_depth_2d;
@group(2) @binding(1) var shadowSampler: sampler_comparison;

@group(3) @binding(0) var cubemapTexture: texture_cube<f32>;
@group(3) @binding(1) var cubeSampler: sampler;

@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
	var out: VertexOutput;

	let transform = mat4x4<f32>(
			vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
			vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
			vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
			vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
	);

	let worldPosition = transform * vec4f(in.a_position, 1.0);

	let myMat3x3 = mat3x3<f32>(transform[0].xyz, transform[1].xyz, transform[2].xyz);

	out.pos = u_scene.viewProjection * transform * vec4f(in.a_position, 1.0);

	out.WorldPosition = worldPosition;
	out.Normal = myMat3x3 * in.a_normal;
	out.Uv = in.a_uv;

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



@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
		let lightColor = vec3(1.0);

		//let albedo = uMaterial.diffuseColor;
		//let albedo = vec3(1.0, 0.0, 0.0);
		//let metallic = uMaterial.Metallic;
		//let roughness = uMaterial.Roughness;
		let aoBooDebug = uMaterial.Ao;
		let albedo = pow(textureSample(gradientTexture, textureSampler, in.Uv).rgb, vec3(2.2));
		let metallic = textureSample(metalicRoughnessTexture,textureSampler, in.Uv).b * uMaterial.Metallic;
		let roughness = textureSample(metalicRoughnessTexture,textureSampler, in.Uv).g * uMaterial.Roughness;
		let ao = textureSample(metalicRoughnessTexture,textureSampler, in.Uv).r;
		//u_MaterialUniforms.Metalness
		var Lo = vec3(0.0);

		//let N = normalize(in.Normal);
		let N = getNormalFromMap(heightTexture, textureSampler, in.Uv, in.WorldPosition.xyz, in.Normal);
		let View = normalize(u_scene.CameraPosition - in.WorldPosition.xyz);

		let L = normalize(u_scene.LightPosition - in.WorldPosition.xyz);
		let H = normalize(View + L);

		let distance = length(u_scene.LightPosition - in.WorldPosition.xyz);
		let attenuation = 1.0 / (distance * distance);
		//let radiance = lightColor * attenuation; 
		let radiance = lightColor * 1.5;

		var F0 = vec3(0.04); 
		F0 = mix(F0, albedo, metallic);

		let NDF = DistributionGGX(N, H, roughness);       
		let G = GeometrySmith(N, View, L, roughness);
		let F = fresnelSchlick(max(dot(H, View), 0.0), F0);

		// DEAL
		let numerator = NDF * G * F;
		let denominator = 4.0 * max(dot(N, View), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
		let specular = numerator / denominator;

		let kS = F;
		var kD = vec3(1.0) - kS;
		kD *= 1.0 - metallic;

		let PI = 3.14159265359;
		let NdotL = max(dot(N, L), 0.0);
		Lo += (kD * albedo / PI + specular) * radiance * NdotL;

		let ambient = vec3(0.03) * albedo * ao;
		var color = ambient + Lo;  
		color = color / (color + vec3(1.0));
		color = pow(color, vec3(1.0/2.2));

		return vec4f(color, 1.0);
}
