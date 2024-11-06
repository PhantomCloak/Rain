struct VertexInput {
	@location(0) a_position: vec3f,
	@location(1) a_normal: vec3f,
	@location(2) a_uv: vec2f,
	@location(3) a_tangent: vec3f
};

struct InstanceInput {
	@location(4) a_MRow0: vec4<f32>,
	@location(5) a_MRow1: vec4<f32>,
	@location(6) a_MRow2: vec4<f32>,
}

struct VertexOutput {
	@builtin(position) pos: vec4f,
	@location(2) Normal: vec3f,
	@location(3) Uv: vec2f,
	@location(4) FragPos: vec3f,
	@location(5) WorldPos: vec3f,
    @location(6) ShadowCoord0: vec3f,
    @location(7) ShadowCoord1: vec3f,
    @location(8) ShadowCoord2: vec3f,
    @location(9) ShadowCoord3: vec3f,
};

struct SceneData {
	viewProjection: mat4x4f,
	cameraViewMatrix: mat4x4f,
	CameraPosition: vec3<f32>,
	LightDirection: vec3<f32>
};

struct ShadowData {
	ShadowViewProjection: array<mat4x4<f32>, 4>,
	CascadeDistances: vec4<f32>
};

struct MaterialUniform {
    Metallic: f32,
    Roughness: f32,
    Ao: f32,
	UseNormalMap: i32
};

@group(0) @binding(0) var<uniform> u_Scene: SceneData;

@group(1) @binding(0) var u_AlbedoTex: texture_2d<f32>;
@group(1) @binding(1) var u_TextureSampler: sampler;
@group(1) @binding(2) var<uniform> uMaterial: MaterialUniform;
@group(1) @binding(3) var u_MetallicTex: texture_2d<f32>;
@group(1) @binding(4) var u_NormalTex: texture_2d<f32>;

@group(2) @binding(0) var u_ShadowMap: texture_depth_2d_array;
@group(2) @binding(1) var u_ShadowSampler: sampler_comparison;
@group(2) @binding(2) var<uniform> u_ShadowData: ShadowData;

@group(3) @binding(0) var irradianceMap: texture_cube<f32>;
@group(3) @binding(1) var irradianceMapSampler: sampler;


@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
    var out: VertexOutput;

    let transform = mat4x4<f32>(
        vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
        vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
        vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
        vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
    );

    let worldPos = transform * vec4f(in.a_position, 1.0);
    out.WorldPos = worldPos.xyz;

    let normalMatrix = mat3x3<f32>(
        transform[0].xyz,
        transform[1].xyz,
        transform[2].xyz
    );

    out.Normal = normalize(normalMatrix * in.a_normal);
    out.Uv = in.a_uv;

    out.pos = u_Scene.viewProjection * worldPos;

    let shadowCoords0 = u_ShadowData.ShadowViewProjection[0] * worldPos;
    let shadowCoords1 = u_ShadowData.ShadowViewProjection[1] * worldPos;
    let shadowCoords2 = u_ShadowData.ShadowViewProjection[2] * worldPos;
    let shadowCoords3 = u_ShadowData.ShadowViewProjection[3] * worldPos;

    out.ShadowCoord0 = shadowCoords0.xyz / shadowCoords0.w;
    out.ShadowCoord1 = shadowCoords1.xyz / shadowCoords1.w;
    out.ShadowCoord2 = shadowCoords2.xyz / shadowCoords2.w;
    out.ShadowCoord3 = shadowCoords3.xyz / shadowCoords3.w;

	out.FragPos = (u_Scene.cameraViewMatrix * vec4f(out.WorldPos, 1.0)).xyz;

    return out;
}

fn sampleShadow(in: VertexOutput, cascadeIndex: u32, bias: f32) -> f32 {
    let shadowCoords = GetShadowMapCoords(in, cascadeIndex);
    let projCoords = shadowCoords.xy * vec2(0.5, -0.5) + vec2(0.5);
    let texelSize: vec2<f32> = vec2(1.0 / 4096.0); // Adjust based on your shadow map resolution
    let halfKernelWidth: i32 = 1;

    var shadow: f32 = 0.0;
    let totalSamples: f32 = f32((halfKernelWidth * 2 + 1) * (halfKernelWidth * 2 + 1));

    for (var x: i32 = -halfKernelWidth; x <= halfKernelWidth; x = x + 1) {
        for (var y: i32 = -halfKernelWidth; y <= halfKernelWidth; y = y + 1) {
            let offset = vec2<f32>(f32(x), f32(y)) * texelSize;
            //let sampleCoords = projCoords + offset;
			let sampleCoords = clamp(projCoords + offset, vec2(0.0), vec2(1.0));

            let inBoundsX = step(0.0, sampleCoords.x) * (1.0 - step(1.0, sampleCoords.x));
            let inBoundsY = step(0.0, sampleCoords.y) * (1.0 - step(1.0, sampleCoords.y));
            let inBounds = inBoundsX * inBoundsY;

            let depthComparison = textureSampleCompare(
                u_ShadowMap,
                u_ShadowSampler,
                sampleCoords,
                cascadeIndex,
                shadowCoords.z - bias
            );

            let adjustedDepthComparison = inBounds * depthComparison + (1.0 - inBounds) * 1.0;

            shadow += adjustedDepthComparison;
        }
    }

    shadow /= totalSamples;
    return shadow;
}


@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let albedo = textureSample(u_AlbedoTex, u_TextureSampler, in.Uv).rgb;
    let norm = normalize(in.Normal);
    let lightDir = normalize(u_Scene.LightDirection);
    let viewDepth = -in.FragPos.z;

    let ambientStrength = 1.0;
    let ambientColor = vec3f(0.32, 0.3, 0.25); 
    let ambient = ambientStrength * ambientColor * albedo;
    let lightColor = vec3f(1.0, 1.0, 1.0);
    let diff = max(dot(norm, lightDir), 0.0);
    let diffuse = diff * lightColor * albedo;

    let MIN_BIAS = 0.005;
    let bias = max(MIN_BIAS * (1.0 - dot(norm, lightDir)), MIN_BIAS);
	let cascadeTransitionFade = 1.0;

	let SHADOW_MAP_CASCADE_COUNT = 4u;
	var layer = 0u;
	for (var i = 0u; i < SHADOW_MAP_CASCADE_COUNT - 1u; i = i + 1u) {
		if (viewDepth > u_ShadowData.CascadeDistances[i]) {
			layer = i + 1u;
		}
	}

    //let cascadeColors = array<vec3<f32>, 4>(
    //    vec3<f32>(1.0, 0.0, 0.0),  // Red
    //    vec3<f32>(0.0, 1.0, 0.0),  // Green
    //    vec3<f32>(0.0, 0.0, 1.0),  // Blue
    //    vec3<f32>(1.0, 1.0, 0.0)   // Yellow
    //);

    //var cascadeColor: vec3<f32> = vec3<f32>(1.0);
	//cascadeColor = cascadeColors[layer];

	let shadowScale = sampleShadow(in, layer, bias);
    let finalColor = ambient + shadowScale * diffuse;

    return vec4f(finalColor, 1.0);
}

fn GetShadowMapCoords(
	in: VertexOutput,
    cascade: u32
) -> vec3<f32> {
    switch (cascade) {
        case 0: { return in.ShadowCoord0; }
        case 1: { return in.ShadowCoord1; }
        case 2: { return in.ShadowCoord2; }
        case 3: { return in.ShadowCoord3; }
        default: { return vec3<f32>(0.0); }
    }
}
