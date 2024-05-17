struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) tangent: vec3f,
	@location(4) bittangent: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(2) Normal: vec3f,
	@location(3) uv: vec2f,
	@location(4) LightPos: vec3f,
	@location(5) FragPos: vec3f,
	@location(6) FragPosLightSpace: vec4f,
	@location(7) T: vec3f,
	@location(8) B: vec3f,
	@location(9) N: vec3f
}

struct Camera {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};

struct SceneUniform {
    modelMatrix: mat4x4f,
    color: vec4f,
};

struct ShadowUniform {
    lightProjection: mat4x4f,
    lightView: mat4x4f,
    lightPos: vec3<f32>,
};

@group(0) @binding(0) var<uniform> uScene: SceneUniform;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;
@group(0) @binding(3) var heightTexture: texture_2d<f32>;

@group(1) @binding(0) var<uniform> uCam: Camera;

@group(2) @binding(0) var shadowMap: texture_depth_2d;
@group(2) @binding(1) var shadowSampler: sampler_comparison;
@group(2) @binding(2) var<uniform> shadowUniform: ShadowUniform;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

	let fragPos = (uScene.modelMatrix * vec4f(in.position, 1.0)).xyz;

	out.position = uCam.projectionMatrix * uCam.viewMatrix * uScene.modelMatrix * vec4f(in.position, 1.0);
	out.Normal =   (uCam.viewMatrix * uScene.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.uv = in.uv;

	out.LightPos = (uCam.viewMatrix * vec4f(shadowUniform.lightPos, 1.0)).xyz;
	out.FragPos =  (uCam.viewMatrix * vec4f(fragPos, 1.0)).xyz;
	out.FragPosLightSpace = (shadowUniform.lightProjection * shadowUniform.lightView) * vec4f(fragPos, 1.0);

	out.T = normalize(((uScene.modelMatrix) * vec4f(in.tangent, 0.0)).xyz);
	out.N = normalize(((uScene.modelMatrix) * vec4f(in.normal, 0.0)).xyz);
	out.T = normalize(out.T - dot(out.T, out.N) * out.N);
	out.B = cross(out.N, out.T);

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
    let bias: f32 = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);

    var shadow: f32 = 0.0;

    let texelSize: vec2<f32> = vec2(1.0 / 4096.0);
    const halfKernelWidth: i32 = 1;

    for (var x: i32 = -halfKernelWidth; x <= halfKernelWidth; x++) {
        for (var y: i32 = -halfKernelWidth; y <= halfKernelWidth; y++) {
            let sampleCoords: vec2<f32> = projCoords.xy + vec2<f32>(f32(x), f32(y)) * texelSize;
            let pcfDepth: f32 = textureSampleCompare(shadowMap, shadowSampler, sampleCoords, currentDepth - bias);
            shadow += pcfDepth;
        }
    }
    shadow /= f32((halfKernelWidth * 2 + 1) * (halfKernelWidth * 2 + 1));

    return shadow;
}


@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	// Get data from the texture using our sampler
	let textureColor = textureSample(gradientTexture, textureSampler, in.uv).rgba;

	let TBN = mat3x3f(in.T, in.B, in.N);

	var normal: vec3f;
	normal = textureSample(heightTexture, textureSampler, in.uv).rgb;
	normal = normal * 2.0 - 1.0;
	normal = normalize(TBN * normal);
	
	//let normal = normalize(in.Normal);
	// Ambient
	var ambient = vec3f(0.1);

	// Diffuse
	let lightDir = normalize(in.LightPos - in.FragPos);
	var diffuse = vec3f(max(dot(normal, lightDir), 0.0));

	// Specular
	let viewDir = normalize(-in.FragPos);
	let halfwayDir = normalize(lightDir + viewDir);
	var specular = vec3f(pow(max(dot(normal, halfwayDir), 0.0), 64));

	var shadow = ShadowCalculation(in.FragPosLightSpace, normal, lightDir);
	let finalColor = (ambient + (shadow)  * (diffuse + specular)) * textureColor.rgb;

	return vec4f(finalColor, 1.0);
}

