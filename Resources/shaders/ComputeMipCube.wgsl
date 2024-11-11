@group(0) @binding(0) var previousMipLevel: texture_cube<f32>;
@group(0) @binding(1) var nextMipLevel: texture_storage_cube<rgba8unorm, write>;

@compute @workgroup_size(8, 8, 1)
fn computeMipMap(@builtin(global_invocation_id) id: vec3<u32>) {
    let faceIndex = id.z;
    let texCoord = vec2<u32>(id.x, id.y);
    let offset = vec2<u32>(0u, 1u);
    
    // Sampling the four neighboring texels for averaging
    let color = (
        textureLoad(previousMipLevel, faceIndex, 2u * texCoord + offset.xx, 0) +
        textureLoad(previousMipLevel, faceIndex, 2u * texCoord + offset.xy, 0) +
        textureLoad(previousMipLevel, faceIndex, 2u * texCoord + offset.yx, 0) +
        textureLoad(previousMipLevel, faceIndex, 2u * texCoord + offset.yy, 0)
    ) * 0.25;

    // Storing the mipmapped color in the next mip level of the cube map
    textureStore(nextMipLevel, faceIndex, texCoord, color);
}
