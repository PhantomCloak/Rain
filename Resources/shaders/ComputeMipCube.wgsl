@group(0) @binding(0) var previousMipLevel: texture_2d_array<f32>;
@group(0) @binding(1) var nextMipLevel: texture_storage_2d_array<rgba16float, write>;

@compute @workgroup_size(32, 32, 1)
fn computeMipMap(@builtin(global_invocation_id) id: vec3<u32>) {
    let faceIndex: i32 = i32(id.z); 
    let texCoord: vec2<i32> = vec2<i32>(id.xy); 

    let prevMipDimensions: vec2<i32> = vec2<i32>(textureDimensions(previousMipLevel).xy);

    let coord = 2 * texCoord;
    let offsets = array<vec2<i32>, 4>(
        vec2<i32>(0, 0),
        vec2<i32>(1, 0),
        vec2<i32>(0, 1),
        vec2<i32>(1, 1)
    );

    var color: vec4<f32> = vec4<f32>(0.0);

    for (var i = 0; i < 4; i = i + 1) {
        let sampleCoord = coord + offsets[i];
        let clampedCoord = clamp(sampleCoord, vec2<i32>(0, 0), prevMipDimensions - vec2<i32>(1, 1));

        color = color + textureLoad(previousMipLevel, clampedCoord, faceIndex, 0);
    }

    color = color * 0.25; // Average the colors
    textureStore(nextMipLevel, texCoord, faceIndex, color);
}

