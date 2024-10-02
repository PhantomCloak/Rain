struct VertexInput {
	@location(0) position: vec3f,
	@location(1) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(1) v_Position: vec3f,
};

struct CameraData {
	InverseViewProjectionMatrix: mat4x4<f32>
};

@group(0) @binding(0) var cubemapTexture: texture_cube<f32>;
@group(0) @binding(1) var textureSampler: sampler;

@group(1) @binding(0) var<uniform> u_Camera: CameraData;

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    var position: vec4f = vec4f(input.position.xy, 0.0, 1.0);
    output.position = position;
    output.v_Position = (u_Camera.InverseViewProjectionMatrix * position).xyz;
    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
	var color = textureSample(cubemapTexture, textureSampler, in.v_Position);
	//color.r = 255;
	return vec4<f32>(color.rgb, 1.0);
}
