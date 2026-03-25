git submodule update --init --depth 1 vendor/dawn

git submodule update --init --recursive vendor/spdlog vendor/imgui vendor/assimp vendor/flecs vendor/tracy vendor/stb vendor/ImGuizmo vendor/JoltPhysics vendor/ozz-animation

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    if (-not (Get-Command scoop -ErrorAction SilentlyContinue)) {
        Write-Error "cmake is not installed and scoop was not found. Install scoop first: https://scoop.sh"
        exit 1
    }
    Write-Host "Installing cmake via scoop..."
    scoop install cmake python
}

