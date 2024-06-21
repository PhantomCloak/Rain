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
};

struct SceneData {
	viewProjection: mat4x4f,
	shadowViewProjection: mat4x4f,
	cameraViewMatrix: mat4x4f,
	lightPos: vec3<f32>,
};

@group(0) @binding(0) var<uniform> u_scene: SceneData;

@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
	var out: VertexOutput;

	let transform = mat4x4<f32>(
			vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
			vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
			vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
			vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
	);

	out.position = u_scene.shadowViewProjection * transform * vec4f(in.position, 1.0);
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(1.0f);
}

