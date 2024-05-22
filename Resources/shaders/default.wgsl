struct VertexInput {
	@location(0) position: vec3f,
	@location(1) normal: vec3f,
	@location(2) uv: vec2f,
	@location(3) tangent: vec3f,
};

struct VertexOutput {
    @builtin(position) position: vec4f, 
    @location(1) uv: vec2f,              
    @location(2) TangentLightPos: vec3f,   
    @location(3) TangentFragPos: vec3f,     
    @location(4) FragPosLightSpace: vec4f,
}

struct Camera {
    projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
};

struct SceneUniform {
    modelMatrix: mat4x4f,
    color: vec4f,
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

@group(0) @binding(0) var<uniform> uScene: SceneUniform;

@group(0) @binding(1) var gradientTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;
@group(0) @binding(3) var heightTexture: texture_2d<f32>;
@group(0) @binding(4) var<uniform> uMaterial: MaterialUniform;

@group(1) @binding(0) var<uniform> uCam: Camera;

@group(2) @binding(0) var shadowMap: texture_depth_2d;
@group(2) @binding(1) var shadowSampler: sampler_comparison;
@group(2) @binding(2) var<uniform> shadowUniform: ShadowUniform;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;

	let fragPos = (uScene.modelMatrix * vec4f(in.position, 1.0)).xyz;

	out.position = uCam.projectionMatrix * uCam.viewMatrix * uScene.modelMatrix * vec4f(in.position, 1.0);
	out.uv = in.uv;

	let fragPos2 =  (uCam.viewMatrix * vec4f(fragPos, 1.0)).xyz;

	let normalMatrix = uCam.viewMatrix * uScene.modelMatrix;

	var T = normalize((normalMatrix * vec4f(in.tangent, 0.0)).xyz);
	let N = normalize((normalMatrix * vec4f(in.normal, 0.0)).xyz);

	T = normalize(T - dot(T, N) * N);
	let B = cross(N, T);

	let TBN: mat3x3f = transpose(mat3x3f(T, B, N));
	
	out.TangentLightPos = TBN * (uCam.viewMatrix * vec4f(shadowUniform.lightPos, 1.0)).xyz;
	out.TangentFragPos = TBN * fragPos2;
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
    let bias: f32 = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);

    var shadow: f32 = 0.0;

    let texelSize: vec2<f32> = vec2(1.0 / 2048.0);
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
	let textureColor = textureSample(gradientTexture, textureSampler, in.uv).rgba;
	var normal = textureSample(heightTexture, textureSampler, in.uv).rgb;
	normal =  normalize(normal * 2.0 - 1.0);

	// Ambient
	var ambient = uMaterial.ambientColor;

	// Diffuse
	let lightDir = normalize(in.TangentLightPos - in.TangentFragPos);
	var diffuse = max(dot(normal, lightDir), 0.0) * uMaterial.diffuseColor;

	// Specular
	let viewDir = normalize(-in.TangentFragPos);
	let halfwayDir = normalize(lightDir + viewDir);
	var specular = pow(max(dot(normal, halfwayDir), 0.0), uMaterial.shininess) * uMaterial.specularColor;

	var shadow = ShadowCalculation(in.FragPosLightSpace, normal , lightDir);
	let finalColor = (ambient + (shadow)  * (diffuse + specular)) * textureColor.rgb;

	return vec4f(finalColor, 1.0);
}

