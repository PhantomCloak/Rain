struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f, // Consider removing if normals are no longer used.
	@location(2) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) depth: f32,
};

struct Camera {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};

struct MyUniforms {
    modelMatrix: mat4x4f,
    color: vec4f,
};

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;
@group(1) @binding(0) var<uniform> uCam: Camera;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = uCam.projectionMatrix * uCam.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
	out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz; // Consider removing if normals are no longer used.
	out.color = vec3f(1);
	out.uv = in.uv;
	out.depth = out.position.z / out.position.w; // Calculate depth
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let textureColor = textureSample(gradientTexture, textureSampler, in.uv).rgba;
	return vec4f(textureColor);
}

