struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
};

struct InstanceInput {
	@location(4) a_MRow0: vec4<f32>,
	@location(5) a_MRow1: vec4<f32>,
	@location(6) a_MRow2: vec4<f32>,
}

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(2) Normal: vec3f,
	@location(3) uv: vec2f,
	@location(4) LightPos: vec3f,
	@location(5) FragPos: vec3f,
	@location(6) FragPosLightSpace: vec4f,
};

struct SceneData {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};

struct MaterialUniform {
    ambientColor: vec3f,
    diffuseColor: vec3f,
    specularColor: vec3f,
	shininess: f32
};

struct ShadowUniform {
    lightProjection: mat4x4f,
    lightView: mat4x4f,
    lightPos: vec3<f32>,
};

//@group(0) @binding(0) var<storage> uScene: array<SceneUniform>;
@group(0) @binding(0) var<uniform> u_scene: SceneData;

@group(1) @binding(0) var gradientTexture: texture_2d<f32>;
@group(1) @binding(1) var textureSampler: sampler;
@group(1) @binding(2) var<uniform> uMaterial: MaterialUniform;

//@group(3) @binding(0) var shadowMap: texture_depth_2d;
//@group(3) @binding(1) var shadowSampler: sampler_comparison;
//@group(3) @binding(2) var<uniform> shadowUniform: ShadowUniform;

@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
	var out: VertexOutput;

	let transform = mat4x4<f32>(
			vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
			vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
			vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
			vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
	);

	out.position = u_scene.projectionMatrix * u_scene.viewMatrix * transform * vec4f(in.position, 1.0);

	out.Normal = in.normal;
	out.uv = in.uv;

	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let textureColor = textureSample(gradientTexture, textureSampler, in.uv).rgba;
	var ambient = vec3f(0.1);

	// Diffuse
	let norm = normalize(in.Normal);
	let lightDir = normalize(in.LightPos - in.FragPos);
	var diffuse = vec3f(max(dot(norm, lightDir), 0.0)) * uMaterial.diffuseColor;

	// Specular
	let viewDir = normalize(-in.FragPos);
	let halfwayDir = normalize(lightDir + viewDir);
	var specular = vec3f(pow(max(dot(norm, halfwayDir), 0.0), uMaterial.shininess));

	let finalColor = ambient + diffuse + specular * textureColor.rgb;
	return vec4f(finalColor, 1.0);
}
