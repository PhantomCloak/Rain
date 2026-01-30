struct VertexInput {
    @location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) uv: vec2f,
    @location(3) tangent: vec3f,
    @location(4) bitangent: vec3f,
    @location(5) boneIndices: vec4<u32>,
    @location(6) boneWeights: vec4f,
};

struct InstanceInput {
    @location(7) a_MRow0: vec4<f32>,
    @location(8) a_MRow1: vec4<f32>,
    @location(9) a_MRow2: vec4<f32>,
}

struct VertexOutput {
    @builtin(position) position: vec4f,
};

struct ShadowData {
    ShadowViewProjection: array<mat4x4<f32>, 4>,
    CascadeDistances: vec4<f32>
};

@group(0) @binding(0) var<uniform> u_ShadowData: ShadowData;
@group(0) @binding(1) var<storage, read> u_BoneMatrices: array<mat4x4<f32>, 128>;

override co: u32 = 0;

@vertex
fn vs_main(in: VertexInput, instance: InstanceInput) -> VertexOutput {
    var out: VertexOutput;

    let modelMatrix = mat4x4<f32>(
        vec4<f32>(instance.a_MRow0.x, instance.a_MRow1.x, instance.a_MRow2.x, 0.0),
        vec4<f32>(instance.a_MRow0.y, instance.a_MRow1.y, instance.a_MRow2.y, 0.0),
        vec4<f32>(instance.a_MRow0.z, instance.a_MRow1.z, instance.a_MRow2.z, 0.0),
        vec4<f32>(instance.a_MRow0.w, instance.a_MRow1.w, instance.a_MRow2.w, 1.0)
    );

    // Compute skin matrix from bone transforms
    let skinMatrix = u_BoneMatrices[in.boneIndices.x] * in.boneWeights.x
                   + u_BoneMatrices[in.boneIndices.y] * in.boneWeights.y
                   + u_BoneMatrices[in.boneIndices.z] * in.boneWeights.z
                   + u_BoneMatrices[in.boneIndices.w] * in.boneWeights.w;

    // Apply skinning then model transform
    let skinnedPos = skinMatrix * vec4f(in.position, 1.0);
    let worldPos = modelMatrix * skinnedPos;

    out.position = u_ShadowData.ShadowViewProjection[co] * worldPos;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
    return vec4f(1.0);
}