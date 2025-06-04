struct VertexInput {
    @location(0) position: vec3f,
    @location(1) uv: vec2f,
};

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) v_Position: vec3f,
};

struct CameraData {
    InverseViewProjectionMatrix: mat4x4<f32>
};

// Cubemap texture declaration
@group(0) @binding(1) var u_Texture: texture_cube<f32>;
@group(0) @binding(2) var textureSampler: sampler;
@group(1) @binding(0) var<uniform> u_Camera: CameraData;

// Hardcoded constants instead of uniforms
const TextureLod: f32 = 0.0;  // Base mipmap level
const Intensity: f32 = 1.0;   // Full intensity

@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var output: VertexOutput;
    // Full screen quad
    var position = vec4f(input.position.xy, 0.0, 1.0);
    output.position = position;
    // Get the view direction in world space
    output.v_Position = (u_Camera.InverseViewProjectionMatrix * position).xyz;
    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    // Sample the cubemap with constant LOD
    var color = textureSampleLevel(
        u_Texture, 
        textureSampler, 
        normalize(in.v_Position), 
        TextureLod
    ) * Intensity;
    
    // Set alpha to 1.0
    color.a = 1.0;
    
    return color;
}
