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
	@location(5) WorldPos: vec4f,
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

@group(0) @binding(0) var<uniform> u_scene: SceneData;

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

    let worldPos = (transform * vec4f(in.a_position, 1.0)).xyz;
    out.WorldPos = vec4f(worldPos, 1.0);

    let normalMatrix = mat3x3<f32>(
        transform[0].xyz,
        transform[1].xyz,
        transform[2].xyz
    );

    out.Normal = normalize(normalMatrix * in.a_normal);
    out.Uv = in.a_uv;

    out.pos = u_scene.viewProjection * vec4f(worldPos, 1.0);

    let shadowCoords0 = u_ShadowData.ShadowViewProjection[0] * vec4f(worldPos, 1.0);
    let shadowCoords1 = u_ShadowData.ShadowViewProjection[1] * vec4f(worldPos, 1.0);
    let shadowCoords2 = u_ShadowData.ShadowViewProjection[2] * vec4f(worldPos, 1.0);
    let shadowCoords3 = u_ShadowData.ShadowViewProjection[3] * vec4f(worldPos, 1.0);

    out.ShadowCoord0 = shadowCoords0.xyz / shadowCoords0.w;
    out.ShadowCoord1 = shadowCoords1.xyz / shadowCoords1.w;
    out.ShadowCoord2 = shadowCoords2.xyz / shadowCoords2.w;
    out.ShadowCoord3 = shadowCoords3.xyz / shadowCoords3.w;

	out.FragPos = (u_scene.cameraViewMatrix * vec4f(worldPos, 1.0)).xyz;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    let albedo = textureSample(u_AlbedoTex, u_TextureSampler, in.Uv).rgb;

    let ambientStrength = 0.25;
    let ambient = ambientStrength * albedo;

    let norm = normalize(in.Normal);
	let lightDir = u_scene.LightDirection;

    let lightColor = vec3f(1.0, 1.0, 1.0);
    let diff = max(dot(norm, lightDir), 0.0);
    let diffuse = diff * lightColor * albedo;

    let SHADOW_MAP_CASCADE_COUNT = 4u;
    var layer = SHADOW_MAP_CASCADE_COUNT - 1u;

    for (var i = SHADOW_MAP_CASCADE_COUNT - 1u; i > 0u; i = i - 1u) {
        if (-in.FragPos.z < u_ShadowData.CascadeDistances[i - 1u]) {
            layer = i - 1u;
        }
    }

    let shadowCoords = GetShadowMapCoords(in, layer);
    let MIN_BIAS = 0.005;
    let bias = max(MIN_BIAS * (1.0 - dot(norm, lightDir)), MIN_BIAS);
    var shadow: f32 = textureSampleCompare(u_ShadowMap, u_ShadowSampler, shadowCoords.xy * vec2(0.5, -0.5) + vec2(0.5), layer, shadowCoords.z - bias);

    return vec4f(ambient + shadow * diffuse, 1.0);
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
