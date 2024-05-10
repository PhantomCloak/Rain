struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) color: vec3f,
	@location(3) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
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
    out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = in.color;
	out.uv = in.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    // Constants for lighting
    let lightDir: vec3f = normalize(vec3f(1.0, -0.6, 0.0)); // Light direction
    let viewDir: vec3f = normalize(vec3f(0.0, 0.0, 1.0));  // Viewer direction
    let lightColor: vec3f = vec3f(1.0, 1.0, 1.0);          // White light
    let ambientIntensity: vec3f = vec3f(0.8, 0.8, 0.8);    // Low intensity ambient light
    let specularStrength: f32 = 0.5;                        // Specular intensity
    let shininess: f32 = 32.0;                              // Shininess factor

    // Get data from the texture using our sampler
    let textureColor = textureSample(gradientTexture, textureSampler, in.uv).rgb;

    // Calculate the diffuse component
    let norm = normalize(in.normal);
    let diff = max(dot(norm, lightDir), 0.0);

    // Calculate the specular component
    let reflectDir = reflect(-lightDir, norm);
    let spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    let specular = specularStrength * spec * lightColor;

    // Combine ambient, diffuse, and specular components
    let lighting = ambientIntensity + (diff + specular) * lightColor;

    // Apply the lighting to the texture color
    let litColor = textureColor * lighting;

    // Gamma-correction
    let corrected_color = pow(litColor, vec3f(2.2));

    return vec4f(corrected_color, uMyUniforms.color.a);
}

