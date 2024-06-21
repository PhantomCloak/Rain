struct VertexInput {
	@location(0) a_position: vec3f,
	@location(1) a_normal: vec3f,
	@location(2) a_uv: vec2f,
};

struct InstanceInput {
	@location(4) a_MRow0: vec4<f32>,
	@location(5) a_MRow1: vec4<f32>,
	@location(6) a_MRow2: vec4<f32>,
}

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(2) Normal: vec3f,
	@location(3) Uv: vec2f,
	@location(4) LightPos: vec3f,
	@location(5) FragPos: vec3f,
	@location(6) FragPosLightSpace: vec4f,
};

struct SceneData {
	viewProjection: mat4x4f,
	shadowViewProjection: mat4x4f,
	cameraViewMatrix: mat4x4f,
	lightPos: vec3<f32>,

};

struct MaterialUniform {
    ambientColor: vec3f,
    diffuseColor: vec3f,
    specularColor: vec3f,
	shininess: f32
};

@group(0) @binding(0) var<uniform> u_scene: SceneData;

@group(1) @binding(0) var gradientTexture: texture_2d<f32>;
@group(1) @binding(1) var textureSampler: sampler;
@group(1) @binding(2) var<uniform> uMaterial: MaterialUniform;

@group(2) @binding(0) var shadowMap: texture_depth_2d;
@group(2) @binding(1) var shadowSampler: sampler_comparison;

@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
	var out: VertexOutput;

	let transform = mat4x4<f32>(
			vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
			vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
			vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
			vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
	);

	let fragPos = (transform * vec4f(in.a_position, 1.0)).xyz;
	out.position = u_scene.viewProjection * transform * vec4f(in.a_position, 1.0);
	//out.Normal = in.a_normal;
	out.Normal = (u_scene.cameraViewMatrix * transform * vec4f(in.a_normal, 0.0)).xyz;

	out.Uv = in.a_uv;

	out.LightPos = (u_scene.cameraViewMatrix * vec4f(u_scene.lightPos, 1.0)).xyz;
	out.FragPos = (u_scene.cameraViewMatrix * vec4f(fragPos, 1.0)).xyz;
	out.FragPosLightSpace = u_scene.shadowViewProjection * vec4f(fragPos, 1.0);

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
    //let bias: f32 = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);
    let bias: f32 = max(0.0005 * (1.0 - dot(normal, lightDir)), 0.0001);

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
	let textureColor = textureSample(gradientTexture, textureSampler, in.Uv).rgba;
	var ambient = vec3f(0.1);

	// Diffuse
	let norm = normalize(in.Normal);
	let lightDir = normalize(in.LightPos - in.FragPos);
	var diffuse = vec3f(max(dot(norm, lightDir), 0.0)) * uMaterial.diffuseColor;

	// Specular
	let viewDir = normalize(-in.FragPos);
	let halfwayDir = normalize(lightDir + viewDir);
	var specular = vec3f(pow(max(dot(norm, halfwayDir), 0.0), uMaterial.shininess));

	var shadow = ShadowCalculation(in.FragPosLightSpace, in.Normal , lightDir);
	let finalColor = (ambient + (shadow)  * (diffuse + specular)) * textureColor.rgb;
	//let finalColor = (ambient + diffuse + specular) * textureColor.rgb;
	return vec4f(finalColor, 1.0);
}
