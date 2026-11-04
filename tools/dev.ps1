param(
    [ValidateSet('setup', 'doctor', 'build', 'libraries', 'package', 'test', 'test-media', 'shell')]
    [string]$Command = 'doctor'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$image = 'webos-dev:local'
Get-Command docker -ErrorAction Stop | Out-Null
& docker info --format '{{.OSType}}' | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'Start Docker Desktop with Linux containers, then retry.' }

if ($Command -eq 'setup') {
    # Windows PowerShell 5 treats redirected native stderr as ErrorRecords.
    # Docker writes normal build progress there; use its exit code for success.
    $ErrorActionPreference = 'Continue'
    & docker build --tag $image --file (Join-Path $repoRoot '.devcontainer/Dockerfile') (Join-Path $repoRoot '.devcontainer')
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -ne 0) { throw 'Development image build failed.' }
    exit 0
}

$runArgs = @('run', '--rm', '--init', '--mount', "type=bind,source=$repoRoot,target=/workspace",
    '--mount', 'type=volume,source=webos-dev-cache,target=/opt/origin-build', '--workdir', '/workspace')
foreach ($name in @('APPID', 'TITLE', 'ENGINE_JOBS', 'FFMPEG_JOBS')) {
    $value = [Environment]::GetEnvironmentVariable($name)
    if ($value) { $runArgs += @('--env', "$name=$value") }
}
if ($Command -eq 'shell') { $runArgs += '-it' }
$runArgs += $image
switch ($Command) {
    'doctor' { $runArgs += @('bash', 'tools/dev-container.sh', 'doctor') }
    'build' { $runArgs += @('bash', 'tools/build.sh', 'webos') }
    'libraries' { $runArgs += @('bash', 'tools/dev-container.sh', 'libraries') }
    'package' { $runArgs += @('bash', 'tools/package.sh') }
    'test' { $runArgs += @('bash', 'tools/test-local.sh') }
    'test-media' { $runArgs += @('bash', 'tools/dev-container.sh', 'test-media') }
    'shell' { $runArgs += 'bash' }
}
$ErrorActionPreference = 'Continue'
& docker @runArgs
$ErrorActionPreference = 'Stop'
if ($LASTEXITCODE -ne 0) { throw "Development command '$Command' failed (exit $LASTEXITCODE)." }
