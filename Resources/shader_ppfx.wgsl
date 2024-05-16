struct VertexInput {
	@location(0) position: vec3f,
	@location(1) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(1) uv: vec2f,
};

@group(0) @binding(0) var renderTexture: texture_2d<f32>;
//@group(0) @binding(0) var renderTexture: texture_depth_2d;
@group(0) @binding(1) var textureSampler: sampler;


@vertex
fn vs_main(input : VertexInput) -> VertexOutput {
    var output : VertexOutput;
    output.uv = input.uv;
    output.position = vec4<f32>(input.position, 1.0);
    return output;
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
    let textureColor = textureSample(renderTexture, textureSampler, in.uv).rgb;

    let acesInput = textureColor * 0.5;
    let aces = acesFilm(acesInput);

    let gammaCorrected = pow(aces, vec3<f32>(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    return vec4<f32>(gammaCorrected, 1.0);
}

//fn LinearizeDepth(depth: f32, near_plane: f32, far_plane: f32) -> f32 {
//    let z: f32 = depth * 2.0 - 1.0; // Convert from [0,1] range to [-1,1] range (NDC space)
//    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
//}

//@fragment
//fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
//	let depthValue = textureSample(renderTexture, textureSampler, in.uv); // Assuming depth is stored in the 'r' channel
//	let linearDepth = LinearizeDepth(depthValue, 0.1, 1500.0); // Adjust zNear and zFar based on your camera setup
//
//	// Normalize the linear depth for better visualization
//	let normalizedDepth = linearDepth / 1500.0; // Normalize based on the far plane
//
//	return vec4<f32>(vec3<f32>(normalizedDepth, normalizedDepth, normalizedDepth), 1.0);
//}
