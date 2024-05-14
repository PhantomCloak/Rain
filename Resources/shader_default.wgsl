struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f, // Consider removing if normals are no longer used.
	@location(2) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f, // Consider removing if normals are no longer used.
	@location(2) uv: vec2f,

	@location(3) LightPos: vec3f,
	@location(4) Normal: vec3f,
	@location(5) FragPos: vec3f,
};

/**
 * A structure holding the value of our uniforms
 */
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

	let lightPosConstant = vec3f(0, 50, 0);
	out.LightPos = (uCam.viewMatrix * vec4f(lightPosConstant, 1.0)).xyz;
	out.Normal =   (uCam.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.FragPos =  (uCam.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0)).xyz;

	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // Get data from the texture using our sampler

	let constant = 1.0;
	let linear = 0.007;
	let quadratic = 0.0002;

	let distance = length(in.LightPos - in.FragPos);
	let attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    let textureColor = textureSample(gradientTexture, textureSampler, in.uv).rgba;

	if(textureColor.a < 0.5) {
		discard;
	}

	// Ambient
	var ambient = vec3f(0.1);

	// Diffuse
	let norm = normalize(in.Normal);
	let lightDir = normalize(in.LightPos - in.FragPos);
	var diffuse = vec3f(max(dot(norm, lightDir), 0.0));

	// Specular
	let viewDir = normalize(-in.FragPos);
	let halfwayDir = normalize(lightDir + viewDir);
	var specular = vec3f(pow(max(dot(norm, halfwayDir), 0.0), 64));

	ambient  *= attenuation; 
	diffuse  *= attenuation;
	specular *= attenuation;

    let finalColor = (ambient + diffuse + specular) * textureColor.rgb;

    return vec4f(finalColor, 1.0);
}

