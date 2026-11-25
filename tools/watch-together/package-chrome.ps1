$ErrorActionPreference = 'Stop'

$repo = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$dist = New-Item -ItemType Directory -Path (Join-Path $repo 'dist') -Force
$zip = Join-Path $dist.FullName 'Vista-Watch-Together-Chrome-Setup.zip'
$tempRoot = (Get-Item -LiteralPath ([IO.Path]::GetTempPath())).FullName
$staging = New-Item -ItemType Directory -Path (Join-Path $tempRoot ("vista-watch-" + [guid]::NewGuid()))

try {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'extension') -Destination $staging.FullName -Recurse
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Install Vista Watch Together.cmd') -Destination $staging.FullName

    $setup = Join-Path $staging.FullName 'extension/setup.html'
    $html = [IO.File]::ReadAllText($setup)
    $relayField = 'id="relay" type="url"'
    if (!$html.Contains($relayField)) { throw 'Could not find the relay input in setup.html.' }
    $html = $html.Replace($relayField, $relayField + ' value="https://watch.notice.photos"')
    [IO.File]::WriteAllText($setup, $html)

    $contents = @(
        (Join-Path $staging.FullName 'extension')
        (Join-Path $staging.FullName 'Install Vista Watch Together.cmd')
    )
    Compress-Archive -LiteralPath $contents -DestinationPath $zip -Force
    Get-Item -LiteralPath $zip
} finally {
    $resolved = Get-Item -LiteralPath $staging.FullName
    if ($resolved.Parent.FullName -eq $tempRoot -and $resolved.Name -match '^vista-watch-[0-9a-f-]{36}$') {
        Remove-Item -LiteralPath $resolved.FullName -Recurse
    }
}
