struct VertexInput {
	@location(0) position: vec3f,
	@location(1) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(1) uv: vec2f,
};

struct DebugUniform {
	near: f32,
	far: f32
};

@group(0) @binding(0) var renderTexture: texture_depth_2d;
@group(0) @binding(1) var textureSampler: sampler;
@group(0) @binding(2) var<uniform> u_debug: DebugUniform;


@vertex
fn vs_main(input : VertexInput) -> VertexOutput {
    var output : VertexOutput;
    output.uv = input.uv;
    output.position = vec4<f32>(input.position, 1.0);
    return output;
}

fn LinearizeDepth(depth: f32, near_plane: f32, far_plane: f32) -> f32 {
    return depth * (far_plane - near_plane) + near_plane;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let depthValue = textureSample(renderTexture, textureSampler, in.uv);
    let linearDepth = LinearizeDepth(depthValue, u_debug.near, u_debug.far);
    let normalizedDepth = (linearDepth - 0.1) / (u_debug.far - u_debug.near);

    return vec4<f32>(vec3<f32>(normalizedDepth, normalizedDepth, normalizedDepth), 1.0);
}
