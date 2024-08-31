struct VertexInput {
	@location(0) position: vec3f,
	@location(1) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(1) v_Position: vec2f,
};

struct CameraData {
	InverseViewProjectionMatrix: mat4x4
};

@group(0) @binding(0) var cubemapTexture: texture_cube<f32>;
@group(0) @binding(1) var textureSampler: sampler;


@vertex
fn vs_main(input : VertexInput) -> VertexOutput {
    var output : VertexOutput;
    output.uv = input.uv;
    output.position = vec4(input.position.xy, 0.0, 1.0);
    return output;

	//v_Position = (u_Camera.InverseViewProjectionMatrix * position).xyz;
}

	//vec4 position = vec4(a_Position.xy, 0.0, 1.0);
	//gl_Position = position;

	//v_Position = (u_Camera.InverseViewProjectionMatrix * position).xyz;

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    //let textureColor = textureSample(, textureSampler, in.uv).rgb;

    //let acesInput = textureColor * 0.6;
    ///let aces = acesFilm(acesInput);

    //let gammaCorrected = pow(aces, vec3<f32>(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    //return vec4<f32>(aces, 1.0);
    //return vec4<f32>(aces, 1.0);
    return vec4<f32>(gammaCorrected, 1.0);
}
