# Init Dawn WITHOUT --recursive (its deps are fetched by DAWN_FETCH_DEPENDENCIES in CMake)
git submodule update --init --depth 1 vendor/dawn

# Init everything else WITH --recursive
git submodule update --init --recursive vendor/spdlog vendor/imgui vendor/assimp vendor/flecs vendor/tracy vendor/stb vendor/ImGuizmo vendor/JoltPhysics vendor/ozz-animation

# Install cmake via scoop if not available
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    if (-not (Get-Command scoop -ErrorAction SilentlyContinue)) {
        Write-Error "cmake is not installed and scoop was not found. Install scoop first: https://scoop.sh"
        exit 1
    }
    Write-Host "Installing cmake via scoop..."
    scoop install cmake
}

# Generate Visual Studio solution
Write-Host "Generating Visual Studio solution..."
cmake --preset windows-vs2022-debug
