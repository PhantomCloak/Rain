const PI: f32 = 3.14159265359;
// Binding group
@group(0) @binding(0) var o_CubeMap: texture_storage_2d_array<rgba16float, write>;
@group(0) @binding(1) var u_EquirectangularTex: texture_2d<f32>;
@group(0) @binding(2) var u_Sampler: sampler;

// Function to get cube map texture coordinates
fn GetCubeMapTexCoord(size: f32, global_id: vec3<u32>) -> vec3<f32> {
    let face = global_id.z;
    let uvx = (f32(global_id.x) + 0.5) / size;
    let uvy = (f32(global_id.y) + 0.5) / size;
    
    // Map UV in [0,1] range to [-1,1] range
    let uv = vec2<f32>(2.0 * uvx - 1.0, 2.0 * uvy - 1.0);
    
    var coords: vec3<f32>;
    
    // Convert 2D coordinates to 3D direction based on cube face
    switch face {
        // +X face
        case 0u: {
            coords = vec3<f32>(1.0, -uv.y, -uv.x);
        }
        // -X face
        case 1u: {
            coords = vec3<f32>(-1.0, -uv.y, uv.x);
        }
        // +Y face
        case 2u: {
            coords = vec3<f32>(uv.x, 1.0, uv.y);
        }
        // -Y face
        case 3u: {
            coords = vec3<f32>(uv.x, -1.0, -uv.y);
        }
        // +Z face
        case 4u: {
            coords = vec3<f32>(uv.x, -uv.y, 1.0);
        }
        // -Z face
        case 5u: {
            coords = vec3<f32>(-uv.x, -uv.y, -1.0);
        }
        default: {
            coords = vec3<f32>(0.0);
        }
    }
    
    return normalize(coords);
}

@compute @workgroup_size(16, 16, 1)
fn main(@builtin(global_invocation_id) global_id: vec3<u32>) {
    // Get dimensions of the cubemap texture (2D size of each face)
    let dimensions = textureDimensions(o_CubeMap);
    let size = f32(dimensions.x); // Assuming cube faces are square

    // Check if the current invocation is within the cube face bounds
    if (global_id.x >= dimensions.x || global_id.y >= dimensions.y || global_id.z >= 6u) {
        return;
    }

    // Get the direction vector for this cubemap texel
    let cubeTC = GetCubeMapTexCoord(size, global_id);

    // Calculate sampling coords for equirectangular texture
    let phi = atan2(cubeTC.z, cubeTC.x);
    let theta = acos(cubeTC.y);

    // Convert to UV coordinates for the equirectangular texture
    let uv = vec2<f32>(phi / (2.0 * PI) + 0.5, theta / PI);

    // Sample the equirectangular texture and store raw HDR values.
    // Tonemapping must NOT happen here — prefilter and irradiance compute
    // need raw radiance for energy-correct IBL. Tonemap at display time instead.
    let color = textureSampleLevel(u_EquirectangularTex, u_Sampler, uv, 0.0);

    // Store the result in the cubemap (each layer is a cube face)
    textureStore(o_CubeMap, vec2<i32>(i32(global_id.x), i32(global_id.y)), i32(global_id.z), color);
}
