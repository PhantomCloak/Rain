struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(2) Normal: vec3f,
	@location(3) uv: vec2f,
	@location(4) LightPos: vec3f,
	@location(5) FragPos: vec3f,
	@location(6) FragPosLightSpace: vec4f,
};

struct Camera {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};

struct SceneUniform {
	modelMatrix : mat4x4f,
    color: vec4f,
};

struct MaterialUniform {
    ambientColor: vec3f,
    diffuseColor: vec3f,
    specularColor: vec3f,
	shininess: f32
};

@group(0) @binding(0) var<storage> uScene: array<SceneUniform>;
@group(1) @binding(0) var<uniform> uCam: Camera;

@group(2) @binding(0) var gradientTexture: texture_2d<f32>;
@group(2) @binding(1) var textureSampler: sampler;
@group(2) @binding(2) var<uniform> uMaterial: MaterialUniform;

@vertex
fn vs_main(@builtin(instance_index) instanceIdx : u32, in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = uCam.projectionMatrix * uCam.viewMatrix * uScene[instanceIdx].modelMatrix * vec4f(in.position, 1.0);
	out.Normal = in.normal;
	out.uv = in.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let textureColor = textureSample(gradientTexture, textureSampler, in.uv).rgb;
	return vec4f(textureColor, 1.0);
}
