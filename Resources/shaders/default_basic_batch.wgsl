struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(2) Normal: vec3f,
	@location(3) uv: vec2f,
	@location(4) LightPos: vec3f,
	@location(5) FragPos: vec3f,
	@location(6) FragPosLightSpace: vec4f,
};

struct Camera {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};


struct MaterialUniform {
    ambientColor: vec3f,
    diffuseColor: vec3f,
    specularColor: vec3f,
	shininess: f32
};

struct ShadowUniform {
    lightProjection: mat4x4f,
    lightView: mat4x4f,
    lightPos: vec3<f32>,
};

struct SceneUniform {
	modelMatrix : mat4x4f,
    color: vec4f,
};

@group(0) @binding(0) var<storage> uScene: array<SceneUniform>;
@group(1) @binding(0) var<uniform> uCam: Camera;

@group(2) @binding(0) var gradientTexture: texture_2d<f32>;
@group(2) @binding(1) var textureSampler: sampler;
@group(2) @binding(2) var<uniform> uMaterial: MaterialUniform;

@group(3) @binding(0) var shadowMap: texture_depth_2d;
@group(3) @binding(1) var shadowSampler: sampler_comparison;
@group(3) @binding(2) var<uniform> shadowUniform: ShadowUniform;

@vertex
fn vs_main(@builtin(instance_index) instanceIdx : u32, in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

	let fragPos = (uScene[instanceIdx].modelMatrix * vec4f(in.position, 1.0)).xyz;

	out.position = uCam.projectionMatrix * uCam.viewMatrix * uScene[instanceIdx].modelMatrix * vec4f(in.position, 1.0);
	out.Normal =   (uCam.viewMatrix * uScene[instanceIdx].modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.uv = in.uv;

	out.LightPos = (uCam.viewMatrix * vec4f(shadowUniform.lightPos, 1.0)).xyz;
	out.FragPos =  (uCam.viewMatrix * vec4f(fragPos, 1.0)).xyz;
	out.FragPosLightSpace = (shadowUniform.lightProjection * shadowUniform.lightView) * vec4f(fragPos, 1.0);

	return out;
}

fn ShadowCalculation(
		fragPosLightSpace: vec4<f32>,
		normal: vec3<f32>,
		lightDir: vec3<f32>
		) -> f32 {
	var projCoords: vec3<f32> = fragPosLightSpace.xyz;
	projCoords = vec3(projCoords.xy * vec2(0.5, -0.5) + vec2(0.5), projCoords.z);

	let currentDepth: f32 = projCoords.z;
	let bias: f32 = max(0.009 * (1.0 - dot(normal, lightDir)), 0.0005);

	var visibility = 0.0;
	let oneOverShadowDepthTextureSize = 1.0 / 4096;
	for (var y = -1; y <= 1; y++) {
		for (var x = -1; x <= 1; x++) {
			let offset = vec2f(vec2(x, y)) * oneOverShadowDepthTextureSize;
			visibility += textureSampleCompare(
					shadowMap, shadowSampler,
					projCoords.xy + offset, currentDepth - bias
					);
		}
	}
	visibility /= 9.0;

	return visibility;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let textureColor = textureSample(gradientTexture, textureSampler, in.uv).rgba;
	var ambient = vec3f(0.1);
	//var ambient = uMaterial.ambientColor;

	// Diffuse
	let norm = normalize(in.Normal);
	let lightDir = normalize(in.LightPos - in.FragPos);
	var diffuse = vec3f(max(dot(norm, lightDir), 0.0)) * uMaterial.diffuseColor;

	// Specular
	let viewDir = normalize(-in.FragPos);
	let halfwayDir = normalize(lightDir + viewDir);
	var specular = vec3f(pow(max(dot(norm, halfwayDir), 0.0), uMaterial.shininess));

	var shadow = ShadowCalculation(in.FragPosLightSpace, norm, lightDir);
	shadow = shadow * 0.8 + 0.2;

	let shadowedAmbient = shadow * ambient;
	let shadowedDiffuse = shadow * diffuse;
	let shadowedSpecular = shadow * specular;

	let finalColor = (ambient + shadowedDiffuse + shadowedSpecular) * textureColor.rgb;
	//let finalColor = (ambient + diffuse + specular) * textureColor.rgb;
	return vec4f(finalColor, 1.0);
}
