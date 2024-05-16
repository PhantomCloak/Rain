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
	@location(6) FragPosLightSpace: vec4f,
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

struct ShadowUniform {
    lightProjection: mat4x4f,
    lightView: mat4x4f,
    lightPos: vec3<f32>,
};

@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;

@group(1) @binding(0) var<uniform> uCam: Camera;

@group(2) @binding(0) var shadowMap: texture_depth_2d;
@group(2) @binding(1) var shadowSampler: sampler_comparison;
@group(2) @binding(2) var<uniform> shadowUniform: ShadowUniform;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = uCam.projectionMatrix * uCam.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
	out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz; // Consider removing if normals are no longer used.
	out.color = vec3f(1);
	out.uv = in.uv;

	out.LightPos = (uCam.viewMatrix * vec4f(shadowUniform.lightPos, 1.0)).xyz;
	out.Normal =   (uCam.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.FragPos =  (uCam.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0)).xyz;

	let fragPos = (uMyUniforms.modelMatrix * vec4f(in.position, 1.0)).xyz;
	out.FragPosLightSpace = (shadowUniform.lightProjection * shadowUniform.lightView) * vec4f(fragPos, 1.0);

	return out;
}

fn ShadowCalculation(
	fragPosLightSpace: vec4<f32>,
	normal: vec3<f32>,
	shadowMap: texture_depth_2d) -> f32 {

    var projCoords: vec3<f32> = fragPosLightSpace.xyz;

    projCoords = vec3(projCoords.xy * vec2(0.5, -0.5) + vec2(0.5), projCoords.z);

		let currentDepth: f32 = projCoords.z;
		let bias: f32 = 0.005;

		//let shadow: f32 = textureSampleCompare(shadowMap, shadowSampler, projCoords.xy, currentDepth - bias);
		let shadow: f32 = textureSampleCompare(shadowMap, shadowSampler, projCoords.xy, currentDepth - bias);

		return shadow;
}

fn LinearizeDepth(depth: f32, near_plane: f32, far_plane: f32) -> f32 {
    // Linear depth calculation for orthographic projection
    return depth * (far_plane - near_plane) + near_plane;
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

	//if(textureColor.a < 0.5) {
	//	discard;
	//}

	// Ambient
	var ambient = vec3f(0.3);

	// Diffuse
	let norm = normalize(in.Normal);
	let lightDir = normalize(in.LightPos - in.FragPos);
	var diffuse = vec3f(max(dot(norm, lightDir), 0.0));

	// Specular
	let viewDir = normalize(-in.FragPos);
	let halfwayDir = normalize(lightDir + viewDir);
	var specular = vec3f(pow(max(dot(norm, halfwayDir), 0.0), 64));

	//ambient  *= attenuation; 
	//diffuse  *= attenuation;
	//specular *= attenuation;

	var shadow = ShadowCalculation(in.FragPosLightSpace, norm, shadowMap);
	let finalColor = (ambient + (shadow)  * (diffuse + specular)) * textureColor.rgb;

	return vec4f(finalColor, 1.0);
}

