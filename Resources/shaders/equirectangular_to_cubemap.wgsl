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

// Simple tone mapping function to make HDR content visible
fn toneMap(color: vec4<f32>, exposure: f32) -> vec4<f32> {
    let tone_mapped = vec3<f32>(1.0) - exp(-color.rgb * exposure);
    return vec4<f32>(tone_mapped, color.a);
}

@compute @workgroup_size(32, 32, 1)
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
    var uv = vec2<f32>(phi / (2.0 * PI) + 0.5, theta / PI);
    
    // Sample the equirectangular texture
    let color = textureSampleLevel(u_EquirectangularTex, u_Sampler, uv, 0.0);
    
    // Debug: Visualization constants
    let debug_enabled = false; // Set to true to see debug visualization
    let exposure = 1.0;        // Adjust exposure for HDR content
    
    var final_color = color;
    
    if (debug_enabled) {
        // Debug color based on face ID
        let debug_colors = array<vec4<f32>, 6>(
            vec4<f32>(1.0, 0.0, 0.0, 1.0), // +X: Red
            vec4<f32>(0.0, 1.0, 0.0, 1.0), // -X: Green
            vec4<f32>(0.0, 0.0, 1.0, 1.0), // +Y: Blue
            vec4<f32>(1.0, 1.0, 0.0, 1.0), // -Y: Yellow
            vec4<f32>(1.0, 0.0, 1.0, 1.0), // +Z: Magenta
            vec4<f32>(0.0, 1.0, 1.0, 1.0)  // -Z: Cyan
        );
        
        // UV visualization to check texcoord mapping
        let checker_size = 8.0;
        let is_odd = (floor(uv.x * checker_size) + floor(uv.y * checker_size)) % 2.0;
        let checker = mix(0.3, 1.0, is_odd);
        
        // Mix between actual color, checkerboard, and face color
        let debug_uv = vec4<f32>(uv, 0.0, 1.0);
        let debug_checker = vec4<f32>(checker, checker, checker, 1.0);
        final_color = mix(debug_colors[global_id.z], mix(color, debug_checker, 0.5), 0.5);
    } else {
        // Apply exposure adjustment for HDR content
        final_color = toneMap(color, exposure);
    }
    
    // Store the result in the cubemap (each layer is a cube face)
    textureStore(o_CubeMap, vec2<i32>(i32(global_id.x), i32(global_id.y)), i32(global_id.z), final_color);
}
