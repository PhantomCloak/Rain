struct VertexInput {
	@location(0) position: vec3f,
	@location(1) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(1) uv: vec2f,
};

@group(0) @binding(0) var renderTexture: texture_2d<f32>;
@group(0) @binding(1) var textureSampler: sampler;

@vertex
fn vs_main(input : VertexInput) -> VertexOutput {
    var output : VertexOutput;
    output.uv = input.uv;
    output.position = vec4<f32>(input.position, 1.0);
    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Mobile: passthrough — tonemapping already applied in PBR/skybox shaders
    let color = textureSample(renderTexture, textureSampler, in.uv).rgb;
    return vec4<f32>(color, 1.0);
}
