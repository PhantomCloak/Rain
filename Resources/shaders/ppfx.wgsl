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

    let acesInput = textureColor * 0.6;
    let aces = acesFilm(acesInput);

    let gammaCorrected = pow(aces, vec3<f32>(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    //return vec4<f32>(aces, 1.0);
    //return vec4<f32>(aces, 1.0);
    return vec4<f32>(gammaCorrected, 1.0);
}
