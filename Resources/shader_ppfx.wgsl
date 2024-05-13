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
    // Get data from the texture using our sampler
    let textureColor = textureSample(renderTexture, textureSampler, in.uv).rgb;

    // Calculate luminance of the texture color
    let luminance = 0.2126 * textureColor.r + 0.7152 * textureColor.g + 0.0722 * textureColor.b;

    // Create a grayscale color by setting RGB components to luminance
    let grayscaleColor = vec3<f32>(luminance, luminance, luminance);

    // Return the final color with alpha
    return vec4<f32>(grayscaleColor, 1.0);
}

