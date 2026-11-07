param(
    [ValidateSet('start', 'stop')][string]$Action = 'start',
    [switch]$Demo,
    [switch]$NoOpen,
    [ValidateRange(1024, 65535)][int]$Port = 6080
)
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$container = 'stremio-vista-preview'
$image = 'stremio-vista-preview:local'
$url = "http://localhost:$Port/vnc.html?autoconnect=1&resize=scale"
Get-Command docker -ErrorAction Stop | Out-Null
& docker info --format '{{.OSType}}' | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Start Docker Desktop with Linux containers, then retry.' }

$existing = & docker container ls --all --filter "name=^/$container`$" --format '{{.ID}}'
if ($existing) {
    $info = (& docker inspect $container | ConvertFrom-Json)[0]
    $owner = $info.Config.Labels.'stremio-vista.workspace'
    if ($LASTEXITCODE -ne 0 -or $owner -ne $repoRoot) { throw "Container $container belongs to a different workspace; stop it there first." }
    & docker stop --time 10 $container | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not stop the existing preview.' }
    & docker rm $container | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Could not remove the stopped preview container.' }
}
if ($Action -eq 'stop') { Write-Host 'Preview stopped. Saved profiles are retained.'; exit 0 }

$baseImage = & docker image ls --quiet webos-dev:local
if (-not $baseImage) { throw 'Run .\tools\dev.ps1 setup first to build the base development image.' }
$ErrorActionPreference = 'Continue'
& docker build --tag $image --file (Join-Path $repoRoot '.devcontainer/Dockerfile.preview') (Join-Path $repoRoot '.devcontainer')
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { throw 'Preview image build failed.' }

$profile = if ($Demo) { 'stremio-vista-preview-demo' } else { 'stremio-vista-preview-data' }
$demoFlag = if ($Demo) { '1' } else { '0' }
$phonePort = $null
foreach ($candidatePort in 8099..8108) {
    if ($candidatePort -eq $Port) { continue }
    $probe = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, $candidatePort)
    try {
        $probe.Server.ExclusiveAddressUse = $true
        $probe.Start()
        $phonePort = $candidatePort
        break
    } catch [System.Net.Sockets.SocketException] {
        # Try the next host port; a service on Windows is invisible inside Docker.
    } finally { $probe.Stop() }
}
if (-not $phonePort) { throw 'No free key-page port from 8099 through 8108. Free one of these local ports and retry.' }
& docker run --detach --init --name $container --shm-size 512m `
    --label "stremio-vista.workspace=$repoRoot" --publish "127.0.0.1:${Port}:6080" `
    --publish "127.0.0.1:${phonePort}:${phonePort}" `
    --mount "type=bind,source=$repoRoot,target=/workspace" `
    --mount 'type=volume,source=webos-dev-cache,target=/opt/origin-build' `
    --mount "type=volume,source=$profile,target=/data" `
    --env "SB_PREVIEW_DEMO=$demoFlag" --env "SB_PHONE_PORT=$phonePort" $image | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Could not start the preview. Check that port $Port is free." }

Write-Host 'Building and starting the native UI preview (first build takes longer)...'
$deadline = [DateTime]::UtcNow.AddMinutes(10)
while ([DateTime]::UtcNow -lt $deadline) {
    $status = & docker inspect $container --format '{{.State.Status}} {{.State.Health.Status}}'
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the preview container.' }
    if ($status -eq 'running healthy') {
        Write-Host "Stremio Vista preview: $url"
        Write-Host 'Mouse/arrows: navigate; Enter: select; Escape/Backspace: back. Video and audio are simulated.'
        if (-not $NoOpen) { Start-Process $url }
        exit 0
    }
    if ($status -notlike 'running *') {
        $ErrorActionPreference = 'Continue'
        & docker logs --tail 35 $container
        $ErrorActionPreference = 'Stop'
        throw 'The preview exited before its first frame. See the startup log above.'
    }
    Start-Sleep -Seconds 1
}
throw "Preview startup timed out. Inspect with: docker logs $container"
